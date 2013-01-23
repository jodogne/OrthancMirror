/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


// http://en.highscore.de/cpp/boost/stringhandling.html

#include "MongooseServer.h"

#include <algorithm>
#include <string.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <boost/thread.hpp>
#include <glog/logging.h>

#include "../OrthancException.h"
#include "../ChunkedBuffer.h"
#include "HttpOutput.h"
#include "mongoose.h"


#define ORTHANC_REALM "Orthanc Secure Area"

static const long LOCALHOST = (127ll << 24) + 1ll;


namespace Orthanc
{
  static const char multipart[] = "multipart/form-data; boundary=";
  static unsigned int multipartLength = sizeof(multipart) / sizeof(char) - 1;


  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    class MongooseOutput : public HttpOutput
    {
    private:
      struct mg_connection* connection_;

    public:
      MongooseOutput(struct mg_connection* connection) : connection_(connection)
      {
      }

      virtual void Send(const void* buffer, size_t length)
      {
        if (length > 0)
        {
          mg_write(connection_, buffer, length);
        }
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
    ChunkedFile(const std::string& filename) :
      filename_(filename)
    {
    }

    const std::string& GetFilename() const
    {
      return filename_;
    }
  };



  class ChunkStore
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
           it != content_.end(); it++)
      {
        delete *it;
      }
    }

    Content::iterator Find(const std::string& filename)
    {
      for (Content::iterator it = content_.begin();
           it != content_.end(); it++)
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


  struct MongooseServer::PImpl
  {
    struct mg_context *context_;
    ChunkStore chunkStore_;
  };


  ChunkStore& MongooseServer::GetChunkStore()
  {
    return pimpl_->chunkStore_;
  }



  HttpHandler* MongooseServer::FindHandler(const UriComponents& forUri) const
  {
    for (Handlers::const_iterator it = 
           handlers_.begin(); it != handlers_.end(); it++) 
    {
      if ((*it)->IsServedUri(forUri))
      {
        return *it;
      }
    }

    return NULL;
  }




  static PostDataStatus ReadPostData(std::string& postData,
                                     struct mg_connection *connection,
                                     const HttpHandler::Arguments& headers)
  {
    HttpHandler::Arguments::const_iterator cs = headers.find("content-length");
    if (cs == headers.end())
    {
      return PostDataStatus_NoLength;
    }

    int length;      
    try
    {
      length = boost::lexical_cast<int>(cs->second);
    }
    catch (boost::bad_lexical_cast)
    {
      return PostDataStatus_NoLength;
    }

    if (length < 0)
    {
      length = 0;
    }

    postData.resize(length);

    size_t pos = 0;
    while (length > 0)
    {
      int r = mg_read(connection, &postData[pos], length);
      if (r <= 0)
      {
        return PostDataStatus_Failure;
      }
      assert(r <= length);
      length -= r;
      pos += r;
    }

    return PostDataStatus_Success;
  }



  static PostDataStatus ParseMultipartPost(std::string &completedFile,
                                           struct mg_connection *connection,
                                           const HttpHandler::Arguments& headers,
                                           const std::string& contentType,
                                           ChunkStore& chunkStore)
  {
    std::string boundary = "--" + contentType.substr(multipartLength);

    std::string postData;
    PostDataStatus status = ReadPostData(postData, connection, headers);

    if (status != PostDataStatus_Success)
    {
      return status;
    }

    /*for (HttpHandler::Arguments::const_iterator i = headers.begin(); i != headers.end(); i++)
      {
      std::cout << "Header [" << i->first << "] = " << i->second << "\n";
      }
      printf("CHUNK\n");*/

    typedef HttpHandler::Arguments::const_iterator ArgumentIterator;

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
      catch (boost::bad_lexical_cast)
      {
        return PostDataStatus_Failure;
      }
    }

    typedef boost::find_iterator<std::string::iterator> FindIterator;
    typedef boost::iterator_range<char*> Range;

    //chunkStore.Print();

    try
    {
      FindIterator last;
      for (FindIterator it =
             make_find_iterator(postData, boost::first_finder(boundary));
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
                if (chunkSize > 0)
                {
                  memcpy(&completedFile[0], chunkData, chunkSize);
                }
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
    catch (std::length_error)
    {
      return PostDataStatus_Failure;
    }

    return PostDataStatus_Pending;
  }


  static void SendUnauthorized(HttpOutput& output)
  {
    std::string s = "HTTP/1.1 401 Unauthorized\r\n" 
      "WWW-Authenticate: Basic realm=\"" ORTHANC_REALM "\""
      "\r\n\r\n";
    output.Send(&s[0], s.size());
  }


  static bool Authorize(const MongooseServer& that,
                        const HttpHandler::Arguments& headers,
                        HttpOutput& output)
  {
    bool granted = false;

    HttpHandler::Arguments::const_iterator auth = headers.find("authorization");
    if (auth != headers.end())
    {
      std::string s = auth->second;
      if (s.substr(0, 6) == "Basic ")
      {
        std::string b64 = s.substr(6);
        granted = that.IsValidBasicHttpAuthentication(b64);
      }
    }

    if (!granted)
    {
      SendUnauthorized(output);
      return false;
    }
    else
    {
      return true;
    }
  }



  static void* Callback(enum mg_event event,
                        struct mg_connection *connection,
                        const struct mg_request_info *request)
  {
    if (event == MG_NEW_REQUEST) 
    {
      MongooseServer* that = (MongooseServer*) (request->user_data);
      MongooseOutput output(connection);

      if (!that->IsRemoteAccessAllowed() &&
          request->remote_ip != LOCALHOST)
      {
        SendUnauthorized(output);
        return (void*) "";
      }

      HttpHandler::Arguments arguments, headers;

      for (int i = 0; i < request->num_headers; i++)
      {
        std::string name = request->http_headers[i].name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        headers.insert(std::make_pair(name, request->http_headers[i].value));
      }

      // Authenticate this connection
      if (that->IsAuthenticationEnabled() &&
          !Authorize(*that, headers, output))
      {
        return (void*) "";
      }

      std::string postData;

      if (!strcmp(request->request_method, "GET"))
      {
        HttpHandler::ParseGetQuery(arguments, request->query_string);
      }
      else if (!strcmp(request->request_method, "POST") ||
               !strcmp(request->request_method, "PUT"))
      {
        HttpHandler::Arguments::const_iterator ct = headers.find("content-type");
        if (ct == headers.end())
        {
          output.SendHeader(Orthanc_HttpStatus_400_BadRequest);
          return (void*) "";
        }

        PostDataStatus status;
      
        std::string contentType = ct->second;
        if (contentType.size() >= multipartLength &&
            !memcmp(contentType.c_str(), multipart, multipartLength))
        {
          status = ParseMultipartPost(postData, connection, headers, contentType, that->GetChunkStore());
        }
        else
        {
          status = ReadPostData(postData, connection, headers);
        }

        switch (status)
        {
        case PostDataStatus_NoLength:
          output.SendHeader(Orthanc_HttpStatus_411_LengthRequired);
          return (void*) "";

        case PostDataStatus_Failure:
          output.SendHeader(Orthanc_HttpStatus_400_BadRequest);
          return (void*) "";

        case PostDataStatus_Pending:
          output.AnswerBufferWithContentType(NULL, 0, "");
          return (void*) "";

        default:
          break;
        }
      }

      UriComponents uri;
      Toolbox::SplitUriComponents(uri, request->uri);

      HttpHandler* handler = that->FindHandler(uri);
      if (handler)
      {
        try
        {
          handler->Handle(output, std::string(request->request_method),
                          uri, headers, arguments, postData);
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "MongooseServer Exception [" << e.What() << "]";
          output.SendHeader(Orthanc_HttpStatus_500_InternalServerError);        
        }
        catch (boost::bad_lexical_cast&)
        {
          LOG(ERROR) << "MongooseServer Exception: Bad lexical cast";
          output.SendHeader(Orthanc_HttpStatus_400_BadRequest);
        }
        catch (std::runtime_error&)
        {
          LOG(ERROR) << "MongooseServer Exception: Presumably a bad JSON request";
          output.SendHeader(Orthanc_HttpStatus_400_BadRequest);
        }
      }
      else
      {
        output.SendHeader(Orthanc_HttpStatus_404_NotFound);
      }

      // Mark as processed
      return (void*) "";
    } 
    else 
    {
      return NULL;
    }
  }


  bool MongooseServer::IsRunning() const
  {
    return (pimpl_->context_ != NULL);
  }


  MongooseServer::MongooseServer() : pimpl_(new PImpl)
  {
    pimpl_->context_ = NULL;
    remoteAllowed_ = false;
    authentication_ = false;
    ssl_ = false;
    port_ = 8000;
  }


  MongooseServer::~MongooseServer()
  {
    Stop();
    ClearHandlers();
  }


  void MongooseServer::SetPortNumber(uint16_t port)
  {
    Stop();
    port_ = port;
  }

  void MongooseServer::Start()
  {
    if (!IsRunning())
    {
      std::string port = boost::lexical_cast<std::string>(port_);

      if (ssl_)
      {
        port += "s";
      }

      const char *options[] = {
        "listening_ports", port.c_str(), 
        ssl_ ? "ssl_certificate" : NULL,
        certificate_.c_str(),
        NULL
      };

      pimpl_->context_ = mg_start(&Callback, this, options);
      if (!pimpl_->context_)
      {
        throw OrthancException("Unable to launch the Mongoose server");
      }
    }
  }

  void MongooseServer::Stop()
  {
    if (IsRunning())
    {
      mg_stop(pimpl_->context_);
      pimpl_->context_ = NULL;
    }
  }


  void MongooseServer::RegisterHandler(HttpHandler* handler)
  {
    Stop();

    handlers_.push_back(handler);
  }


  void MongooseServer::ClearHandlers()
  {
    Stop();

    for (Handlers::iterator it = 
           handlers_.begin(); it != handlers_.end(); it++)
    {
      delete *it;
    }
  }


  void MongooseServer::ClearUsers()
  {
    Stop();
    registeredUsers_.clear();
  }


  void MongooseServer::RegisterUser(const char* username,
                                    const char* password)
  {
    Stop();

    std::string tag = std::string(username) + ":" + std::string(password);
    registeredUsers_.insert(Toolbox::EncodeBase64(tag));
  }

  void MongooseServer::SetSslEnabled(bool enabled)
  {
    Stop();

#if ORTHANC_SSL_ENABLED == 0
    if (enabled)
    {
      throw OrthancException("Orthanc has been built without SSL support");
    }
    else
    {
      ssl_ = false;
    }
#else
    ssl_ = enabled;
#endif
  }

  void MongooseServer::SetAuthenticationEnabled(bool enabled)
  {
    Stop();
    authentication_ = enabled;
  }

  void MongooseServer::SetSslCertificate(const char* path)
  {
    Stop();
    certificate_ = path;
  }

  void MongooseServer::SetRemoteAccessAllowed(bool allowed)
  {
    Stop();
    remoteAllowed_ = allowed;
  }


  bool MongooseServer::IsValidBasicHttpAuthentication(const std::string& basic) const
  {
    return registeredUsers_.find(basic) != registeredUsers_.end();
  }
}
