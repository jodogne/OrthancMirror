/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
  static const char MULTIPART_FORM[] = "multipart/form-data; boundary=";
  static unsigned int MULTIPART_FORM_LENGTH = sizeof(MULTIPART_FORM) / sizeof(char) - 1;


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

      virtual void Send(bool isHeader, const void* buffer, size_t length)
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

      virtual void OnHttpStatusReceived(HttpStatus status)
      {
        // Ignore this
      }

      virtual void DisableKeepAlive()
      {
#if ORTHANC_ENABLE_MONGOOSE == 1
        throw OrthancException(ErrorCode_NotImplemented,
                               "Only available if using CivetWeb");

#elif ORTHANC_ENABLE_CIVETWEB == 1
#  if CIVETWEB_HAS_DISABLE_KEEP_ALIVE == 1
        mg_disable_keep_alive(connection_);
#  else
#       warning The function "mg_disable_keep_alive()" is not available, DICOMweb might run slowly
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


// TODO Move this to external file


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



  class ChunkStore : public boost::noncopyable
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
                         const char* chunkData,
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


  ChunkStore& HttpServer::GetChunkStore()
  {
    return pimpl_->chunkStore_;
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
                                         const IHttpHandler::Arguments& headers)
  {
    IHttpHandler::Arguments::const_iterator contentLength = headers.find("content-length");

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
                                         const IHttpHandler::Arguments& headers)
  {
    IHttpHandler::Arguments::const_iterator contentLength = headers.find("content-length");

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


  static PostDataStatus ParseMultipartForm(std::string &completedFile,
                                           struct mg_connection *connection,
                                           const IHttpHandler::Arguments& headers,
                                           const std::string& contentType,
                                           ChunkStore& chunkStore)
  {
    std::string boundary = "--" + contentType.substr(MULTIPART_FORM_LENGTH);

    std::string body;
    PostDataStatus status = ReadBodyToString(body, connection, headers);

    if (status != PostDataStatus_Success)
    {
      return status;
    }

    /*for (IHttpHandler::Arguments::const_iterator i = headers.begin(); i != headers.end(); i++)
      {
      std::cout << "Header [" << i->first << "] = " << i->second << "\n";
      }
      printf("CHUNK\n");*/

    typedef IHttpHandler::Arguments::const_iterator ArgumentIterator;

    ArgumentIterator requestedWith = headers.find("x-requested-with");
    ArgumentIterator fileName = headers.find("x-file-name");
    ArgumentIterator fileSizeStr = headers.find("x-file-size");

    if (requestedWith != headers.end() &&
        requestedWith->second != "XMLHttpRequest")
    {
      return PostDataStatus_Failure; 
    }

    size_t fileSize = 0;
    if (fileSizeStr != headers.end())
    {
      try
      {
        fileSize = boost::lexical_cast<size_t>(fileSizeStr->second);
      }
      catch (boost::bad_lexical_cast&)
      {
        return PostDataStatus_Failure;
      }
    }

    typedef boost::find_iterator<std::string::iterator> FindIterator;
    typedef boost::iterator_range<char*> Range;

    //chunkStore.Print();

    // TODO - Refactor using class "MultipartStreamReader"
    try
    {
      FindIterator last;
      for (FindIterator it =
             make_find_iterator(body, boost::first_finder(boundary));
           it!=FindIterator();
           ++it)
      {
        if (last != FindIterator())
        {
          Range part(&last->back(), &it->front());
          Range content = boost::find_first(part, "\r\n\r\n");
          if (/*content != Range()*/!content.empty())
          {
            Range c(&content.back() + 1, &it->front() - 2);
            size_t chunkSize = c.size();

            if (chunkSize > 0)
            {
              const char* chunkData = &c.front();

              if (fileName == headers.end())
              {
                // This file is stored in a single chunk
                completedFile.resize(chunkSize);
                memcpy(&completedFile[0], chunkData, chunkSize);
                return PostDataStatus_Success;
              }
              else
              {
                return chunkStore.Store(completedFile, chunkData, chunkSize, fileName->second, fileSize);
              }
            }
          }
        }

        last = it;
      }
    }
    catch (std::length_error&)
    {
      return PostDataStatus_Failure;
    }

    return PostDataStatus_Pending;
  }


  static bool IsAccessGranted(const HttpServer& that,
                              const IHttpHandler::Arguments& headers)
  {
    bool granted = false;

    IHttpHandler::Arguments::const_iterator auth = headers.find("authorization");
    if (auth != headers.end())
    {
      std::string s = auth->second;
      if (s.size() > 6 &&
          s.substr(0, 6) == "Basic ")
      {
        std::string b64 = s.substr(6);
        granted = that.IsValidBasicHttpAuthentication(b64);
      }
    }

    return granted;
  }


  static std::string GetAuthenticatedUsername(const IHttpHandler::Arguments& headers)
  {
    IHttpHandler::Arguments::const_iterator auth = headers.find("authorization");

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
                            const IHttpHandler::Arguments& headers,
                            const IHttpHandler::GetArguments& argumentsGET)
  {
    std::string overriden;

    // Check whether some PUT/DELETE faking is done

    // 1. Faking with Google's approach
    IHttpHandler::Arguments::const_iterator methodOverride =
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

      LOG(INFO) << "HTTP method faking has been detected for " << overriden;

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
                                       const IHttpHandler::Arguments& headers)
  {
    // Look if the client wishes HTTP compression
    // https://en.wikipedia.org/wiki/HTTP_compression
    IHttpHandler::Arguments::const_iterator it = headers.find("accept-encoding");
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
    LOG(ERROR) << "Orthanc was compiled without support for read-write access to WebDAV: " << uri;
    output.SendStatus(HttpStatus_403_Forbidden);
  }    
#  endif
  
  static bool HandleWebDav(HttpOutput& output,
                           const HttpServer::WebDavBuckets& buckets,
                           const std::string& method,
                           const IHttpHandler::Arguments& headers,
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
            IHttpHandler::Arguments::const_iterator i = headers.find("depth");
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
              LOG(ERROR) << "Cannot read the content of a file to be stored in WebDAV";
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
      output.SendUnauthorized(server.GetRealm());
      return;
    }


    // Extract the HTTP headers
    IHttpHandler::Arguments headers;
    for (int i = 0; i < request->num_headers; i++)
    {
      std::string name = request->http_headers[i].name;
      std::string value = request->http_headers[i].value;

      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      headers.insert(std::make_pair(name, value));
      LOG(TRACE) << "HTTP header: [" << name << "]: [" << value << "]";
    }

    if (server.IsHttpCompressionEnabled())
    {
      ConfigureHttpCompression(output, headers);
    }


    // Extract the GET arguments
    IHttpHandler::GetArguments argumentsGET;
    if (!strcmp(request->request_method, "GET"))
    {
      HttpToolbox::ParseGetArguments(argumentsGET, request->query_string);
    }


    // Authenticate this connection
    if (server.IsAuthenticationEnabled() && 
        !IsAccessGranted(server, headers))
    {
      output.SendUnauthorized(server.GetRealm());
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
      LOG(INFO) << EnumerationToString(method) << " " << Toolbox::FlattenUri(uri);
      filterMethod = method;
    }
#if ORTHANC_ENABLE_PUGIXML == 1
    else if (!strcmp(request->request_method, "OPTIONS") ||
             !strcmp(request->request_method, "PROPFIND") ||
             !strcmp(request->request_method, "HEAD"))
    {
      LOG(INFO) << "Incoming read-only WebDAV request: "
                << request->request_method << " " << requestUri;
      filterMethod = HttpMethod_Get;
      isWebDav = true;
    }
    else if (!strcmp(request->request_method, "PROPPATCH") ||
             !strcmp(request->request_method, "LOCK") ||
             !strcmp(request->request_method, "UNLOCK") ||
             !strcmp(request->request_method, "MKCOL"))
    {
      LOG(INFO) << "Incoming read-write WebDAV request: "
                << request->request_method << " " << requestUri;
      filterMethod = HttpMethod_Put;
      isWebDav = true;
    }
#endif /* ORTHANC_ENABLE_PUGIXML == 1 */
    else
    {
      LOG(INFO) << "Unknown HTTP method: " << request->request_method;
      output.SendStatus(HttpStatus_400_BadRequest);
      return;
    }
    

    // Check that this connection is allowed by the user's authentication filter
    const std::string username = GetAuthenticatedUsername(headers);

    IIncomingHttpRequestFilter *filter = server.GetIncomingHttpRequestFilter();
    if (filter != NULL)
    {
      if (!filter->IsAllowed(filterMethod, requestUri, remoteIp,
                             username.c_str(), headers, argumentsGET))
      {
        //output.SendUnauthorized(server.GetRealm());
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
      LOG(INFO) << "No WebDAV bucket is registered against URI: "
                << request->request_method << " " << requestUri;
      output.SendStatus(HttpStatus_404_NotFound);
      return;
    }
#endif


    bool found = false;

    // Extract the body of the request for PUT and POST, or process
    // the body as a stream

    // TODO Avoid unneccessary memcopy of the body

    std::string body;
    if (method == HttpMethod_Post ||
        method == HttpMethod_Put)
    {
      PostDataStatus status;

      bool isMultipartForm = false;

      IHttpHandler::Arguments::const_iterator ct = headers.find("content-type");
      if (ct != headers.end() &&
          ct->second.size() >= MULTIPART_FORM_LENGTH &&
          !memcmp(ct->second.c_str(), MULTIPART_FORM, MULTIPART_FORM_LENGTH))
      {
        /** 
         * The user uses the "upload" form of Orthanc Explorer, for
         * file uploads through a HTML form.
         **/
        status = ParseMultipartForm(body, connection, headers, ct->second, server.GetChunkStore());
        isMultipartForm = true;
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
            LOG(ERROR) << "Exception in the HTTP handler: " << e.What();
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
      LOG(ERROR) << "Catastrophic error inside the HTTP server, giving up";
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
    port_(8000),
    filter_(NULL),
    keepAlive_(false),
    httpCompression_(true),
    exceptionFormatter_(NULL),
    realm_(ORTHANC_REALM),
    threadsCount_(50),  // Default value in mongoose
    tcpNoDelay_(true),
    requestTimeout_(30)  // Default value in mongoose/civetweb (30 seconds)    
  {
#if ORTHANC_ENABLE_MONGOOSE == 1
    LOG(INFO) << "This Orthanc server uses Mongoose as its embedded HTTP server";
#endif

#if ORTHANC_ENABLE_CIVETWEB == 1
    LOG(INFO) << "This Orthanc server uses CivetWeb as its embedded HTTP server";
#endif

#if ORTHANC_ENABLE_SSL == 1
    // Check for the Heartbleed exploit
    // https://en.wikipedia.org/wiki/OpenSSL#Heartbleed_bug
    if (OPENSSL_VERSION_NUMBER <  0x1000107fL  /* openssl-1.0.1g */ &&
        OPENSSL_VERSION_NUMBER >= 0x1000100fL  /* openssl-1.0.1 */) 
    {
      LOG(WARNING) << "This version of OpenSSL is vulnerable to the Heartbleed exploit";
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

  void HttpServer::Start()
  {
#if ORTHANC_ENABLE_MONGOOSE == 1
    LOG(INFO) << "Starting embedded Web server using Mongoose";
#elif ORTHANC_ENABLE_CIVETWEB == 1
    LOG(INFO) << "Starting embedded Web server using Civetweb";
#else
#  error
#endif  

    if (!IsRunning())
    {
      std::string port = boost::lexical_cast<std::string>(port_);
      std::string numThreads = boost::lexical_cast<std::string>(threadsCount_);
      std::string requestTimeoutMilliseconds = boost::lexical_cast<std::string>(requestTimeout_ * 1000);

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
      // https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#enable_keep_alive-no
      options.push_back("keep_alive_timeout_ms");
      options.push_back(keepAlive_ ? "500" : "0");
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
            LOG(ERROR) << "OpenSSL error: " << message;
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

      LOG(WARNING) << "HTTP server listening on port: " << GetPortNumber()
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

  void HttpServer::SetKeepAliveEnabled(bool enabled)
  {
    Stop();
    keepAlive_ = enabled;
    LOG(INFO) << "HTTP keep alive is " << (enabled ? "enabled" : "disabled");

#if ORTHANC_ENABLE_MONGOOSE == 1
    if (enabled)
    {
      LOG(WARNING) << "You should disable HTTP keep alive, as you are using Mongoose";
    }
#endif
  }


  void HttpServer::SetAuthenticationEnabled(bool enabled)
  {
    Stop();
    authentication_ = enabled;
  }

  void HttpServer::SetSslCertificate(const char* path)
  {
    Stop();
    certificate_ = path;
  }

  void HttpServer::SetSslTrustedClientCertificates(const char* path)
  {
    Stop();
    trustedClientCertificates_ = path;
  }

  void HttpServer::SetRemoteAccessAllowed(bool allowed)
  {
    Stop();
    remoteAllowed_ = allowed;
  }

  void HttpServer::SetHttpCompressionEnabled(bool enabled)
  {
    Stop();
    httpCompression_ = enabled;
    LOG(WARNING) << "HTTP compression is " << (enabled ? "enabled" : "disabled");
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


  bool HttpServer::IsValidBasicHttpAuthentication(const std::string& basic) const
  {
    return registeredUsers_.find(basic) != registeredUsers_.end();
  }


  void HttpServer::Register(IHttpHandler& handler)
  {
    Stop();
    handler_ = &handler;
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

    LOG(INFO) << "The embedded HTTP server will use " << threads << " threads";
  }

  
  void HttpServer::SetTcpNoDelay(bool tcpNoDelay)
  {
    Stop();
    tcpNoDelay_ = tcpNoDelay;
    LOG(INFO) << "TCP_NODELAY for the HTTP sockets is set to "
              << (tcpNoDelay ? "true" : "false");
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
    LOG(INFO) << "Request timeout in the HTTP server is set to " << seconds << " seconds";
  }


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
      LOG(WARNING) << "Your version of the Orthanc framework was compiled "
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
      LOG(INFO) << "Branching WebDAV bucket at: " << s;
      webDavBuckets_[s] = protection.release();
    }
  }
#endif
}
