/**
 * Palantir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "../PalantirException.h"
#include "../ChunkedBuffer.h"
#include "mongoose.h"


namespace Palantir
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
        mg_write(connection_, buffer, length);
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

    if (requestedWith == headers.end() ||
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
          if (content != Range())
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


  static bool Authorize(MongooseServer& that,
                        HttpOutput& output,
                        struct mg_connection *connection,
                        const struct mg_request_info *request)
  {
    /*std::string s = "HTTP/1.0 401 Unauthorized\r\n" 
      "WWW-Authenticate: Digest realm=\"www.palanthir.com\",qop=\"auth\",nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\""
      "\r\n\r\n";
    output.Send(&s[0], s.size());

    return false;*/

    return true;
  }



  static void* Callback(enum mg_event event,
                        struct mg_connection *connection,
                        const struct mg_request_info *request)
  {
    if (event == MG_NEW_REQUEST) 
    {
      MongooseServer* that = (MongooseServer*) (request->user_data);

      HttpHandler::Arguments arguments, headers;
      MongooseOutput c(connection);

      for (int i = 0; i < request->num_headers; i++)
      {
        std::string name = request->http_headers[i].name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        headers.insert(std::make_pair(name, request->http_headers[i].value));
      }

      printf("=========================\n");
      printf(" URI: [%s]\n", request->uri);
      for (HttpHandler::Arguments::const_iterator i = headers.begin(); i != headers.end(); i++)
      {
        printf("[%s] = [%s]\n", i->first.c_str(), i->second.c_str());
      }

      // Authenticate this connection
      if (!Authorize(*that, c, connection, request))
      {
        return (void*) "";
      }

      std::string postData;

      if (!strcmp(request->request_method, "GET"))
      {
        HttpHandler::ParseGetQuery(arguments, request->query_string);
      }
      else if (!strcmp(request->request_method, "POST"))
      {
        HttpHandler::Arguments::const_iterator ct = headers.find("content-type");
        if (ct == headers.end())
        {
          c.SendHeader(HttpStatus_400_BadRequest);
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
          c.SendHeader(HttpStatus_411_LengthRequired);
          return (void*) "";

        case PostDataStatus_Failure:
          c.SendHeader(HttpStatus_400_BadRequest);
          return (void*) "";

        case PostDataStatus_Pending:
          c.AnswerBuffer("");
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
          handler->Handle(c, std::string(request->request_method),
                          uri, headers, arguments, postData);
        }
        catch (PalantirException& e)
        {
          std::cerr << "MongooseServer Exception [" << e.What() << "]" << std::endl;
          c.SendHeader(HttpStatus_500_InternalServerError);        
        }
      }
      else
      {
        c.SendHeader(HttpStatus_404_NotFound);
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
    ssl_ = false;
    port_ = 8000;
  }


  MongooseServer::~MongooseServer()
  {
    Stop();
    ClearHandlers();
  }


  void MongooseServer::SetPort(uint16_t port)
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
        throw PalantirException("Unable to launch the Mongoose server");
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

#if PALANTIR_SSL_ENABLED == 0
    if (enabled)
    {
      throw PalantirException("Palantir has been build without SSL support");
    }
    else
    {
      ssl_ = false;
    }
#else
    ssl_ = enabled;
#endif
  }

  void MongooseServer::SetSslCertificate(const char* path)
  {
    Stop();
    certificate_ = path;
  }
}
