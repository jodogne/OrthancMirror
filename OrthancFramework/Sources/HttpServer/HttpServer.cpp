/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


// http://en.highscore.de/cpp/boost/stringhandling.html

#include "../PrecompiledHeaders.h"
#include "HttpServer.h"

#include "../ChunkedBuffer.h"
#include "../FileBuffer.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../TemporaryFile.h"
#include "HttpToolbox.h"
#include "IHttpHandler.h"
#include "MultipartStreamReader.h"
#include "StringHttpOutput.h"

#if ORTHANC_ENABLE_PUGIXML == 1
#  include "IWebDavBucket.h"
#endif

#if ORTHANC_ENABLE_MONGOOSE == 1
#  include <mongoose.h>

#elif ORTHANC_ENABLE_CIVETWEB == 1
#  include <civetweb.h>
#  define MONGOOSE_USE_CALLBACKS 1
#  if !defined(CIVETWEB_HAS_DISABLE_KEEP_ALIVE)
#    error Macro CIVETWEB_HAS_DISABLE_KEEP_ALIVE must be defined
#  endif
#  if !defined(CIVETWEB_HAS_WEBDAV_WRITING)
#    error Macro CIVETWEB_HAS_WEBDAV_WRITING must be defined
#  endif
#else
#  error "Either Mongoose or Civetweb must be enabled to compile this file"
#endif

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <stdio.h>
#include <string.h>

#if !defined(ORTHANC_ENABLE_SSL)
#  error The macro ORTHANC_ENABLE_SSL must be defined
#endif

#if ORTHANC_ENABLE_SSL == 1
#  include <openssl/opensslv.h>
#  include <openssl/err.h>
#endif

#define ORTHANC_REALM "Orthanc Secure Area"


namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    class MongooseOutputStream : public IHttpOutputStream
    {
    private:
      struct mg_connection* connection_;

    public:
      explicit MongooseOutputStream(struct mg_connection* connection) :
        connection_(connection)
      {
      }

      virtual void Send(bool isHeader,
                        const void* buffer,
                        size_t length) ORTHANC_OVERRIDE
      {
        if (length > 0)
        {
          int status = mg_write(connection_, buffer, length);
          if (status != static_cast<int>(length))
          {
            // status == 0 when the connection has been closed, -1 on error
            throw OrthancException(ErrorCode_NetworkProtocol);
          }
        }
      }

      virtual void OnHttpStatusReceived(HttpStatus status) ORTHANC_OVERRIDE
      {
        // Ignore this
      }

      virtual void DisableKeepAlive() ORTHANC_OVERRIDE
      {
#if ORTHANC_ENABLE_MONGOOSE == 1
        throw OrthancException(ErrorCode_NotImplemented,
                               "Only available if using CivetWeb");

#elif ORTHANC_ENABLE_CIVETWEB == 1
#  if CIVETWEB_HAS_DISABLE_KEEP_ALIVE == 1
        mg_disable_keep_alive(connection_);
#  else
#    if defined(__GNUC__) || defined(__clang__)
#       warning The function "mg_disable_keep_alive()" is not available, DICOMweb might run slowly
#    endif
        throw OrthancException(ErrorCode_NotImplemented,
                               "Only available if using a patched version of CivetWeb");
#  endif

#else
#  error Please support your embedded Web server here
#endif
      }
    };


    enum PostDataStatus
    {
      PostDataStatus_Success,
      PostDataStatus_NoLength,
      PostDataStatus_Pending,
      PostDataStatus_Failure
    };
  }


  namespace
  {
    class ChunkedFile : public ChunkedBuffer
    {
    private:
      std::string filename_;

    public:
      explicit ChunkedFile(const std::string& filename) :
        filename_(filename)
      {
      }

      const std::string& GetFilename() const
      {
        return filename_;
      }
    };
  }



  class HttpServer::ChunkStore : public boost::noncopyable
  {
  private:
    typedef std::list<ChunkedFile*>  Content;
    Content  content_;
    unsigned int numPlaces_;

    boost::mutex mutex_;
    std::set<std::string> discardedFiles_;

    void Clear()
    {
      for (Content::iterator it = content_.begin();
           it != content_.end(); ++it)
      {
        delete *it;
      }
    }

    Content::iterator Find(const std::string& filename)
    {
      for (Content::iterator it = content_.begin();
           it != content_.end(); ++it)
      {
        if ((*it)->GetFilename() == filename)
        {
          return it;
        }
      }

      return content_.end();
    }

    void Remove(const std::string& filename)
    {
      Content::iterator it = Find(filename);
      if (it != content_.end())
      {
        delete *it;
        content_.erase(it);
      }
    }

  public:
    ChunkStore()
    {
      numPlaces_ = 10;
    }

    ~ChunkStore()
    {
      Clear();
    }

    PostDataStatus Store(std::string& completed,
                         const void* chunkData,
                         size_t chunkSize,
                         const std::string& filename,
                         size_t filesize)
    {
      boost::mutex::scoped_lock lock(mutex_);

      std::set<std::string>::iterator wasDiscarded = discardedFiles_.find(filename);
      if (wasDiscarded != discardedFiles_.end())
      {
        discardedFiles_.erase(wasDiscarded);
        return PostDataStatus_Failure;
      }

      ChunkedFile* f;
      Content::iterator it = Find(filename);
      if (it == content_.end())
      {
        f = new ChunkedFile(filename);

        // Make some room
        if (content_.size() >= numPlaces_)
        {
          discardedFiles_.insert(content_.front()->GetFilename());
          delete content_.front();
          content_.pop_front();
        }

        content_.push_back(f);
      }
      else
      {
        f = *it;
      }

      f->AddChunk(chunkData, chunkSize);

      if (f->GetNumBytes() > filesize)
      {
        Remove(filename);
      }
      else if (f->GetNumBytes() == filesize)
      {
        f->Flatten(completed);
        Remove(filename);
        return PostDataStatus_Success;
      }

      return PostDataStatus_Pending;
    }

    /*void Print() 
      {
      boost::mutex::scoped_lock lock(mutex_);

      printf("ChunkStore status:\n");
      for (Content::const_iterator i = content_.begin();
      i != content_.end(); i++)
      {
      printf("  [%s]: %d\n", (*i)->GetFilename().c_str(), (*i)->GetNumBytes());
      }
      printf("-----\n");
      }*/
  };


  struct HttpServer::PImpl
  {
    struct mg_context *context_;
    ChunkStore chunkStore_;

    PImpl() :
      context_(NULL)
    {
    }
  };


  class HttpServer::MultipartFormDataHandler : public MultipartStreamReader::IHandler
  {
  private:
    IHttpHandler&         handler_;
    ChunkStore&           chunkStore_;
    const std::string&    remoteIp_;
    const std::string&    username_;
    const UriComponents&  uri_;
    bool                  isJQueryUploadChunk_;
    std::string           jqueryUploadFileName_;
    size_t                jqueryUploadFileSize_;

    void HandleInternal(const MultipartStreamReader::HttpHeaders& headers,
                        const void* part,
                        size_t size)
    {
      StringHttpOutput stringOutput;
      HttpOutput fakeOutput(stringOutput, false);
      HttpToolbox::GetArguments getArguments;
      
      if (!handler_.Handle(fakeOutput, RequestOrigin_RestApi, remoteIp_.c_str(), username_.c_str(), 
                           HttpMethod_Post, uri_, headers, getArguments, part, size))
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }
      
  public:
    MultipartFormDataHandler(IHttpHandler& handler,
                             ChunkStore& chunkStore,
                             const std::string& remoteIp,
                             const std::string& username,
                             const UriComponents& uri,
                             const MultipartStreamReader::HttpHeaders& headers) :
      handler_(handler),
      chunkStore_(chunkStore),
      remoteIp_(remoteIp),
      username_(username),
      uri_(uri),
      isJQueryUploadChunk_(false),
      jqueryUploadFileSize_(0)  // Dummy initialization
    {
      typedef HttpToolbox::Arguments::const_iterator Iterator;
      
      Iterator requestedWith = headers.find("x-requested-with");
      if (requestedWith != headers.end() &&
          requestedWith->second != "XMLHttpRequest")
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "HTTP header \"X-Requested-With\" should be "
                               "\"XMLHttpRequest\" in multipart uploads");
      }

      Iterator fileName = headers.find("x-file-name");
      Iterator fileSize = headers.find("x-file-size");
      if (fileName != headers.end() ||
          fileSize != headers.end())
      {
        if (fileName == headers.end())
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "HTTP header \"X-File-Name\" is missing");
        }

        if (fileSize == headers.end())
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "HTTP header \"X-File-Size\" is missing");
        }

        isJQueryUploadChunk_ = true;
        jqueryUploadFileName_ = fileName->second;

        try
        {
          int64_t s = boost::lexical_cast<int64_t>(fileSize->second);
          if (s < 0)
          {
            throw OrthancException(ErrorCode_NetworkProtocol, "HTTP header \"X-File-Size\" has negative value");
          }
          else
          {
            jqueryUploadFileSize_ = static_cast<size_t>(s);
            if (static_cast<int64_t>(jqueryUploadFileSize_) != s)
            {
              throw OrthancException(ErrorCode_NotEnoughMemory);
            }
          }
        }
        catch (boost::bad_lexical_cast& e)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "HTTP header \"X-File-Size\" is not an integer");
        }
      }
    }
      
    virtual void HandlePart(const MultipartStreamReader::HttpHeaders& headers,
                            const void* part,
                            size_t size) ORTHANC_OVERRIDE
    {
      if (isJQueryUploadChunk_)
      {
        std::string completedFile;

        PostDataStatus status = chunkStore_.Store(completedFile, part, size, jqueryUploadFileName_, jqueryUploadFileSize_);

        switch (status)
        {
          case PostDataStatus_Failure:
            throw OrthancException(ErrorCode_NetworkProtocol, "Error in the multipart form upload");

          case PostDataStatus_Success:
            assert(completedFile.size() == jqueryUploadFileSize_);
            HandleInternal(headers, completedFile.empty() ? NULL : completedFile.c_str(), completedFile.size());
            break;

          case PostDataStatus_Pending:
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        HandleInternal(headers, part, size);
      }
    }
  };


  void HttpServer::ProcessMultipartFormData(const std::string& remoteIp,
                                            const std::string& username,
                                            const UriComponents& uri,
                                            const std::map<std::string, std::string>& headers,
                                            const std::string& body,
                                            const std::string& boundary)
  {
    MultipartFormDataHandler handler(GetHandler(), pimpl_->chunkStore_, remoteIp, username, uri, headers);
          
    MultipartStreamReader reader(boundary);
    reader.SetHandler(handler);
    reader.AddChunk(body);        
    reader.CloseStream();
  }
  

  static PostDataStatus ReadBodyWithContentLength(std::string& body,
                                                  struct mg_connection *connection,
                                                  const std::string& contentLength)
  {
    size_t length;
    try
    {
      int64_t tmp = boost::lexical_cast<int64_t>(contentLength);
      if (tmp < 0)
      {
        return PostDataStatus_NoLength;
      }

      length = static_cast<size_t>(tmp);
    }
    catch (boost::bad_lexical_cast&)
    {
      return PostDataStatus_NoLength;
    }

    body.resize(length);

    size_t pos = 0;
    while (length > 0)
    {
      int r = mg_read(connection, &body[pos], length);
      if (r <= 0)
      {
        return PostDataStatus_Failure;
      }

      assert(static_cast<size_t>(r) <= length);
      length -= r;
      pos += r;
    }

    return PostDataStatus_Success;
  }
                                                  

  static PostDataStatus ReadBodyWithoutContentLength(std::string& body,
                                                     struct mg_connection *connection)
  {
    // Store the individual chunks in a temporary file, then read it
    // back into the memory buffer "body"
    FileBuffer buffer;

    std::string tmp(1024 * 1024, 0);
      
    for (;;)
    {
      int r = mg_read(connection, &tmp[0], tmp.size());
      if (r < 0)
      {
        return PostDataStatus_Failure;
      }
      else if (r == 0)
      {
        break;
      }
      else
      {
        buffer.Append(tmp.c_str(), r);
      }
    }

    buffer.Read(body);

    return PostDataStatus_Success;
  }
                                                  

  static PostDataStatus ReadBodyToString(std::string& body,
                                         struct mg_connection *connection,
                                         const HttpToolbox::Arguments& headers)
  {
    HttpToolbox::Arguments::const_iterator contentLength = headers.find("content-length");

    if (contentLength != headers.end())
    {
      // "Content-Length" is available
      return ReadBodyWithContentLength(body, connection, contentLength->second);
    }
    else
    {
      // No Content-Length
      return ReadBodyWithoutContentLength(body, connection);
    }
  }


  static PostDataStatus ReadBodyToStream(IHttpHandler::IChunkedRequestReader& stream,
                                         struct mg_connection *connection,
                                         const HttpToolbox::Arguments& headers)
  {
    HttpToolbox::Arguments::const_iterator contentLength = headers.find("content-length");

    if (contentLength != headers.end())
    {
      // "Content-Length" is available
      std::string body;
      PostDataStatus status = ReadBodyWithContentLength(body, connection, contentLength->second);

      if (status == PostDataStatus_Success &&
          !body.empty())
      {
        stream.AddBodyChunk(body.c_str(), body.size());
      }

      return status;
    }
    else
    {
      // No Content-Length: This is a chunked transfer. Stream the HTTP connection.
      std::string tmp(1024 * 1024, 0);
      
      for (;;)
      {
        int r = mg_read(connection, &tmp[0], tmp.size());
        if (r < 0)
        {
          return PostDataStatus_Failure;
        }
        else if (r == 0)
        {
          break;
        }
        else
        {
          stream.AddBodyChunk(tmp.c_str(), r);
        }
      }

      return PostDataStatus_Success;
    }
  }

  
  enum AccessMode
  {
    AccessMode_Forbidden,
    AccessMode_AuthorizationToken,
    AccessMode_RegisteredUser
  };
  

  static AccessMode IsAccessGranted(const HttpServer& that,
                                    const HttpToolbox::Arguments& headers)
  {
    static const std::string BASIC = "Basic ";
    static const std::string BEARER = "Bearer ";

    HttpToolbox::Arguments::const_iterator auth = headers.find("authorization");
    if (auth != headers.end())
    {
      std::string s = auth->second;
      if (boost::starts_with(s, BASIC))
      {
        std::string b64 = s.substr(BASIC.length());
        if (that.IsValidBasicHttpAuthentication(b64))
        {
          return AccessMode_RegisteredUser;
        }
      }
      else if (boost::starts_with(s, BEARER) &&
               that.GetIncomingHttpRequestFilter() != NULL)
      {
        // New in Orthanc 1.8.1
        std::string token = s.substr(BEARER.length());
        if (that.GetIncomingHttpRequestFilter()->IsValidBearerToken(token))
        {
          return AccessMode_AuthorizationToken;
        }
      }
    }

    return AccessMode_Forbidden;
  }


  static std::string GetAuthenticatedUsername(const HttpToolbox::Arguments& headers)
  {
    HttpToolbox::Arguments::const_iterator auth = headers.find("authorization");

    if (auth == headers.end())
    {
      return "";
    }

    std::string s = auth->second;
    if (s.size() <= 6 ||
        s.substr(0, 6) != "Basic ")
    {
      return "";
    }

    std::string b64 = s.substr(6);
    std::string decoded;
    Toolbox::DecodeBase64(decoded, b64);
    size_t semicolons = decoded.find(':');

    if (semicolons == std::string::npos)
    {
      // Bad-formatted request
      return "";
    }
    else
    {
      return decoded.substr(0, semicolons);
    }
  }


  static bool ExtractMethod(HttpMethod& method,
                            const struct mg_request_info *request,
                            const HttpToolbox::Arguments& headers,
                            const HttpToolbox::GetArguments& argumentsGET)
  {
    std::string overriden;

    // Check whether some PUT/DELETE faking is done

    // 1. Faking with Google's approach
    HttpToolbox::Arguments::const_iterator methodOverride =
      headers.find("x-http-method-override");

    if (methodOverride != headers.end())
    {
      overriden = methodOverride->second;
    }
    else if (!strcmp(request->request_method, "GET"))
    {
      // 2. Faking with Ruby on Rail's approach
      // GET /my/resource?_method=delete <=> DELETE /my/resource
      for (size_t i = 0; i < argumentsGET.size(); i++)
      {
        if (argumentsGET[i].first == "_method")
        {
          overriden = argumentsGET[i].second;
          break;
        }
      }
    }

    if (overriden.size() > 0)
    {
      // A faking has been done within this request
      Toolbox::ToUpperCase(overriden);

      CLOG(INFO, HTTP) << "HTTP method faking has been detected for " << overriden;

      if (overriden == "PUT")
      {
        method = HttpMethod_Put;
        return true;
      }
      else if (overriden == "DELETE")
      {
        method = HttpMethod_Delete;
        return true;
      }
      else
      {
        return false;
      }
    }

    // No PUT/DELETE faking was present
    if (!strcmp(request->request_method, "GET"))
    {
      method = HttpMethod_Get;
    }
    else if (!strcmp(request->request_method, "POST"))
    {
      method = HttpMethod_Post;
    }
    else if (!strcmp(request->request_method, "DELETE"))
    {
      method = HttpMethod_Delete;
    }
    else if (!strcmp(request->request_method, "PUT"))
    {
      method = HttpMethod_Put;
    }
    else
    {
      return false;
    }    

    return true;
  }


  static void ConfigureHttpCompression(HttpOutput& output,
                                       const HttpToolbox::Arguments& headers)
  {
    // Look if the client wishes HTTP compression
    // https://en.wikipedia.org/wiki/HTTP_compression
    HttpToolbox::Arguments::const_iterator it = headers.find("accept-encoding");
    if (it != headers.end())
    {
      std::vector<std::string> encodings;
      Toolbox::TokenizeString(encodings, it->second, ',');

      for (size_t i = 0; i < encodings.size(); i++)
      {
        std::string s = Toolbox::StripSpaces(encodings[i]);

        if (s == "deflate")
        {
          output.SetDeflateAllowed(true);
        }
        else if (s == "gzip")
        {
          output.SetGzipAllowed(true);
        }
      }
    }
  }


