#include "gtest/gtest.h"

#include "../OrthancServer/DatabaseWrapper.h"

#include <ctype.h>
#include <glog/logging.h>

using namespace Orthanc;

namespace
{
  class ServerIndexListener : public IServerIndexListener
  {
  public:
    std::set<std::string> deletedFiles_;
    std::string ancestorId_;
    ResourceType ancestorType_;

    void Reset()
    {
      ancestorId_ = "";
      deletedFiles_.clear();
    }

    virtual void SignalRemainingAncestor(ResourceType type,
                                         const std::string& publicId) 
    {
      ancestorId_ = publicId;
      ancestorType_ = type;
    }

    virtual void SignalFileDeleted(const std::string& fileUuid)
    {
      deletedFiles_.insert(fileUuid);
      LOG(INFO) << "A file must be removed: " << fileUuid;
    }                                
  };
}


TEST(DatabaseWrapper, Simple)
{
  ServerIndexListener listener;
  DatabaseWrapper index(listener);

  int64_t a[] = {
    index.CreateResource("a", ResourceType_Patient),   // 0
    index.CreateResource("b", ResourceType_Study),     // 1
    index.CreateResource("c", ResourceType_Series),    // 2
    index.CreateResource("d", ResourceType_Instance),  // 3
    index.CreateResource("e", ResourceType_Instance),  // 4
    index.CreateResource("f", ResourceType_Instance),  // 5
    index.CreateResource("g", ResourceType_Study)      // 6
  };

  ASSERT_EQ("a", index.GetPublicId(a[0]));
  ASSERT_EQ("b", index.GetPublicId(a[1]));
  ASSERT_EQ("c", index.GetPublicId(a[2]));
  ASSERT_EQ("d", index.GetPublicId(a[3]));
  ASSERT_EQ("e", index.GetPublicId(a[4]));
  ASSERT_EQ("f", index.GetPublicId(a[5]));
  ASSERT_EQ("g", index.GetPublicId(a[6]));

  {
    Json::Value t;
    index.GetAllPublicIds(t, ResourceType_Patient);

    ASSERT_EQ(1, t.size());
    ASSERT_EQ("a", t[0u].asString());

    index.GetAllPublicIds(t, ResourceType_Series);
    ASSERT_EQ(1, t.size());
    ASSERT_EQ("c", t[0u].asString());

    index.GetAllPublicIds(t, ResourceType_Study);
    ASSERT_EQ(2, t.size());

    index.GetAllPublicIds(t, ResourceType_Instance);
    ASSERT_EQ(3, t.size());
  }

  index.SetGlobalProperty(GlobalProperty_FlushSleep, "World");

  index.AttachChild(a[0], a[1]);
  index.AttachChild(a[1], a[2]);
  index.AttachChild(a[2], a[3]);
  index.AttachChild(a[2], a[4]);
  index.AttachChild(a[6], a[5]);

  int64_t parent;
  ASSERT_FALSE(index.LookupParent(parent, a[0]));
  ASSERT_TRUE(index.LookupParent(parent, a[1])); ASSERT_EQ(a[0], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[2])); ASSERT_EQ(a[1], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[3])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[4])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[5])); ASSERT_EQ(a[6], parent);
  ASSERT_FALSE(index.LookupParent(parent, a[6]));

  std::string s;
  
  ASSERT_FALSE(index.GetParentPublicId(s, a[0]));
  ASSERT_FALSE(index.GetParentPublicId(s, a[6]));
  ASSERT_TRUE(index.GetParentPublicId(s, a[1])); ASSERT_EQ("a", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[2])); ASSERT_EQ("b", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[3])); ASSERT_EQ("c", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[4])); ASSERT_EQ("c", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[5])); ASSERT_EQ("g", s);

  std::list<std::string> l;
  index.GetChildrenPublicId(l, a[0]); ASSERT_EQ(1, l.size()); ASSERT_EQ("b", l.front());
  index.GetChildrenPublicId(l, a[1]); ASSERT_EQ(1, l.size()); ASSERT_EQ("c", l.front());
  index.GetChildrenPublicId(l, a[3]); ASSERT_EQ(0, l.size()); 
  index.GetChildrenPublicId(l, a[4]); ASSERT_EQ(0, l.size()); 
  index.GetChildrenPublicId(l, a[5]); ASSERT_EQ(0, l.size()); 
  index.GetChildrenPublicId(l, a[6]); ASSERT_EQ(1, l.size()); ASSERT_EQ("f", l.front());

  index.GetChildrenPublicId(l, a[2]); ASSERT_EQ(2, l.size()); 
  if (l.front() == "d")
  {
    ASSERT_EQ("e", l.back());
  }
  else
  {
    ASSERT_EQ("d", l.back());
    ASSERT_EQ("e", l.front());
  }

  index.AttachFile(a[4], AttachedFileType_Json, "my json file", 21, 42, CompressionType_Zlib);
  index.AttachFile(a[4], AttachedFileType_Dicom, "my dicom file", 42);
  index.AttachFile(a[6], AttachedFileType_Dicom, "world", 44);
  index.SetMetadata(a[4], MetadataType_Instance_RemoteAet, "PINNACLE");

  ASSERT_EQ(21 + 42 + 44, index.GetTotalCompressedSize());
  ASSERT_EQ(42 + 42 + 44, index.GetTotalUncompressedSize());

  DicomMap m;
  m.SetValue(0x0010, 0x0010, "PatientName");
  index.SetMainDicomTags(a[3], m);

  int64_t b;
  ResourceType t;
  ASSERT_TRUE(index.LookupResource("g", b, t));
  ASSERT_EQ(7, b);
  ASSERT_EQ(ResourceType_Study, t);

  ASSERT_TRUE(index.LookupMetadata(s, a[4], MetadataType_Instance_RemoteAet));
  ASSERT_FALSE(index.LookupMetadata(s, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("PINNACLE", s);
  ASSERT_EQ("PINNACLE", index.GetMetadata(a[4], MetadataType_Instance_RemoteAet));
  ASSERT_EQ("None", index.GetMetadata(a[4], MetadataType_Instance_IndexInSeries, "None"));

  ASSERT_TRUE(index.LookupGlobalProperty(s, GlobalProperty_FlushSleep));
  ASSERT_FALSE(index.LookupGlobalProperty(s, static_cast<GlobalProperty>(42)));
  ASSERT_EQ("World", s);
  ASSERT_EQ("World", index.GetGlobalProperty(GlobalProperty_FlushSleep));
  ASSERT_EQ("None", index.GetGlobalProperty(static_cast<GlobalProperty>(42), "None"));

  uint64_t us, cs;
  CompressionType ct;
  ASSERT_TRUE(index.LookupFile(a[4], AttachedFileType_Json, s, cs, us, ct));
  ASSERT_EQ("my json file", s);
  ASSERT_EQ(21, cs);
  ASSERT_EQ(42, us);
  ASSERT_EQ(CompressionType_Zlib, ct);

  ASSERT_EQ(0u, listener.deletedFiles_.size());
  ASSERT_EQ(7, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(3, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(1, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(1, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[0]);

  ASSERT_EQ(2, listener.deletedFiles_.size());
  ASSERT_FALSE(listener.deletedFiles_.find("my json file") == listener.deletedFiles_.end());
  ASSERT_FALSE(listener.deletedFiles_.find("my dicom file") == listener.deletedFiles_.end());

  ASSERT_EQ(2, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(0, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(1, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(0, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[5]);
  ASSERT_EQ(0, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(0, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(1, index.GetTableRecordCount("GlobalProperties"));

  ASSERT_EQ(3, listener.deletedFiles_.size());
  ASSERT_FALSE(listener.deletedFiles_.find("world") == listener.deletedFiles_.end());
}




TEST(DatabaseWrapper, Upward)
{
  ServerIndexListener listener;
  DatabaseWrapper index(listener);

  int64_t a[] = {
    index.CreateResource("a", ResourceType_Patient),   // 0
    index.CreateResource("b", ResourceType_Study),     // 1
    index.CreateResource("c", ResourceType_Series),    // 2
    index.CreateResource("d", ResourceType_Instance),  // 3
    index.CreateResource("e", ResourceType_Instance),  // 4
    index.CreateResource("f", ResourceType_Study),     // 5
    index.CreateResource("g", ResourceType_Series),    // 6
    index.CreateResource("h", ResourceType_Series)     // 7
  };

  index.AttachChild(a[0], a[1]);
  index.AttachChild(a[1], a[2]);
  index.AttachChild(a[2], a[3]);
  index.AttachChild(a[2], a[4]);
  index.AttachChild(a[1], a[6]);
  index.AttachChild(a[0], a[5]);
  index.AttachChild(a[5], a[7]);

  {
    Json::Value j;
    index.GetChildren(j, a[0]);
    ASSERT_EQ(2, j.size());
    ASSERT_TRUE((j[0u] == "b" && j[1u] == "f") ||
                (j[1u] == "b" && j[0u] == "f"));

    index.GetChildren(j, a[1]);
    ASSERT_EQ(2, j.size());
    ASSERT_TRUE((j[0u] == "c" && j[1u] == "g") ||
                (j[1u] == "c" && j[0u] == "g"));

    index.GetChildren(j, a[2]);
    ASSERT_EQ(2, j.size());
    ASSERT_TRUE((j[0u] == "d" && j[1u] == "e") ||
                (j[1u] == "d" && j[0u] == "e"));

    index.GetChildren(j, a[3]); ASSERT_EQ(0, j.size());
    index.GetChildren(j, a[4]); ASSERT_EQ(0, j.size());
    index.GetChildren(j, a[5]); ASSERT_EQ(1, j.size()); ASSERT_EQ("h", j[0u].asString());
    index.GetChildren(j, a[6]); ASSERT_EQ(0, j.size());
    index.GetChildren(j, a[7]); ASSERT_EQ(0, j.size());
  }

  listener.Reset();
  index.DeleteResource(a[3]);
  ASSERT_EQ("c", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Series, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[4]);
  ASSERT_EQ("b", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Study, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[7]);
  ASSERT_EQ("a", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Patient, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[6]);
  ASSERT_EQ("", listener.ancestorId_);  // No more ancestor
}




#include "../Core/HttpServer/FilesystemHttpSender.h"

#include "../Core/Toolbox.h"
#include "../Core/HttpServer/HttpOutput.h"
#include "../Core/HttpServer/HttpHandler.h"

#include "../Core/HttpServer/HttpFileSender.h"


namespace Orthanc
{
  class RestApiPath
  {
  private:
    UriComponents uri_;
    bool hasTrailing_;
    std::vector<std::string> components_;

  public:
    typedef std::map<std::string, std::string> Components;

    RestApiPath(const std::string& uri)
    {
      Toolbox::SplitUriComponents(uri_, uri);

      if (uri_.size() == 0)
      {
        return;
      }

      if (uri_.back() == "*")
      {
        hasTrailing_ = true;
        uri_.pop_back();
      }
      else
      {
        hasTrailing_ = false;
      }

      components_.resize(uri_.size());
      for (size_t i = 0; i < uri_.size(); i++)
      {
        size_t s = uri_[i].size();
        assert(s > 0);

        if (uri_[i][0] == '{' && 
            uri_[i][s - 1] == '}')
        {
          components_[i] = uri_[i].substr(1, s - 2);
          uri_[i] = "";
        }
        else
        {
          components_[i] = "";
        }
      }
    }

    // This version is slower
    bool Match(Components& components,
               UriComponents& trailing,
               const std::string& uriRaw) const
    {
      UriComponents uri;
      Toolbox::SplitUriComponents(uri, uriRaw);
      return Match(components, trailing, uri);
    }

    bool Match(Components& components,
               UriComponents& trailing,
               const UriComponents& uri) const
    {
      if (uri.size() < uri_.size())
      {
        return false;
      }

      if (!hasTrailing_ && uri.size() > uri_.size())
      {
        return false;
      }

      components.clear();
      trailing.clear();

      assert(uri_.size() <= uri.size());
      for (size_t i = 0; i < uri_.size(); i++)
      {
        if (components_[i].size() == 0)
        {
          // This URI component is not a free parameter
          if (uri_[i] != uri[i])
          {
            return false;
          }
        }
        else
        {
          // This URI component is a free parameter
          components[components_[i]] = uri[i];
        }
      }

      if (hasTrailing_)
      {
        trailing.assign(uri.begin() + uri_.size(), uri.end());
      }

      return true;
    }

    bool Match(const UriComponents& uri) const
    {
      Components components;
      UriComponents trailing;
      return Match(components, trailing, uri);
    }
  };


  class RestApiOutput
  {
  private:
    HttpOutput& output_;

  public:
    RestApiOutput(HttpOutput& output) : output_(output)
    {
    }

    void AnswerFile(HttpFileSender& sender)
    {
      sender.Send(output_);
    }

    void AnswerJson(const Json::Value& value)
    {
      Json::StyledWriter writer;
      std::string s = writer.write(value);
      output_.AnswerBufferWithContentType(s, "application/json");
    }

    void AnswerBuffer(const std::string& buffer,
                      const std::string& contentType)
    {
      output_.AnswerBufferWithContentType(buffer, contentType);
    }

    void Redirect(const char* path)
    {
      output_.Redirect(path);
    }
  };


  class RestApiSharedCall
  {
  protected:
    RestApiOutput* output_;
    IDynamicObject* context_;
    const HttpHandler::Arguments* httpHeaders_;
    const RestApiPath::Components* uriComponents_;
    const UriComponents* trailing_;

  public:
    RestApiOutput& GetOutput()
    {
      return *output_;
    }

    IDynamicObject* GetContext()
    {
      return context_;
    }
    
    const HttpHandler::Arguments& GetHttpHeaders() const
    {
      return *httpHeaders_;
    }

    const RestApiPath::Components& GetUriComponents() const
    {
      return *uriComponents_;
    }

    const UriComponents& GetTrailing() const
    {
      return *trailing_;
    }

    std::string GetUriComponent(const std::string& name,
                                const std::string& defaultValue)
    {
      return HttpHandler::GetArgument(*uriComponents_, name, defaultValue);
    }
  };

 
  class RestApiPutCall : public RestApiSharedCall
  {
    friend class RestApi;

  private:
    const std::string* data_;

  public:
    const std::string& GetData()
    {
      return *data_;
    }
  };


  class RestApiPostCall : public RestApiSharedCall
  {
    friend class RestApi;

  private:
    const std::string* data_;

  public:
    const std::string& GetData()
    {
      return *data_;
    }
  };


 
  class RestApiDeleteCall : public RestApiSharedCall
  {
    friend class RestApi;
  };




  class RestApiGetCall : public RestApiSharedCall
  {
    friend class RestApi;

  private:
    const HttpHandler::Arguments* getArguments_;

  public:
    std::string GetArgument(const std::string& name,
                            const std::string& defaultValue)
    {
      return HttpHandler::GetArgument(*getArguments_, name, defaultValue);
    }
  };



  class RestApi : public HttpHandler
  {
  public:
    typedef void (*GetHandler) (RestApiGetCall& call);
    
    typedef void (*DeleteHandler) (RestApiDeleteCall& call);
    
    typedef void (*PutHandler) (RestApiPutCall& call);
    
    typedef void (*PostHandler) (RestApiPostCall& call);
    
  private:
    typedef std::list< std::pair<RestApiPath*, GetHandler> > GetHandlers;
    typedef std::list< std::pair<RestApiPath*, PutHandler> > PutHandlers;
    typedef std::list< std::pair<RestApiPath*, PostHandler> > PostHandlers;
    typedef std::list< std::pair<RestApiPath*, DeleteHandler> > DeleteHandlers;

    // TODO MUTEX BETWEEN CONTEXTS !!!
    std::auto_ptr<IDynamicObject> context_;

    GetHandlers  getHandlers_;
    PutHandlers  putHandlers_;
    PostHandlers  postHandlers_;
    DeleteHandlers  deleteHandlers_;

    bool IsGetAccepted(const UriComponents& uri)
    {
      for (GetHandlers::const_iterator it = getHandlers_.begin();
           it != getHandlers_.end(); it++)
      {
        if (it->first->Match(uri))
        {
          return true;
        }
      }

      return false;
    }

    bool IsPutAccepted(const UriComponents& uri)
    {
      for (PutHandlers::const_iterator it = putHandlers_.begin();
           it != putHandlers_.end(); it++)
      {
        if (it->first->Match(uri))
        {
          return true;
        }
      }

      return false;
    }

    bool IsPostAccepted(const UriComponents& uri)
    {
      for (PostHandlers::const_iterator it = postHandlers_.begin();
           it != postHandlers_.end(); it++)
      {
        if (it->first->Match(uri))
        {
          return true;
        }
      }

      return false;
    }

    bool IsDeleteAccepted(const UriComponents& uri)
    {
      for (DeleteHandlers::const_iterator it = deleteHandlers_.begin();
           it != deleteHandlers_.end(); it++)
      {
        if (it->first->Match(uri))
        {
          return true;
        }
      }

      return false;
    }

    void AddMethod(std::string& target,
                   const std::string& method) const
    {
      if (target.size() > 0)
        target += "," + method;
      else
        target = method;
    }

    std::string  GetAcceptedMethods(const UriComponents& uri)
    {
      std::string s;

      if (IsGetAccepted(uri))
        AddMethod(s, "GET");

      if (IsPutAccepted(uri))
        AddMethod(s, "PUT");

      if (IsPostAccepted(uri))
        AddMethod(s, "POST");

      if (IsDeleteAccepted(uri))
        AddMethod(s, "DELETE");

      return s;
    }

  public:
    RestApi()
    {
    }

    void SetContext(IDynamicObject* context)  // This takes the ownership
    {
      context_.reset(context);
    }

    ~RestApi()
    {
      for (GetHandlers::iterator it = getHandlers_.begin(); 
           it != getHandlers_.end(); it++)
      {
        delete it->first;
      } 

      for (PutHandlers::iterator it = putHandlers_.begin(); 
           it != putHandlers_.end(); it++)
      {
        delete it->first;
      } 

      for (PostHandlers::iterator it = postHandlers_.begin(); 
           it != postHandlers_.end(); it++)
      {
        delete it->first;
      } 

      for (DeleteHandlers::iterator it = deleteHandlers_.begin(); 
           it != deleteHandlers_.end(); it++)
      {
        delete it->first;
      } 
    }

    virtual bool IsServedUri(const UriComponents& uri)
    {
      return (IsGetAccepted(uri) ||
              IsPutAccepted(uri) ||
              IsPostAccepted(uri) ||
              IsDeleteAccepted(uri));
    }

    virtual void Handle(HttpOutput& output,
                        const std::string& method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const Arguments& getArguments,
                        const std::string& postData)
    {
      bool ok = false;
      RestApiOutput restOutput(output);
      RestApiPath::Components components;
      UriComponents trailing;
               
      if (method == "GET")
      {
        for (GetHandlers::const_iterator it = getHandlers_.begin();
             it != getHandlers_.end(); it++)
        {
          if (it->first->Match(components, trailing, uri))
          {
            ok = true;
            RestApiGetCall call;
            call.output_ = &restOutput;
            call.context_ = context_.get();
            call.httpHeaders_ = &headers;
            call.uriComponents_ = &components;
            call.trailing_ = &trailing;
           
            call.getArguments_ = &getArguments;
            it->second(call);
          }
        }
      }
      else if (method == "PUT")
      {
        for (PutHandlers::const_iterator it = putHandlers_.begin();
             it != putHandlers_.end(); it++)
        {
          if (it->first->Match(components, trailing, uri))
          {
            ok = true;
            RestApiPutCall call;
            call.output_ = &restOutput;
            call.context_ = context_.get();
            call.httpHeaders_ = &headers;
            call.uriComponents_ = &components;
            call.trailing_ = &trailing;
           
            call.data_ = &postData;
            it->second(call);
          }
        }
      }
      else if (method == "POST")
      {
        for (PostHandlers::const_iterator it = postHandlers_.begin();
             it != postHandlers_.end(); it++)
        {
          if (it->first->Match(components, trailing, uri))
          {
            ok = true;
            RestApiPostCall call;
            call.output_ = &restOutput;
            call.context_ = context_.get();
            call.httpHeaders_ = &headers;
            call.uriComponents_ = &components;
            call.trailing_ = &trailing;
           
            call.data_ = &postData;
            it->second(call);
          }
        }
      }
      else if (method == "DELETE")
      {
        for (DeleteHandlers::const_iterator it = deleteHandlers_.begin();
             it != deleteHandlers_.end(); it++)
        {
          if (it->first->Match(components, trailing, uri))
          {
            ok = true;
            RestApiDeleteCall call;
            call.output_ = &restOutput;
            call.context_ = context_.get();
            call.httpHeaders_ = &headers;
            call.uriComponents_ = &components;
            call.trailing_ = &trailing;
            it->second(call);
          }
        }
      }

      if (!ok)
      {
        output.SendMethodNotAllowedError(GetAcceptedMethods(uri));
      }
    }

    void Register(const std::string& path,
                  GetHandler handler)
    {
      getHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
    }


    void Register(const std::string& path,
                  PutHandler handler)
    {
      putHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
    }


    void Register(const std::string& path,
                  PostHandler handler)
    {
      postHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
    }


    void Register(const std::string& path,
                  DeleteHandler handler)
    {
      deleteHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
    }

  };

}


TEST(RestApi, RestApiPath)
{
  RestApiPath::Components args;
  UriComponents trail;

  {
    RestApiPath uri("/coucou/{abc}/d/*");
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
    ASSERT_EQ("e", trail[0]);
    ASSERT_EQ("f", trail[1]);
    ASSERT_EQ("g", trail[2]);

    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/f"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/"));
    ASSERT_FALSE(uri.Match(args, trail, "/a/moi/d"));
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi"));
  }

  {
    RestApiPath uri("/coucou/{abc}/d");
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(0u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
  }

  {
    RestApiPath uri("/*");
    ASSERT_TRUE(uri.Match(args, trail, "/a/b/c"));
    ASSERT_EQ(0u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("a", trail[0]);
    ASSERT_EQ("b", trail[1]);
    ASSERT_EQ("c", trail[2]);
  }
}




#include "../Core/HttpServer/MongooseServer.h"

struct Tutu : public IDynamicObject
{
  static void Toto(RestApiGetCall& call)
  {
    printf("DONE\n");
    Json::Value a = Json::objectValue;
    a["Tutu"] = "Toto";
    a["Youpie"] = call.GetArgument("coucou", "nope");
    a["Toto"] = call.GetUriComponent("test", "nope");
    call.GetOutput().AnswerJson(a);
  }
};



TEST(RestApi, Tutu)
{
  MongooseServer httpServer;
  httpServer.SetPortNumber(8042);
  httpServer.Start();

  RestApi* api = new RestApi;
  httpServer.RegisterHandler(api);
  api->Register("/coucou/{test}/a/*", Tutu::Toto);

  httpServer.Start();
  /*LOG(WARNING) << "REST has started";
    Toolbox::ServerBarrier();*/
}