#if ORTHANC_ENABLE_PUGIXML == 1

#  if CIVETWEB_HAS_WEBDAV_WRITING == 0
  static void AnswerWebDavReadOnly(HttpOutput& output,
                                   const std::string& uri)
  {
    CLOG(ERROR, HTTP) << "Orthanc was compiled without support for read-write access to WebDAV: " << uri;
    output.SendStatus(HttpStatus_403_Forbidden);
  }    
#  endif
  
  static bool HandleWebDav(HttpOutput& output,
                           const HttpServer::WebDavBuckets& buckets,
                           const std::string& method,
                           const HttpToolbox::Arguments& headers,
                           const std::string& uri,
                           struct mg_connection *connection /* to read the PUT body if need be */)
  {
    if (buckets.empty())
    {
      return false;  // Speed up things if WebDAV is not used
    }
    
    /**
     * The "buckets" maps an URI relative to the root of the
     * bucket, to the content of the bucket. The root URI does *not*
     * contain a trailing slash.
     **/
    
    if (method == "OPTIONS")
    {
      // Remove the trailing slash, if any (necessary for davfs2)
      std::string s = uri;
      if (!s.empty() &&
          s[s.size() - 1] == '/')
      {
        s.resize(s.size() - 1);
      }
      
      HttpServer::WebDavBuckets::const_iterator bucket = buckets.find(s);
      if (bucket == buckets.end())
      {
        return false;
      }
      else
      {
        output.AddHeader("DAV", "1,2");  // Necessary for Windows XP

#if CIVETWEB_HAS_WEBDAV_WRITING == 1
        output.AddHeader("Allow", "GET, PUT, DELETE, OPTIONS, PROPFIND, HEAD, LOCK, UNLOCK, PROPPATCH, MKCOL");
#else
        output.AddHeader("Allow", "GET, PUT, DELETE, OPTIONS, PROPFIND, HEAD");
#endif

        output.SendStatus(HttpStatus_200_Ok);
        return true;
      }
    }
    else if (method == "GET" ||
             method == "PROPFIND" ||
             method == "PROPPATCH" ||
             method == "PUT" ||
             method == "DELETE" ||
             method == "HEAD" ||
             method == "LOCK" ||
             method == "UNLOCK" ||
             method == "MKCOL")
    {
      // Locate the WebDAV bucket of interest, if any
      for (HttpServer::WebDavBuckets::const_iterator bucket = buckets.begin();
           bucket != buckets.end(); ++bucket)
      {
        assert(!bucket->first.empty() &&
               bucket->first[bucket->first.size() - 1] != '/' &&
               bucket->second != NULL);
        
        if (uri == bucket->first ||
            boost::starts_with(uri, bucket->first + "/"))
        {
          std::string s = uri.substr(bucket->first.size());
          if (s.empty())
          {
            s = "/";
          }

          std::vector<std::string> path;
          Toolbox::SplitUriComponents(path, s);


          /**
           * WebDAV - PROPFIND
           **/
          
          if (method == "PROPFIND")
          {
            HttpToolbox::Arguments::const_iterator i = headers.find("depth");
            if (i == headers.end())
            {
              throw OrthancException(ErrorCode_NetworkProtocol, "WebDAV PROPFIND without depth");
            }

            int depth = boost::lexical_cast<int>(i->second);
            if (depth != 0 &&
                depth != 1)
            {
              throw OrthancException(
                ErrorCode_NetworkProtocol,
                "WebDAV PROPFIND at unsupported depth (can only be 0 or 1): " + i->second);
            }
      
            std::string answer;
          
            MimeType mime;
            std::string content;
            boost::posix_time::ptime modificationTime = boost::posix_time::second_clock::universal_time();

            if (bucket->second->IsExistingFolder(path))
            {
              if (depth == 0)
              {
                IWebDavBucket::Collection c;
                c.Format(answer, uri);
              }
              else if (depth == 1)
              {
                IWebDavBucket::Collection c;
              
                if (!bucket->second->ListCollection(c, path))
                {
                  output.SendStatus(HttpStatus_404_NotFound);
                  return true;
                }
                
                c.Format(answer, uri);
              }
              else
              {
                throw OrthancException(ErrorCode_InternalError);
              }
            }
            else if (!path.empty() &&
                     bucket->second->GetFileContent(mime, content, modificationTime, path))
            {
              if (depth == 0 ||
                  depth == 1)
              {
                std::unique_ptr<IWebDavBucket::File> f(new IWebDavBucket::File(path.back()));
                f->SetContentLength(content.size());
                f->SetModificationTime(modificationTime);
                f->SetMimeType(mime);

                IWebDavBucket::Collection c;
                c.AddResource(f.release());

                std::vector<std::string> p;
                Toolbox::SplitUriComponents(p, uri);
                if (p.empty())
                {
                  throw OrthancException(ErrorCode_InternalError);
                }

                p.resize(p.size() - 1);
                c.Format(answer, Toolbox::FlattenUri(p));
              }
              else
              {
                throw OrthancException(ErrorCode_InternalError);
              }
            }
            else
            {
              output.SendStatus(HttpStatus_404_NotFound);
              return true;
            }

            output.AddHeader("Content-Type", "application/xml; charset=UTF-8");
            output.SendStatus(HttpStatus_207_MultiStatus, answer);
            return true;
          }
          

          /**
           * WebDAV - GET and HEAD
           **/
          
          else if (method == "GET" ||
                   method == "HEAD")
          {
            MimeType mime;
            std::string content;
            boost::posix_time::ptime modificationTime;
            
            if (bucket->second->GetFileContent(mime, content, modificationTime, path))
            {
              output.AddHeader("Content-Type", EnumerationToString(mime));

              // "Last-Modified" is necessary on Windows XP. The "Z"
              // suffix is mandatory on Windows >= 7.
              output.AddHeader("Last-Modified", boost::posix_time::to_iso_extended_string(modificationTime) + "Z");

              if (method == "GET")
              {
                output.Answer(content);
              }
              else
              {
                output.SendStatus(HttpStatus_200_Ok);
              }
            }
            else
            {
              output.SendStatus(HttpStatus_404_NotFound);
            }

            return true;
          }

          
          /**
           * WebDAV - PUT
           **/
          
          else if (method == "PUT")
          {
#if CIVETWEB_HAS_WEBDAV_WRITING == 1           
            std::string body;
            if (ReadBodyToString(body, connection, headers) == PostDataStatus_Success)
            {
              if (bucket->second->StoreFile(body, path))
              {
                //output.SendStatus(HttpStatus_200_Ok);
                output.SendStatus(HttpStatus_201_Created);
              }
              else
              {
                output.SendStatus(HttpStatus_403_Forbidden);
              }
            }
            else
            {
              CLOG(ERROR, HTTP) << "Cannot read the content of a file to be stored in WebDAV";
              output.SendStatus(HttpStatus_400_BadRequest);
            }
#else
            AnswerWebDavReadOnly(output, uri);
#endif

            return true;
          }
          

          /**
           * WebDAV - DELETE
           **/
          
          else if (method == "DELETE")
          {
            if (bucket->second->DeleteItem(path))
            {
              output.SendStatus(HttpStatus_204_NoContent);
            }
            else
            {
              output.SendStatus(HttpStatus_403_Forbidden);
            }
            return true;
          }
          

          /**
           * WebDAV - MKCOL
           **/
          
          else if (method == "MKCOL")
          {
#if CIVETWEB_HAS_WEBDAV_WRITING == 1           
            if (bucket->second->CreateFolder(path))
            {
              //output.SendStatus(HttpStatus_200_Ok);
              output.SendStatus(HttpStatus_201_Created);
            }
            else
            {
              output.SendStatus(HttpStatus_403_Forbidden);
            }
#else
            AnswerWebDavReadOnly(output, uri);
#endif

            return true;
          }
          

          /**
           * WebDAV - Faking PROPPATCH, LOCK and UNLOCK
           **/
          
          else if (method == "PROPPATCH")
          {
#if CIVETWEB_HAS_WEBDAV_WRITING == 1           
            IWebDavBucket::AnswerFakedProppatch(output, uri);
#else
            AnswerWebDavReadOnly(output, uri);
#endif
            return true;
          }
          else if (method == "LOCK")
          {
#if CIVETWEB_HAS_WEBDAV_WRITING == 1           
            IWebDavBucket::AnswerFakedLock(output, uri);
#else
            AnswerWebDavReadOnly(output, uri);
#endif
            return true;
          }
          else if (method == "UNLOCK")
          {
#if CIVETWEB_HAS_WEBDAV_WRITING == 1           
            IWebDavBucket::AnswerFakedUnlock(output);
#else
            AnswerWebDavReadOnly(output, uri);
#endif
            return true;
          }
          else
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }
      }
      
      return false;
    }
    else
    {
      /**
       * WebDAV - Unapplicable method (such as POST and DELETE)
       **/
          
      return false;
    }
  } 
#endif /* ORTHANC_ENABLE_PUGIXML == 1 */

  
  static void InternalCallback(HttpOutput& output /* out */,
                               HttpMethod& method /* out */,
                               HttpServer& server,
                               struct mg_connection *connection,
                               const struct mg_request_info *request)
  {
    bool localhost;

#if ORTHANC_ENABLE_MONGOOSE == 1
    static const long LOCALHOST = (127ll << 24) + 1ll;
    localhost = (request->remote_ip == LOCALHOST);
#elif ORTHANC_ENABLE_CIVETWEB == 1
    // The "remote_ip" field of "struct mg_request_info" is tagged as
    // deprecated in Civetweb, using "remote_addr" instead.
    localhost = (std::string(request->remote_addr) == "127.0.0.1");
#else
#  error
#endif
    
    // Check remote calls
    if (!server.IsRemoteAccessAllowed() &&
        !localhost)
    {
      output.SendUnauthorized(server.GetRealm());   // 401 error
      return;
    }


    // Extract the HTTP headers
    HttpToolbox::Arguments headers;
    for (int i = 0; i < request->num_headers; i++)
    {
      std::string name = request->http_headers[i].name;
      std::string value = request->http_headers[i].value;

      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      headers.insert(std::make_pair(name, value));
      CLOG(TRACE, HTTP) << "HTTP header: [" << name << "]: [" << value << "]";
    }

    if (server.IsHttpCompressionEnabled())
    {
      ConfigureHttpCompression(output, headers);
    }


    // Extract the GET arguments
    HttpToolbox::GetArguments argumentsGET;
    if (!strcmp(request->request_method, "GET"))
    {
      HttpToolbox::ParseGetArguments(argumentsGET, request->query_string);
    }

    
    AccessMode accessMode = IsAccessGranted(server, headers);

    // Authenticate this connection
    if (server.IsAuthenticationEnabled() && 
        accessMode == AccessMode_Forbidden)
    {
      output.SendUnauthorized(server.GetRealm());   // 401 error
      return;
    }

    
#if ORTHANC_ENABLE_MONGOOSE == 1
    // Apply the filter, if it is installed
    char remoteIp[24];
    sprintf(remoteIp, "%d.%d.%d.%d", 
            reinterpret_cast<const uint8_t*>(&request->remote_ip) [3], 
            reinterpret_cast<const uint8_t*>(&request->remote_ip) [2], 
            reinterpret_cast<const uint8_t*>(&request->remote_ip) [1], 
            reinterpret_cast<const uint8_t*>(&request->remote_ip) [0]);

    const char* requestUri = request->uri;
      
#elif ORTHANC_ENABLE_CIVETWEB == 1
    const char* remoteIp = request->remote_addr;
    const char* requestUri = request->local_uri;
#else
#  error
#endif

    if (requestUri == NULL)
    {
      requestUri = "";
    }
      
    // Decompose the URI into its components
    UriComponents uri;
    try
    {
      Toolbox::SplitUriComponents(uri, requestUri);
    }
    catch (OrthancException&)
    {
      output.SendStatus(HttpStatus_400_BadRequest);
      return;
    }


    // Compute the HTTP method, taking method faking into consideration
    method = HttpMethod_Get;

#if ORTHANC_ENABLE_PUGIXML == 1
    bool isWebDav = false;
#endif
    
    HttpMethod filterMethod;

    
    if (ExtractMethod(method, request, headers, argumentsGET))
    {
      CLOG(INFO, HTTP) << EnumerationToString(method) << " " << Toolbox::FlattenUri(uri);
      filterMethod = method;
    }
#if ORTHANC_ENABLE_PUGIXML == 1
    else if (!strcmp(request->request_method, "OPTIONS") ||
             !strcmp(request->request_method, "PROPFIND") ||
             !strcmp(request->request_method, "HEAD"))
    {
      CLOG(INFO, HTTP) << "Incoming read-only WebDAV request: "
                       << request->request_method << " " << requestUri;
      filterMethod = HttpMethod_Get;
      isWebDav = true;
    }
    else if (!strcmp(request->request_method, "PROPPATCH") ||
             !strcmp(request->request_method, "LOCK") ||
             !strcmp(request->request_method, "UNLOCK") ||
             !strcmp(request->request_method, "MKCOL"))
    {
      CLOG(INFO, HTTP) << "Incoming read-write WebDAV request: "
                       << request->request_method << " " << requestUri;
      filterMethod = HttpMethod_Put;
      isWebDav = true;
    }
#endif /* ORTHANC_ENABLE_PUGIXML == 1 */
    else
    {
      CLOG(INFO, HTTP) << "Unknown HTTP method: " << request->request_method;
      output.SendStatus(HttpStatus_400_BadRequest);
      return;
    }
    

    const std::string username = GetAuthenticatedUsername(headers);

    if (accessMode != AccessMode_AuthorizationToken)
    {
      // Check that this access is granted by the user's authorization
      // filter. In the case of an authorization bearer token, grant
      // full access to the API.

      assert(accessMode == AccessMode_Forbidden ||  // Could be the case if "!server.IsAuthenticationEnabled()"
             accessMode == AccessMode_RegisteredUser);
      
      IIncomingHttpRequestFilter *filter = server.GetIncomingHttpRequestFilter();
      if (filter != NULL &&
          !filter->IsAllowed(filterMethod, requestUri, remoteIp,
                             username.c_str(), headers, argumentsGET))
      {
        output.SendStatus(HttpStatus_403_Forbidden);
        return;
      }
    }


#if ORTHANC_ENABLE_PUGIXML == 1
    if (HandleWebDav(output, server.GetWebDavBuckets(), request->request_method,
                     headers, requestUri, connection))
    {
      return;
    }
    else if (isWebDav)
    {
      CLOG(INFO, HTTP) << "No WebDAV bucket is registered against URI: "
                       << request->request_method << " " << requestUri;
      output.SendStatus(HttpStatus_404_NotFound);
      return;
    }
#endif


    bool found = false;

    // Extract the body of the request for PUT and POST, or process
    // the body as a stream

    std::string body;
    if (method == HttpMethod_Post ||
        method == HttpMethod_Put)
    {
      PostDataStatus status;

      bool isMultipartForm = false;

      std::string type, subType, boundary;
      HttpToolbox::Arguments::const_iterator ct = headers.find("content-type");
      if (method == HttpMethod_Post &&
          ct != headers.end() &&
          MultipartStreamReader::ParseMultipartContentType(type, subType, boundary, ct->second) &&
          type == "multipart/form-data")
      {
        /** 
         * The user uses the "upload" form of Orthanc Explorer, for
         * file uploads through a HTML form.
         **/
        isMultipartForm = true;

        status = ReadBodyToString(body, connection, headers);
        if (status == PostDataStatus_Success)
        {
          server.ProcessMultipartFormData(remoteIp, username, uri, headers, body, boundary);
          output.SendStatus(HttpStatus_200_Ok);
          return;
        }
      }

      if (!isMultipartForm)
      {
        std::unique_ptr<IHttpHandler::IChunkedRequestReader> stream;

        if (server.HasHandler())
        {
          found = server.GetHandler().CreateChunkedRequestReader
            (stream, RequestOrigin_RestApi, remoteIp, username.c_str(), method, uri, headers);
        }
        
        if (found)
        {
          if (stream.get() == NULL)
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          status = ReadBodyToStream(*stream, connection, headers);

          if (status == PostDataStatus_Success)
          {
            stream->Execute(output);
          }
        }
        else
        {
          status = ReadBodyToString(body, connection, headers);
        }
      }

      switch (status)
      {
        case PostDataStatus_NoLength:
          output.SendStatus(HttpStatus_411_LengthRequired);
          return;

        case PostDataStatus_Failure:
          output.SendStatus(HttpStatus_400_BadRequest);
          return;

        case PostDataStatus_Pending:
          output.AnswerEmpty();
          return;

        case PostDataStatus_Success:
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    if (!found && 
        server.HasHandler())
    {
      found = server.GetHandler().Handle(output, RequestOrigin_RestApi, remoteIp, username.c_str(), 
                                         method, uri, headers, argumentsGET, body.c_str(), body.size());
    }

    if (!found)
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  static void ProtectedCallback(struct mg_connection *connection,
                                const struct mg_request_info *request)
  {
    try
    {
#if ORTHANC_ENABLE_MONGOOSE == 1
      void *that = request->user_data;
      const char* requestUri = request->uri;
#elif ORTHANC_ENABLE_CIVETWEB == 1
      // https://github.com/civetweb/civetweb/issues/409
      void *that = mg_get_user_data(mg_get_context(connection));
      const char* requestUri = request->local_uri;
#else
#  error
#endif

      if (requestUri == NULL)
      {
        requestUri = "";
      }
      
      HttpServer* server = reinterpret_cast<HttpServer*>(that);

      if (server == NULL)
      {
        MongooseOutputStream stream(connection);
        HttpOutput output(stream, false /* assume no keep-alive */);
        output.SendStatus(HttpStatus_500_InternalServerError);
        return;
      }

      MongooseOutputStream stream(connection);
      HttpOutput output(stream, server->IsKeepAliveEnabled());
      HttpMethod method = HttpMethod_Get;

      try
      {
        try
        {
          InternalCallback(output, method, *server, connection, request);
        }
        catch (OrthancException&)
        {
          throw;  // Pass the exception to the main handler below
        }
        // Now convert native exceptions as OrthancException
        catch (boost::bad_lexical_cast&)
        {
          throw OrthancException(ErrorCode_BadParameterType,
                                 "Syntax error in some user-supplied data");
        }
        catch (boost::filesystem::filesystem_error& e)
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Error while accessing the filesystem: " + e.path1().string());
        }
        catch (std::runtime_error&)
        {
          throw OrthancException(ErrorCode_BadRequest,
                                 "Presumably an error while parsing the JSON body");
        }
        catch (std::bad_alloc&)
        {
          throw OrthancException(ErrorCode_NotEnoughMemory,
                                 "The server hosting Orthanc is running out of memory");
        }
        catch (...)
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "An unhandled exception was generated inside the HTTP server");
        }
      }
      catch (OrthancException& e)
      {
        assert(server != NULL);

        // Using this candidate handler results in an exception
        try
        {
          if (server->GetExceptionFormatter() == NULL)
          {
            CLOG(ERROR, HTTP) << "Exception in the HTTP handler: " << e.What();
            output.SendStatus(e.GetHttpStatus());
          }
          else
          {
            server->GetExceptionFormatter()->Format(output, e, method, requestUri);
          }
        }
        catch (OrthancException&)
        {
          // An exception here reflects the fact that the status code
          // was already set by the HTTP handler.
        }
      }
    }
    catch (...)
    {
      // We should never arrive at this point, where it is even impossible to send an answer
      CLOG(ERROR, HTTP) << "Catastrophic error inside the HTTP server, giving up";
    }
  }


#if MONGOOSE_USE_CALLBACKS == 0
  static void* Callback(enum mg_event event,
                        struct mg_connection *connection,
                        const struct mg_request_info *request)
  {
    if (event == MG_NEW_REQUEST) 
    {
      ProtectedCallback(connection, request);

      // Mark as processed
      return (void*) "";
    }
    else
    {
      return NULL;
    }
  }

#elif MONGOOSE_USE_CALLBACKS == 1
  static int Callback(struct mg_connection *connection)
  {
    const struct mg_request_info *request = mg_get_request_info(connection);

    ProtectedCallback(connection, request);

    return 1;  // Do not let Mongoose handle the request by itself
  }

#else
#  error Please set MONGOOSE_USE_CALLBACKS
#endif





  bool HttpServer::IsRunning() const
  {
    return (pimpl_->context_ != NULL);
  }


  HttpServer::HttpServer() :
    pimpl_(new PImpl),
    handler_(NULL),
    remoteAllowed_(false),
    authentication_(false),
    sslVerifyPeers_(false),
    ssl_(false),
    sslMinimumVersion_(0),  // Default to any of "SSL2+SSL3+TLS1.0+TLS1.1+TLS1.2"
    sslHasCiphers_(false),
    port_(8000),
    filter_(NULL),
    keepAlive_(false),
    httpCompression_(true),
    exceptionFormatter_(NULL),
    realm_(ORTHANC_REALM),
    threadsCount_(50),  // Default value in mongoose/civetweb
    tcpNoDelay_(true),
    requestTimeout_(30)  // Default value in mongoose/civetweb (30 seconds)
  {
#if ORTHANC_ENABLE_MONGOOSE == 1
    CLOG(INFO, HTTP) << "This Orthanc server uses Mongoose as its embedded HTTP server";
#endif

#if ORTHANC_ENABLE_CIVETWEB == 1
    CLOG(INFO, HTTP) << "This Orthanc server uses CivetWeb as its embedded HTTP server";
#endif

#if ORTHANC_ENABLE_SSL == 1
    // Check for the Heartbleed exploit
    // https://en.wikipedia.org/wiki/OpenSSL#Heartbleed_bug
    if (OPENSSL_VERSION_NUMBER <  0x1000107fL  /* openssl-1.0.1g */ &&
        OPENSSL_VERSION_NUMBER >= 0x1000100fL  /* openssl-1.0.1 */) 
    {
      CLOG(WARNING, HTTP) << "This version of OpenSSL is vulnerable to the Heartbleed exploit";
    }
#endif
  }


  HttpServer::~HttpServer()
  {
    Stop();

#if ORTHANC_ENABLE_PUGIXML == 1    
    for (WebDavBuckets::iterator it = webDavBuckets_.begin(); it != webDavBuckets_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
#endif
  }


  void HttpServer::SetPortNumber(uint16_t port)
  {
    Stop();
    port_ = port;
  }

  uint16_t HttpServer::GetPortNumber() const
  {
    return port_;
  }

  void HttpServer::Start()
  {
#if ORTHANC_ENABLE_MONGOOSE == 1
    CLOG(INFO, HTTP) << "Starting embedded Web server using Mongoose";
#elif ORTHANC_ENABLE_CIVETWEB == 1
    CLOG(INFO, HTTP) << "Starting embedded Web server using Civetweb";
#else
#  error
#endif  

    if (!IsRunning())
    {
      std::string port = boost::lexical_cast<std::string>(port_);
      std::string numThreads = boost::lexical_cast<std::string>(threadsCount_);
      std::string requestTimeoutMilliseconds = boost::lexical_cast<std::string>(requestTimeout_ * 1000);
      std::string keepAliveTimeoutMilliseconds = boost::lexical_cast<std::string>(CIVETWEB_KEEP_ALIVE_TIMEOUT_SECONDS * 1000);
      std::string sslMinimumVersion = boost::lexical_cast<std::string>(sslMinimumVersion_);

      if (ssl_)
      {
        port += "s";
      }

      std::vector<const char*> options;

      // Set the TCP port for the HTTP server
      options.push_back("listening_ports");
      options.push_back(port.c_str());
        
      // Optimization reported by Chris Hafey
      // https://groups.google.com/d/msg/orthanc-users/CKueKX0pJ9E/_UCbl8T-VjIJ
      options.push_back("enable_keep_alive");
      options.push_back(keepAlive_ ? "yes" : "no");

#if ORTHANC_ENABLE_CIVETWEB == 1
      /**
       * The "keep_alive_timeout_ms" cannot use milliseconds, as the
       * value of "timeout" in the HTTP header "Keep-Alive" must be
       * expressed in seconds (at least for the Java client).
       * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Keep-Alive
       * https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#enable_keep_alive-no
       * https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#keep_alive_timeout_ms-500-or-0
       **/
      options.push_back("keep_alive_timeout_ms");
      options.push_back(keepAlive_ ? keepAliveTimeoutMilliseconds.c_str() : "0");
#endif

#if ORTHANC_ENABLE_CIVETWEB == 1
      // Disable TCP Nagle's algorithm to maximize speed (this
      // option is not available in Mongoose).
      // https://groups.google.com/d/topic/civetweb/35HBR9seFjU/discussion
      // https://eklitzke.org/the-caveats-of-tcp-nodelay
      options.push_back("tcp_nodelay");
      options.push_back(tcpNoDelay_ ? "1" : "0");
#endif

      // Set the number of threads
      options.push_back("num_threads");
      options.push_back(numThreads.c_str());
        
      // Set the timeout for the HTTP server
      options.push_back("request_timeout_ms");
      options.push_back(requestTimeoutMilliseconds.c_str());

      // Set the client authentication
      options.push_back("ssl_verify_peer");
      options.push_back(sslVerifyPeers_ ? "yes" : "no");

      if (sslVerifyPeers_)
      {
        // Set the trusted client certificates (for X509 mutual authentication)
        options.push_back("ssl_ca_file");
        options.push_back(trustedClientCertificates_.c_str());
      }
      
      if (ssl_)
      {
        // Restrict minimum SSL/TLS protocol version
        options.push_back("ssl_protocol_version");
        options.push_back(sslMinimumVersion.c_str());

        // Set the accepted ciphers list
        if (sslHasCiphers_)
        {
          options.push_back("ssl_cipher_list");
          options.push_back(sslCiphers_.c_str());
        }

        // Set the SSL certificate, if any
        options.push_back("ssl_certificate");
        options.push_back(certificate_.c_str());
      };

      assert(options.size() % 2 == 0);
      options.push_back(NULL);
      
#if MONGOOSE_USE_CALLBACKS == 0
      pimpl_->context_ = mg_start(&Callback, this, &options[0]);

#elif MONGOOSE_USE_CALLBACKS == 1
      struct mg_callbacks callbacks;
      memset(&callbacks, 0, sizeof(callbacks));
      callbacks.begin_request = Callback;
      pimpl_->context_ = mg_start(&callbacks, this, &options[0]);

#else
#  error Please set MONGOOSE_USE_CALLBACKS
#endif

      if (!pimpl_->context_)
      {
        bool isSslError = false;

#if ORTHANC_ENABLE_SSL == 1
        for (;;)
        {
          unsigned long code = ERR_get_error();
          if (code == 0)
          {
            break;
          }
          else
          {
            isSslError = true;
            char message[1024];
            ERR_error_string_n(code, message, sizeof(message) - 1);
            CLOG(ERROR, HTTP) << "OpenSSL error: " << message;
          }
        }        
#endif

        if (isSslError)
        {
          throw OrthancException(ErrorCode_SslInitialization);
        }
        else
        {
          throw OrthancException(ErrorCode_HttpPortInUse,
                                 " (port = " + boost::lexical_cast<std::string>(port_) + ")");
        }
      }

#if ORTHANC_ENABLE_PUGIXML == 1    
      for (WebDavBuckets::iterator it = webDavBuckets_.begin(); it != webDavBuckets_.end(); ++it)
      {
        assert(it->second != NULL);
        it->second->Start();
      }
#endif

      CLOG(WARNING, HTTP) << "HTTP server listening on port: " << GetPortNumber()
                          << " (HTTPS encryption is "
                          << (IsSslEnabled() ? "enabled" : "disabled")
                          << ", remote access is "
                          << (IsRemoteAccessAllowed() ? "" : "not ")
                          << "allowed)";
    }
  }

  void HttpServer::Stop()
  {
    if (IsRunning())
    {
      mg_stop(pimpl_->context_);
      
#if ORTHANC_ENABLE_PUGIXML == 1    
      for (WebDavBuckets::iterator it = webDavBuckets_.begin(); it != webDavBuckets_.end(); ++it)
      {
        assert(it->second != NULL);
        it->second->Stop();
      }
#endif

      pimpl_->context_ = NULL;
    }
  }


  void HttpServer::ClearUsers()
  {
    Stop();
    registeredUsers_.clear();
  }


  void HttpServer::RegisterUser(const char* username,
                                const char* password)
  {
    Stop();

    std::string tag = std::string(username) + ":" + std::string(password);
    std::string encoded;
    Toolbox::EncodeBase64(encoded, tag);
    registeredUsers_.insert(encoded);
  }

  bool HttpServer::IsAuthenticationEnabled() const
  {
    return authentication_;
  }

  void HttpServer::SetSslEnabled(bool enabled)
  {
    Stop();

#if ORTHANC_ENABLE_SSL == 0
    if (enabled)
    {
      throw OrthancException(ErrorCode_SslDisabled);
    }
    else
    {
      ssl_ = false;
    }
#else
    ssl_ = enabled;
#endif
  }

  void HttpServer::SetSslVerifyPeers(bool enabled)
  {
    Stop();

#if ORTHANC_ENABLE_SSL == 0
    if (enabled)
    {
      throw OrthancException(ErrorCode_SslDisabled);
    }
    else
    {
      sslVerifyPeers_ = false;
    }
#else
    sslVerifyPeers_ = enabled;
#endif
  }

  void HttpServer::SetSslMinimumVersion(unsigned int version)
  {
    Stop();
    sslMinimumVersion_ = version;

    std::string info;
    
    switch (version)
    {
      case 0:
        info = "SSL2+SSL3+TLS1.0+TLS1.1+TLS1.2";
        break;

      case 1:
        info = "SSL3+TLS1.0+TLS1.1+TLS1.2";
        break;

      case 2:
        info = "TLS1.0+TLS1.1+TLS1.2";
        break;

      case 3:
        info = "TLS1.1+TLS1.2";
        break;

      case 4:
        info = "TLS1.2";
        break;

      default:
        info = "Unknown value (" + boost::lexical_cast<std::string>(version) + ")";
        break;
    }

    CLOG(INFO, HTTP) << "Minimal accepted version of SSL/TLS protocol: " << info;
  }

  void HttpServer::SetSslCiphers(const std::list<std::string>& ciphers)
  {
    Stop();

    sslHasCiphers_ = true;
    sslCiphers_.clear();

    for (std::list<std::string>::const_iterator
           it = ciphers.begin(); it != ciphers.end(); ++it)
    {
      if (it->empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Empty name for a cipher");
      }

      if (!sslCiphers_.empty())
      {
        sslCiphers_ += ':';
      }
      
      sslCiphers_ += (*it);
    }      

    CLOG(INFO, HTTP) << "List of accepted SSL ciphers: " << sslCiphers_;
    
    if (sslCiphers_.empty())
    {
      CLOG(WARNING, HTTP) << "No cipher is accepted for SSL";
    }
  }

  void HttpServer::SetKeepAliveEnabled(bool enabled)
  {
    Stop();
    keepAlive_ = enabled;
    CLOG(INFO, HTTP) << "HTTP keep alive is " << (enabled ? "enabled" : "disabled");

#if ORTHANC_ENABLE_MONGOOSE == 1
    if (enabled)
    {
      CLOG(WARNING, HTTP) << "You should disable HTTP keep alive, as you are using Mongoose";
    }
#endif
  }

  const std::string &HttpServer::GetSslCertificate() const
  {
    return certificate_;
  }


  void HttpServer::SetAuthenticationEnabled(bool enabled)
  {
    Stop();
    authentication_ = enabled;
  }

  bool HttpServer::IsSslEnabled() const
  {
    return ssl_;
  }

  void HttpServer::SetSslCertificate(const char* path)
  {
    Stop();
    certificate_ = path;
  }

  bool HttpServer::IsRemoteAccessAllowed() const
  {
    return remoteAllowed_;
  }

  void HttpServer::SetSslTrustedClientCertificates(const char* path)
  {
    Stop();
    trustedClientCertificates_ = path;
  }

  bool HttpServer::IsKeepAliveEnabled() const
  {
    return keepAlive_;
  }

  void HttpServer::SetRemoteAccessAllowed(bool allowed)
  {
    Stop();
    remoteAllowed_ = allowed;
  }

  bool HttpServer::IsHttpCompressionEnabled() const
  {
    return httpCompression_;;
  }

  void HttpServer::SetHttpCompressionEnabled(bool enabled)
  {
    Stop();
    httpCompression_ = enabled;
    CLOG(WARNING, HTTP) << "HTTP compression is " << (enabled ? "enabled" : "disabled");
  }

  IIncomingHttpRequestFilter *HttpServer::GetIncomingHttpRequestFilter() const
  {
    return filter_;
  }
  
  void HttpServer::SetIncomingHttpRequestFilter(IIncomingHttpRequestFilter& filter)
  {
    Stop();
    filter_ = &filter;
  }


  void HttpServer::SetHttpExceptionFormatter(IHttpExceptionFormatter& formatter)
  {
    Stop();
    exceptionFormatter_ = &formatter;
  }

  IHttpExceptionFormatter *HttpServer::GetExceptionFormatter()
  {
    return exceptionFormatter_;
  }

  const std::string &HttpServer::GetRealm() const
  {
    return realm_;
  }

  void HttpServer::SetRealm(const std::string &realm)
  {
    realm_ = realm;
  }


  bool HttpServer::IsValidBasicHttpAuthentication(const std::string& basic) const
  {
    return registeredUsers_.find(basic) != registeredUsers_.end();
  }


  void HttpServer::Register(IHttpHandler& handler)
  {
    Stop();
    handler_ = &handler;
  }

  bool HttpServer::HasHandler() const
  {
    return handler_ != NULL;
  }


  IHttpHandler& HttpServer::GetHandler() const
  {
    if (handler_ == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    return *handler_;
  }


  void HttpServer::SetThreadsCount(unsigned int threads)
  {
    if (threads == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    Stop();
    threadsCount_ = threads;

    CLOG(INFO, HTTP) << "The embedded HTTP server will use " << threads << " threads";
  }

  unsigned int HttpServer::GetThreadsCount() const
  {
    return threadsCount_;
  }

  
  void HttpServer::SetTcpNoDelay(bool tcpNoDelay)
  {
    Stop();
    tcpNoDelay_ = tcpNoDelay;
    CLOG(INFO, HTTP) << "TCP_NODELAY for the HTTP sockets is set to "
                     << (tcpNoDelay ? "true" : "false");
  }

  bool HttpServer::IsTcpNoDelay() const
  {
    return tcpNoDelay_;
  }


  void HttpServer::SetRequestTimeout(unsigned int seconds)
  {
    if (seconds == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Request timeout must be a stricly positive integer");
    }

    Stop();
    requestTimeout_ = seconds;
    CLOG(INFO, HTTP) << "Request timeout in the HTTP server is set to " << seconds << " seconds";
  }

  unsigned int HttpServer::GetRequestTimeout() const
  {
    return requestTimeout_;
  }


#if ORTHANC_ENABLE_PUGIXML == 1
  HttpServer::WebDavBuckets& HttpServer::GetWebDavBuckets()
  {
    return webDavBuckets_;
  }
#endif


#if ORTHANC_ENABLE_PUGIXML == 1
  void HttpServer::Register(const std::vector<std::string>& root,
                            IWebDavBucket* bucket)
  {
    std::unique_ptr<IWebDavBucket> protection(bucket);

    if (bucket == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    Stop();
    
#if CIVETWEB_HAS_WEBDAV_WRITING == 0
    if (webDavBuckets_.size() == 0)
    {
      CLOG(WARNING, HTTP) << "Your version of the Orthanc framework was compiled "
                          << "without support for writing into WebDAV collections";
    }
#endif
    
    const std::string s = Toolbox::FlattenUri(root);

    if (webDavBuckets_.find(s) != webDavBuckets_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Cannot register two WebDAV buckets at the same root: " + s);
    }
    else
    {
      CLOG(INFO, HTTP) << "Branching WebDAV bucket at: " << s;
      webDavBuckets_[s] = protection.release();
    }
  }
#endif
}
