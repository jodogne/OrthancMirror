/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "OrthancWebDav.h"

#include "../../OrthancFramework/Sources/Compression/ZipReader.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/HttpServer/WebDavStorage.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "Search/DatabaseLookup.h"
#include "ServerContext.h"

#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>


static const char* const BY_PATIENTS = "by-patients";
static const char* const BY_STUDIES = "by-studies";
static const char* const BY_DATES = "by-dates";
static const char* const BY_UIDS = "by-uids";
static const char* const UPLOADS = "uploads";
static const char* const STUDY_INFO = "study.json";
static const char* const SERIES_INFO = "series.json";


namespace Orthanc
{
  static boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::second_clock::universal_time();
  }
  

  static void LookupTime(boost::posix_time::ptime& target,
                         ServerContext& context,
                         const std::string& publicId,
                         ResourceType level,
                         MetadataType metadata)
  {
    std::string value;
    int64_t revision;  // Ignored
    if (context.GetIndex().LookupMetadata(value, revision, publicId, level, metadata))
    {
      try
      {
        target = boost::posix_time::from_iso_string(value);
        return;
      }
      catch (std::exception& e)
      {
      }
    }

    target = GetNow();
  }

  
  class OrthancWebDav::DicomIdentifiersVisitor : public ServerContext::ILookupVisitor
  {
  private:
    ServerContext&  context_;
    bool            isComplete_;
    Collection&     target_;
    ResourceType    level_;

  public:
    DicomIdentifiersVisitor(ServerContext& context,
                            Collection&  target,
                            ResourceType level) :
      context_(context),
      isComplete_(false),
      target_(target),
      level_(level)
    {
    }
      
    virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
    {
      return false;   // (*)
    }
      
    virtual void MarkAsComplete() ORTHANC_OVERRIDE
    {
      isComplete_ = true;  // TODO
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId   /* unused     */,
                       const DicomMap& mainDicomTags,
                       const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
    {
      DicomTag tag(0, 0);
      MetadataType timeMetadata;

      switch (level_)
      {
        case ResourceType_Study:
          tag = DICOM_TAG_STUDY_INSTANCE_UID;
          timeMetadata = MetadataType_LastUpdate;
          break;

        case ResourceType_Series:
          tag = DICOM_TAG_SERIES_INSTANCE_UID;
          timeMetadata = MetadataType_LastUpdate;
          break;
        
        case ResourceType_Instance:
          tag = DICOM_TAG_SOP_INSTANCE_UID;
          timeMetadata = MetadataType_Instance_ReceptionDate;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
        
      std::string s;
      if (mainDicomTags.LookupStringValue(s, tag, false) &&
          !s.empty())
      {
        std::unique_ptr<Resource> resource;

        if (level_ == ResourceType_Instance)
        {
          FileInfo info;
          int64_t revision;  // Ignored
          if (context_.GetIndex().LookupAttachment(info, revision, publicId, FileContentType_Dicom))
          {
            std::unique_ptr<File> f(new File(s + ".dcm"));
            f->SetMimeType(MimeType_Dicom);
            f->SetContentLength(info.GetUncompressedSize());
            resource.reset(f.release());
          }
        }
        else
        {
          resource.reset(new Folder(s));
        }

        if (resource.get() != NULL)
        {
          boost::posix_time::ptime t;
          LookupTime(t, context_, publicId, level_, timeMetadata);
          resource->SetCreationTime(t);
          target_.AddResource(resource.release());
        }
      }
    }
  };

  
  class OrthancWebDav::DicomFileVisitor : public ServerContext::ILookupVisitor
  {
  private:
    ServerContext&  context_;
    bool            success_;
    std::string&    target_;
    boost::posix_time::ptime&  time_;

  public:
    DicomFileVisitor(ServerContext& context,
                     std::string& target,
                     boost::posix_time::ptime& time) :
      context_(context),
      success_(false),
      target_(target),
      time_(time)
    {
    }

    bool IsSuccess() const
    {
      return success_;
    }

    virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
    {
      return false;   // (*)
    }
      
    virtual void MarkAsComplete() ORTHANC_OVERRIDE
    {
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId   /* unused     */,
                       const DicomMap& mainDicomTags,
                       const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
    {
      if (success_)
      {
        success_ = false;  // Two matches => Error
      }
      else
      {
        LookupTime(time_, context_, publicId, ResourceType_Instance, MetadataType_Instance_ReceptionDate);
        context_.ReadDicom(target_, publicId);
        success_ = true;
      }
    }
  };
  

  class OrthancWebDav::OrthancJsonVisitor : public ServerContext::ILookupVisitor
  {
  private:
    ServerContext&  context_;
    bool            success_;
    std::string&    target_;
    ResourceType    level_;

  public:
    OrthancJsonVisitor(ServerContext& context,
                       std::string& target,
                       ResourceType level) :
      context_(context),
      success_(false),
      target_(target),
      level_(level)
    {
    }

    bool IsSuccess() const
    {
      return success_;
    }

    virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
    {
      return false;   // (*)
    }
      
    virtual void MarkAsComplete() ORTHANC_OVERRIDE
    {
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId   /* unused     */,
                       const DicomMap& mainDicomTags,
                       const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
    {
      Json::Value resource;
      std::set<DicomTag> emptyRequestedTags;  // not supported for webdav

      if (context_.ExpandResource(resource, publicId, level_, DicomToJsonFormat_Human, emptyRequestedTags))
      {
        if (success_)
        {
          success_ = false;  // Two matches => Error
        }
        else
        {
          target_ = resource.toStyledString();

          // Replace UNIX newlines with DOS newlines 
          boost::replace_all(target_, "\n", "\r\n");

          success_ = true;
        }
      }
    }
  };


  class OrthancWebDav::ResourcesIndex : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>   Map;

  private:
    ServerContext&  context_;
    ResourceType    level_;
    std::string     template_;
    Map             pathToResource_;
    Map             resourceToPath_;

    void CheckInvariants()
    {
#ifndef NDEBUG
      assert(pathToResource_.size() == resourceToPath_.size());

      for (Map::const_iterator it = pathToResource_.begin(); it != pathToResource_.end(); ++it)
      {
        assert(resourceToPath_[it->second] == it->first);
      }

      for (Map::const_iterator it = resourceToPath_.begin(); it != resourceToPath_.end(); ++it)
      {
        assert(pathToResource_[it->second] == it->first);
      }
#endif
    }      

    void AddTags(DicomMap& target,
                 const std::string& resourceId,
                 ResourceType tagsFromLevel)
    {
      DicomMap tags;
      if (context_.GetIndex().GetMainDicomTags(tags, resourceId, level_, tagsFromLevel))
      {
        target.Merge(tags);
      }
    }

    void Register(const std::string& resourceId)
    {
      // Don't register twice the same resource
      if (resourceToPath_.find(resourceId) == resourceToPath_.end())
      {
        std::string name = template_;

        DicomMap tags;

        AddTags(tags, resourceId, level_);
        
        if (level_ == ResourceType_Study)
        {
          AddTags(tags, resourceId, ResourceType_Patient);
        }
        
        DicomArray arr(tags);
        for (size_t i = 0; i < arr.GetSize(); i++)
        {
          const DicomElement& element = arr.GetElement(i);
          if (!element.GetValue().IsNull() &&
              !element.GetValue().IsBinary())
          {
            const std::string tag = FromDcmtkBridge::GetTagName(element.GetTag(), "");
            boost::replace_all(name, "{{" + tag + "}}", element.GetValue().GetContent());
          } 
        }

        // Blank the tags that were not matched
        static const boost::regex REGEX_BLANK_TAGS("{{.*?}}");  // non-greedy match
        name = boost::regex_replace(name, REGEX_BLANK_TAGS, "");

        // UTF-8 characters cannot be used on Windows XP
        name = Toolbox::ConvertToAscii(name);
        boost::replace_all(name, "/", "");
        boost::replace_all(name, "\\", "");

        // Trim sequences of spaces as one single space
        static const boost::regex REGEX_TRIM_SPACES("{{.*?}}");
        name = boost::regex_replace(name, REGEX_TRIM_SPACES, " ");
        name = Toolbox::StripSpaces(name);

        size_t count = 0;
        for (;;)
        {
          std::string path = name;
          if (count > 0)
          {
            path += " (" + boost::lexical_cast<std::string>(count) + ")";
          }

          if (pathToResource_.find(path) == pathToResource_.end())
          {
            pathToResource_[path] = resourceId;
            resourceToPath_[resourceId] = path;
            return;
          }

          count++;
        }

        throw OrthancException(ErrorCode_InternalError);
      }
    }

  public:
    ResourcesIndex(ServerContext& context,
                   ResourceType level,
                   const std::string& templateString) :
      context_(context),
      level_(level),
      template_(templateString)
    {
    }

    ResourceType GetLevel() const
    {
      return level_;
    }

    void Refresh(std::set<std::string>& removedPaths /* out */,
                 const std::set<std::string>& resources)
    {
      CheckInvariants();

      // Detect the resources that have been removed since last refresh
      removedPaths.clear();
      std::set<std::string> removedResources;
      
      for (Map::iterator it = resourceToPath_.begin(); it != resourceToPath_.end(); ++it)
      {
        if (resources.find(it->first) == resources.end())
        {
          const std::string& path = it->second;
          
          assert(pathToResource_.find(path) != pathToResource_.end());
          pathToResource_.erase(path);
          removedPaths.insert(path);
          
          removedResources.insert(it->first);  // Delay the removal to avoid disturbing the iterator
        }
      }

      // Remove the missing resources
      for (std::set<std::string>::const_iterator it = removedResources.begin(); it != removedResources.end(); ++it)
      {
        assert(resourceToPath_.find(*it) != resourceToPath_.end());
        resourceToPath_.erase(*it);
      }

      CheckInvariants();

      for (std::set<std::string>::const_iterator it = resources.begin(); it != resources.end(); ++it)
      {
        Register(*it);
      }

      CheckInvariants();
    }

    const Map& GetPathToResource() const
    {
      return pathToResource_;
    }
  };


  class OrthancWebDav::InstancesOfSeries : public INode
  {
  private:
    ServerContext&  context_;
    std::string     parentSeries_;

    static bool LookupInstanceId(std::string& instanceId,
                                 const UriComponents& path)
    {
      if (path.size() == 1 &&
          boost::ends_with(path[0], ".dcm"))
      {
        instanceId = path[0].substr(0, path[0].size() - 4);
        return true;
      }
      else
      {
        return false;
      }
    }

  public:
    InstancesOfSeries(ServerContext& context,
                      const std::string& parentSeries) :
      context_(context),
      parentSeries_(parentSeries)
    {
    }

    virtual bool ListCollection(IWebDavBucket::Collection& target,
                                const UriComponents& path) ORTHANC_OVERRIDE
    {
      if (path.empty())
      {
        std::list<std::string> resources;
        try
        {
          context_.GetIndex().GetChildren(resources, parentSeries_);
        }
        catch (OrthancException&)
        {
          // Unknown (or deleted) parent series
          return false;
        }

        for (std::list<std::string>::const_iterator
               it = resources.begin(); it != resources.end(); ++it)
        {
          boost::posix_time::ptime time;
          LookupTime(time, context_, *it, ResourceType_Instance, MetadataType_Instance_ReceptionDate);

          FileInfo info;
          int64_t revision;  // Ignored
          if (context_.GetIndex().LookupAttachment(info, revision, *it, FileContentType_Dicom))
          {
            std::unique_ptr<File> resource(new File(*it + ".dcm"));
            resource->SetMimeType(MimeType_Dicom);
            resource->SetContentLength(info.GetUncompressedSize());
            resource->SetCreationTime(time);
            target.AddResource(resource.release());
          }          
        }
        
        return true;
      }
      else
      {
        return false;
      }
    }

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& time, 
                                const UriComponents& path) ORTHANC_OVERRIDE
    {
      std::string instanceId;
      if (LookupInstanceId(instanceId, path))
      {
        try
        {
          mime = MimeType_Dicom;
          context_.ReadDicom(content, instanceId);
          LookupTime(time, context_, instanceId, ResourceType_Instance, MetadataType_Instance_ReceptionDate);
          return true;
        }
        catch (OrthancException&)
        {
          // File was removed
          return false;
        }
      }
      else
      {
        return false;
      }
    }

    virtual bool DeleteItem(const UriComponents& path) ORTHANC_OVERRIDE
    {
      if (path.empty())
      {
        // Delete all
        std::list<std::string> resources;
        try
        {
          context_.GetIndex().GetChildren(resources, parentSeries_);
        }
        catch (OrthancException&)
        {
          // Unknown (or deleted) parent series
          return true;
        }

        for (std::list<std::string>::const_iterator it = resources.begin();
             it != resources.end(); ++it)
        {
          Json::Value info;
          context_.DeleteResource(info, *it, ResourceType_Instance);
        }

        return true;
      }
      else
      {
        std::string instanceId;
        if (LookupInstanceId(instanceId, path))
        {
          Json::Value info;
          return context_.DeleteResource(info, instanceId, ResourceType_Instance);
        }
        else
        {
          return false;
        }
      }
    }
  };



  /**
   * The "InternalNode" class corresponds to a non-leaf node in the
   * WebDAV tree, that only contains subfolders (no file).
   * 
   * TODO: Implement a LRU index to dynamically remove the oldest
   * children on high RAM usage.
   **/
  class OrthancWebDav::InternalNode : public INode
  {
  private:
    typedef std::map<std::string, INode*>  Children;

    Children  children_;

    INode* GetChild(const std::string& path)  // Don't delete the result pointer!
    {
      Children::const_iterator child = children_.find(path);
      if (child == children_.end())
      {
        INode* node = CreateSubfolder(path);
        
        if (node == NULL)
        {
          return NULL;
        }
        else
        {
          children_[path] = node;
          return node;
        }
      }
      else
      {
        assert(child->second != NULL);
        return child->second;
      }
    }

  protected:
    void InvalidateSubfolder(const std::string& path)
    {
      Children::iterator child = children_.find(path);
      if (child != children_.end())
      {
        assert(child->second != NULL);
        delete child->second;
        children_.erase(child);
      }
    }
    
    virtual void Refresh() = 0;
    
    virtual bool ListSubfolders(IWebDavBucket::Collection& target) = 0;
    
    virtual INode* CreateSubfolder(const std::string& path) = 0;

  public:
    virtual ~InternalNode()
    {
      for (Children::iterator it = children_.begin(); it != children_.end(); ++it)
      {
        assert(it->second != NULL);
        delete it->second;
      }
    }

    virtual bool ListCollection(IWebDavBucket::Collection& target,
                                const UriComponents& path)
      ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      Refresh();
      
      if (path.empty())
      {
        return ListSubfolders(target);
      }
      else
      {
        // Recursivity
        INode* child = GetChild(path[0]);
        if (child == NULL)
        {
          // Must be "true" to allow DELETE on folders that are
          // automatically removed through recursive deletion
          return true;
        }
        else
        {
          UriComponents subpath(path.begin() + 1, path.end());
          return child->ListCollection(target, subpath);
        }
      }
    }

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& time, 
                                const UriComponents& path)
      ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      if (path.empty())
      {
        return false;  // An internal node doesn't correspond to a file
      }
      else
      {
        // Recursivity
        Refresh();
      
        INode* child = GetChild(path[0]);
        if (child == NULL)
        {
          return false;
        }
        else
        {
          UriComponents subpath(path.begin() + 1, path.end());
          return child->GetFileContent(mime, content, time, subpath);
        }
      }
    }


    virtual bool DeleteItem(const UriComponents& path) ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      Refresh();

      if (path.empty())
      {
        IWebDavBucket::Collection collection;
        if (ListSubfolders(collection))
        {
          std::set<std::string> content;
          collection.ListDisplayNames(content);

          for (std::set<std::string>::const_iterator
                 it = content.begin(); it != content.end(); ++it)
          {
            INode* node = GetChild(*it);
            if (node)
            {
              node->DeleteItem(path);
            }
          }

          return true;
        }
        else
        {
          return false;
        }
      }
      else
      {
        INode* child = GetChild(path[0]);
        if (child == NULL)
        {
          return true;
        }
        else
        {
          // Recursivity
          UriComponents subpath(path.begin() + 1, path.end());
          return child->DeleteItem(subpath);
        }
      }
    }
  };
  

  class OrthancWebDav::ListOfResources : public InternalNode
  {
  private:
    ServerContext&                   context_;
    const Templates&                 templates_;
    std::unique_ptr<ResourcesIndex>  index_;
    MetadataType                     timeMetadata_;

  protected:
    virtual void Refresh() ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      std::list<std::string> resources;
      GetCurrentResources(resources);

      std::set<std::string> removedPaths;
      index_->Refresh(removedPaths, std::set<std::string>(resources.begin(), resources.end()));

      // Remove the children whose associated resource doesn't exist anymore
      for (std::set<std::string>::const_iterator
             it = removedPaths.begin(); it != removedPaths.end(); ++it)
      {
        InvalidateSubfolder(*it);
      }
    }

    virtual bool ListSubfolders(IWebDavBucket::Collection& target) ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      if (index_->GetLevel() == ResourceType_Instance)
      {
        // Not a collection, no subfolders
        return false;
      }
      else
      {
        const ResourcesIndex::Map& paths = index_->GetPathToResource();
        
        for (ResourcesIndex::Map::const_iterator it = paths.begin(); it != paths.end(); ++it)
        {
          boost::posix_time::ptime time;
          LookupTime(time, context_, it->second, index_->GetLevel(), timeMetadata_);

          std::unique_ptr<IWebDavBucket::Resource> resource(new IWebDavBucket::Folder(it->first));
          resource->SetCreationTime(time);
          target.AddResource(resource.release());
        }

        return true;
      }
    }

    virtual INode* CreateSubfolder(const std::string& path) ORTHANC_OVERRIDE ORTHANC_FINAL
    {
      ResourcesIndex::Map::const_iterator resource = index_->GetPathToResource().find(path);
      if (resource == index_->GetPathToResource().end())
      {
        return NULL;
      }
      else
      {
        return CreateResourceNode(resource->second);
      }
    }

    ServerContext& GetContext() const
    {
      return context_;
    }

    virtual void GetCurrentResources(std::list<std::string>& resources) = 0;

    virtual INode* CreateResourceNode(const std::string& resource) = 0;
    
  public:
    ListOfResources(ServerContext& context,
                    ResourceType level,
                    const Templates& templates) :
      context_(context),
      templates_(templates)
    {
      Templates::const_iterator t = templates.find(level);
      if (t == templates.end())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      
      index_.reset(new ResourcesIndex(context, level, t->second));
      
      if (level == ResourceType_Instance)
      {
        timeMetadata_ = MetadataType_Instance_ReceptionDate;
      }
      else
      {
        timeMetadata_ = MetadataType_LastUpdate;
      }
    }

    ResourceType GetLevel() const
    {
      return index_->GetLevel();
    }

    const Templates& GetTemplates() const
    {
      return templates_;
    }
  };

  

  class OrthancWebDav::SingleDicomResource : public ListOfResources
  {
  private:
    std::string  parentId_;
    
  protected: 
    virtual void GetCurrentResources(std::list<std::string>& resources) ORTHANC_OVERRIDE
    {
      try
      {
        GetContext().GetIndex().GetChildren(resources, parentId_);
      }
      catch (OrthancException&)
      {
        // Unknown parent resource
        resources.clear();
      }
    }

    virtual INode* CreateResourceNode(const std::string& resource) ORTHANC_OVERRIDE
    {
      if (GetLevel() == ResourceType_Instance)
      {
        return NULL;
      }
      else if (GetLevel() == ResourceType_Series)
      {
        return new InstancesOfSeries(GetContext(), resource);
      }
      else
      {
        ResourceType l = GetChildResourceType(GetLevel());
        return new SingleDicomResource(GetContext(), l, resource, GetTemplates());
      }
    }

  public:
    SingleDicomResource(ServerContext& context,
                        ResourceType level,
                        const std::string& parentId,
                        const Templates& templates) :
      ListOfResources(context, level, templates),
      parentId_(parentId)
    {
    }
  };
  
  
  class OrthancWebDav::RootNode : public ListOfResources
  {
  protected:   
    virtual void GetCurrentResources(std::list<std::string>& resources) ORTHANC_OVERRIDE
    {
      GetContext().GetIndex().GetAllUuids(resources, GetLevel());
    }

    virtual INode* CreateResourceNode(const std::string& resource) ORTHANC_OVERRIDE
    {
      if (GetLevel() == ResourceType_Series)
      {
        return new InstancesOfSeries(GetContext(), resource);
      }
      else
      {
        ResourceType l = GetChildResourceType(GetLevel());
        return new SingleDicomResource(GetContext(), l, resource, GetTemplates());
      }
    }

  public:
    RootNode(ServerContext& context,
             ResourceType level,
             const Templates& templates) :
      ListOfResources(context, level, templates)
    {
    }
  };


  class OrthancWebDav::ListOfStudiesByDate : public ListOfResources
  {
  private:
    std::string  year_;
    std::string  month_;

    class Visitor : public ServerContext::ILookupVisitor
    {
    private:
      std::list<std::string>&  resources_;

    public:
      explicit Visitor(std::list<std::string>& resources) :
        resources_(resources)
      {
      }

      virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
      {
        return false;   // (*)
      }
      
      virtual void MarkAsComplete() ORTHANC_OVERRIDE
      {
      }

      virtual void Visit(const std::string& publicId,
                         const std::string& instanceId   /* unused     */,
                         const DicomMap& mainDicomTags,
                         const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
      {
        resources_.push_back(publicId);
      }
    };
    
  protected:   
    virtual void GetCurrentResources(std::list<std::string>& resources) ORTHANC_OVERRIDE
    {
      DatabaseLookup query;
      query.AddRestConstraint(DICOM_TAG_STUDY_DATE, year_ + month_ + "01-" + year_ + month_ + "31",
                              true /* case sensitive */, true /* mandatory tag */);

      Visitor visitor(resources);
      GetContext().Apply(visitor, query, ResourceType_Study, 0 /* since */, 0 /* no limit */);
    }

    virtual INode* CreateResourceNode(const std::string& resource) ORTHANC_OVERRIDE
    {
      return new SingleDicomResource(GetContext(), ResourceType_Series, resource, GetTemplates());
    }

  public:
    ListOfStudiesByDate(ServerContext& context,
                        const std::string& year,
                        const std::string& month,
                        const Templates& templates) :
      ListOfResources(context, ResourceType_Study, templates),
      year_(year),
      month_(month)
    {
      if (year.size() != 4 ||
          month.size() != 2)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  };


  class OrthancWebDav::ListOfStudiesByMonth : public InternalNode
  {
  private:
    ServerContext&    context_;
    std::string       year_;
    const Templates&  templates_;

    class Visitor : public ServerContext::ILookupVisitor
    {
    private:
      std::set<std::string> months_;
      
    public:
      const std::set<std::string>& GetMonths() const
      {
        return months_;
      }
      
      virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
      {
        return false;   // (*)
      }
      
      virtual void MarkAsComplete() ORTHANC_OVERRIDE
      {
      }

      virtual void Visit(const std::string& publicId,
                         const std::string& instanceId   /* unused     */,
                         const DicomMap& mainDicomTags,
                         const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
      {
        std::string s;
        if (mainDicomTags.LookupStringValue(s, DICOM_TAG_STUDY_DATE, false) &&
            s.size() == 8)
        {
          months_.insert(s.substr(4, 2)); // Get the month from "YYYYMMDD"
        }
      }
    };

  protected:
    virtual void Refresh() ORTHANC_OVERRIDE
    {
    }

    virtual bool ListSubfolders(IWebDavBucket::Collection& target) ORTHANC_OVERRIDE
    {
      DatabaseLookup query;
      query.AddRestConstraint(DICOM_TAG_STUDY_DATE, year_ + "0101-" + year_ + "1231",
                              true /* case sensitive */, true /* mandatory tag */);

      Visitor visitor;
      context_.Apply(visitor, query, ResourceType_Study, 0 /* since */, 0 /* no limit */);

      for (std::set<std::string>::const_iterator it = visitor.GetMonths().begin();
           it != visitor.GetMonths().end(); ++it)
      {
        target.AddResource(new IWebDavBucket::Folder(year_ + "-" + *it));
      }

      return true;
    }

    virtual INode* CreateSubfolder(const std::string& path) ORTHANC_OVERRIDE
    {
      if (path.size() != 7)  // Format: "YYYY-MM"
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        const std::string year = path.substr(0, 4);
        const std::string month = path.substr(5, 2);
        return new ListOfStudiesByDate(context_, year, month, templates_);
      }
    }

  public:
    ListOfStudiesByMonth(ServerContext& context,
                         const std::string& year,
                         const Templates& templates) :
      context_(context),
      year_(year),
      templates_(templates)
    {
      if (year_.size() != 4)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  };

  
  class OrthancWebDav::ListOfStudiesByYear : public InternalNode
  {
  private:
    ServerContext&    context_;
    const Templates&  templates_;

  protected:
    virtual void Refresh() ORTHANC_OVERRIDE
    {
    }

    virtual bool ListSubfolders(IWebDavBucket::Collection& target) ORTHANC_OVERRIDE
    {
      std::list<std::string> resources;
      context_.GetIndex().GetAllUuids(resources, ResourceType_Study);

      std::set<std::string> years;
      
      for (std::list<std::string>::const_iterator it = resources.begin(); it != resources.end(); ++it)
      {
        DicomMap tags;
        std::string studyDate;
        if (context_.GetIndex().GetMainDicomTags(tags, *it, ResourceType_Study, ResourceType_Study) &&
            tags.LookupStringValue(studyDate, DICOM_TAG_STUDY_DATE, false) &&
            studyDate.size() == 8)
        {
          years.insert(studyDate.substr(0, 4)); // Get the year from "YYYYMMDD"
        }
      }
      
      for (std::set<std::string>::const_iterator it = years.begin(); it != years.end(); ++it)
      {
        target.AddResource(new IWebDavBucket::Folder(*it));
      }

      return true;
    }

    virtual INode* CreateSubfolder(const std::string& path) ORTHANC_OVERRIDE
    {
      return new ListOfStudiesByMonth(context_, path, templates_);
    }

  public:
    ListOfStudiesByYear(ServerContext& context,
                        const Templates& templates) :
      context_(context),
      templates_(templates)
    {
    }
  };


  class OrthancWebDav::DicomDeleteVisitor : public ServerContext::ILookupVisitor
  {
  private:
    ServerContext&  context_;
    ResourceType    level_;

  public:
    DicomDeleteVisitor(ServerContext& context,
                       ResourceType level) :
      context_(context),
      level_(level)
    {
    }

    virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
    {
      return false;   // (*)
    }
      
    virtual void MarkAsComplete() ORTHANC_OVERRIDE
    {
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId   /* unused     */,
                       const DicomMap& mainDicomTags   /* unused     */,
                       const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
    {
      Json::Value info;
      context_.DeleteResource(info, publicId, level_);
    }
  };
  

  void OrthancWebDav::AddVirtualFile(Collection& collection,
                                     const UriComponents& path,
                                     const std::string& filename)
  {
    MimeType mime;
    std::string content;
    boost::posix_time::ptime modification;  // Unused, let the date be set to "GetNow()"

    UriComponents p = path;
    p.push_back(filename);

    if (GetFileContent(mime, content, modification, p))
    {
      std::unique_ptr<File> f(new File(filename));
      f->SetMimeType(mime);
      f->SetContentLength(content.size());
      collection.AddResource(f.release());
    }
  }


  void OrthancWebDav::UploadWorker(OrthancWebDav* that)
  {
    assert(that != NULL);

    boost::posix_time::ptime lastModification = GetNow();

    while (that->uploadRunning_)
    {
      std::unique_ptr<IDynamicObject> obj(that->uploadQueue_.Dequeue(100));
      if (obj.get() != NULL)
      {
        that->Upload(reinterpret_cast<const SingleValueObject<std::string>&>(*obj).GetValue());
        lastModification = GetNow();
      }
      else if (GetNow() - lastModification > boost::posix_time::seconds(30))
      {
        /**
         * After every 30 seconds of inactivity, remove the empty
         * folders. This delay is needed to avoid removing
         * just-created folders before the remote WebDAV has time to
         * write files into it.
         **/
        LOG(TRACE) << "Cleaning up the empty WebDAV upload folders";
        that->uploads_.RemoveEmptyFolders();
        lastModification = GetNow();
      }
    }
  }

  
  void OrthancWebDav::Upload(const std::string& path)
  {
    UriComponents uri;
    Toolbox::SplitUriComponents(uri, path);
        
    LOG(INFO) << "Upload from WebDAV: " << path;

    MimeType mime;
    std::string content;
    boost::posix_time::ptime time;
    if (uploads_.GetFileContent(mime, content, time, uri))
    {
      bool success = false;

      if (ZipReader::IsZipMemoryBuffer(content))
      {
        // New in Orthanc 1.8.2
        std::unique_ptr<ZipReader> reader(ZipReader::CreateFromMemory(content));

        std::string filename, uncompressedFile;
        while (reader->ReadNextFile(filename, uncompressedFile))
        {
          if (!uncompressedFile.empty())
          {
            LOG(INFO) << "Uploading DICOM file extracted from a ZIP archive in WebDAV: " << filename;
          
            std::unique_ptr<DicomInstanceToStore> instance(DicomInstanceToStore::CreateFromBuffer(uncompressedFile));
            instance->SetOrigin(DicomInstanceOrigin::FromWebDav());

            std::string publicId;

            try
            {
              context_.Store(publicId, *instance, StoreInstanceMode_Default);
            }
            catch (OrthancException& e)
            {
              if (e.GetErrorCode() == ErrorCode_BadFileFormat)
              {
                LOG(ERROR) << "Cannot import non-DICOM file from ZIP archive: " << filename;
              }
            }
          }
        }

        success = true;
      }
      else
      {
        std::unique_ptr<DicomInstanceToStore> instance(DicomInstanceToStore::CreateFromBuffer(content));
        instance->SetOrigin(DicomInstanceOrigin::FromWebDav());

        try
        {
          std::string publicId;
          ServerContext::StoreResult result = context_.Store(publicId, *instance, StoreInstanceMode_Default);
          if (result.GetStatus() == StoreStatus_Success ||
              result.GetStatus() == StoreStatus_AlreadyStored)
          {
            LOG(INFO) << "Successfully imported DICOM instance from WebDAV: "
                      << path << " (Orthanc ID: " << publicId << ")";
            success = true;
          }
        }
        catch (OrthancException& e)
        {
        }
      }

      uploads_.DeleteItem(uri);

      if (!success)
      {
        LOG(WARNING) << "Cannot import DICOM instance from WebWAV (maybe not a DICOM file): " << path;
      }
    }
  }


  OrthancWebDav::INode& OrthancWebDav::GetRootNode(const std::string& rootPath)
  {
    if (rootPath == BY_PATIENTS)
    {
      return *patients_;
    }
    else if (rootPath == BY_STUDIES)
    {
      return *studies_;
    }
    else if (rootPath == BY_DATES)
    {
      return *dates_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
  

  OrthancWebDav::OrthancWebDav(ServerContext& context,
                               bool allowDicomDelete,
                               bool allowUpload) :
    context_(context),
    allowDicomDelete_(allowDicomDelete),
    allowUpload_(allowUpload),
    uploads_(false /* store uploads as temporary files */),
    uploadRunning_(false)
  {
    patientsTemplates_[ResourceType_Patient] = "{{PatientID}} - {{PatientName}}";
    patientsTemplates_[ResourceType_Study] = "{{StudyDate}} - {{StudyDescription}}";
    patientsTemplates_[ResourceType_Series] = "{{Modality}} - {{SeriesDescription}}";

    studiesTemplates_[ResourceType_Study] = "{{PatientID}} - {{PatientName}} - {{StudyDescription}}";
    studiesTemplates_[ResourceType_Series] = patientsTemplates_[ResourceType_Series];

    patients_.reset(new RootNode(context, ResourceType_Patient, patientsTemplates_));
    studies_.reset(new RootNode(context, ResourceType_Study, studiesTemplates_));
    dates_.reset(new ListOfStudiesByYear(context, studiesTemplates_));
  }


  bool OrthancWebDav::IsExistingFolder(const UriComponents& path) 
  {
    if (path.empty())
    {
      return true;
    }
    else if (path[0] == BY_UIDS)
    {
      return (path.size() <= 3 &&
              (path.size() != 3 || path[2] != STUDY_INFO));
    }
    else if (path[0] == BY_PATIENTS ||
             path[0] == BY_STUDIES ||
             path[0] == BY_DATES)
    {
      IWebDavBucket::Collection collection;
      return GetRootNode(path[0]).ListCollection(collection, UriComponents(path.begin() + 1, path.end()));
    }
    else if (allowUpload_ &&
             path[0] == UPLOADS)
    {
      return uploads_.IsExistingFolder(UriComponents(path.begin() + 1, path.end()));
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancWebDav::ListCollection(Collection& collection,
                                     const UriComponents& path) 
  {
    if (path.empty())
    {
      collection.AddResource(new Folder(BY_DATES));
      collection.AddResource(new Folder(BY_PATIENTS));
      collection.AddResource(new Folder(BY_STUDIES));
      collection.AddResource(new Folder(BY_UIDS));

      if (allowUpload_)
      {
        collection.AddResource(new Folder(UPLOADS));
      }
      
      return true;
    }   
    else if (path[0] == BY_UIDS)
    {
      DatabaseLookup query;
      ResourceType level;
      size_t limit = 0;  // By default, no limits

      if (path.size() == 1)
      {
        level = ResourceType_Study;
        limit = 0;  // TODO - Should we limit here?
      }
      else if (path.size() == 2)
      {
        AddVirtualFile(collection, path, STUDY_INFO);

        level = ResourceType_Series;
        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
      }      
      else if (path.size() == 3)
      {
        AddVirtualFile(collection, path, SERIES_INFO);

        level = ResourceType_Instance;
        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
        query.AddRestConstraint(DICOM_TAG_SERIES_INSTANCE_UID, path[2],
                                true /* case sensitive */, true /* mandatory tag */);
      }
      else
      {
        return false;
      }

      DicomIdentifiersVisitor visitor(context_, collection, level);
      context_.Apply(visitor, query, level, 0 /* since */, limit);
      
      return true;
    }
    else if (path[0] == BY_PATIENTS ||
             path[0] == BY_STUDIES ||
             path[0] == BY_DATES)
    {
      return GetRootNode(path[0]).ListCollection(collection, UriComponents(path.begin() + 1, path.end()));
    }
    else if (allowUpload_ &&
             path[0] == UPLOADS)
    {
      return uploads_.ListCollection(collection, UriComponents(path.begin() + 1, path.end()));
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancWebDav::GetFileContent(MimeType& mime,
                                     std::string& content,
                                     boost::posix_time::ptime& modificationTime, 
                                     const UriComponents& path) 
  {
    if (path.empty())
    {
      return false;
    }
    else if (path[0] == BY_UIDS)
    {
      if (path.size() == 3 &&
          path[2] == STUDY_INFO)
      {
        DatabaseLookup query;
        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
      
        OrthancJsonVisitor visitor(context_, content, ResourceType_Study);
        context_.Apply(visitor, query, ResourceType_Study, 0 /* since */, 0 /* no limit */);

        mime = MimeType_Json;
        return visitor.IsSuccess();
      }
      else if (path.size() == 4 &&
               path[3] == SERIES_INFO)
      {
        DatabaseLookup query;
        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
        query.AddRestConstraint(DICOM_TAG_SERIES_INSTANCE_UID, path[2],
                                true /* case sensitive */, true /* mandatory tag */);
      
        OrthancJsonVisitor visitor(context_, content, ResourceType_Series);
        context_.Apply(visitor, query, ResourceType_Series, 0 /* since */, 0 /* no limit */);

        mime = MimeType_Json;
        return visitor.IsSuccess();
      }
      else if (path.size() == 4 &&
               boost::ends_with(path[3], ".dcm"))
      {
        const std::string sopInstanceUid = path[3].substr(0, path[3].size() - 4);
        
        DatabaseLookup query;
        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
        query.AddRestConstraint(DICOM_TAG_SERIES_INSTANCE_UID, path[2],
                                true /* case sensitive */, true /* mandatory tag */);
        query.AddRestConstraint(DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUid,
                                true /* case sensitive */, true /* mandatory tag */);
      
        DicomFileVisitor visitor(context_, content, modificationTime);
        context_.Apply(visitor, query, ResourceType_Instance, 0 /* since */, 0 /* no limit */);
        
        mime = MimeType_Dicom;
        return visitor.IsSuccess();
      }
      else
      {
        return false;
      }
    }
    else if (path[0] == BY_PATIENTS ||
             path[0] == BY_STUDIES ||
             path[0] == BY_DATES)
    {
      return GetRootNode(path[0]).GetFileContent(mime, content, modificationTime, UriComponents(path.begin() + 1, path.end()));
    }
    else if (allowUpload_ &&
             path[0] == UPLOADS)
    {
      return uploads_.GetFileContent(mime, content, modificationTime, UriComponents(path.begin() + 1, path.end()));
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancWebDav::StoreFile(const std::string& content,
                                const UriComponents& path) 
  {
    if (allowUpload_ &&
        path.size() >= 1 &&
        path[0] == UPLOADS)
    {
      UriComponents subpath(UriComponents(path.begin() + 1, path.end()));

      if (uploads_.StoreFile(content, subpath))
      {
        if (!content.empty())
        {
          uploadQueue_.Enqueue(new SingleValueObject<std::string>(Toolbox::FlattenUri(subpath)));
        }
        return true;
      }
      else
      {
        return false;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancWebDav::CreateFolder(const UriComponents& path)
  {
    if (allowUpload_ &&
        path.size() >= 1 &&
        path[0] == UPLOADS)
    {
      return uploads_.CreateFolder(UriComponents(path.begin() + 1, path.end()));
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancWebDav::DeleteItem(const std::vector<std::string>& path) 
  {
    if (path.empty())
    {
      return false;
    }
    else if (path[0] == BY_UIDS &&
             path.size() >= 2 &&
             path.size() <= 4)
    {
      if (allowDicomDelete_)
      {
        ResourceType level;
        DatabaseLookup query;

        query.AddRestConstraint(DICOM_TAG_STUDY_INSTANCE_UID, path[1],
                                true /* case sensitive */, true /* mandatory tag */);
        level = ResourceType_Study;

        if (path.size() >= 3)
        {
          if (path[2] == STUDY_INFO)
          {
            return true;  // Allow deletion of virtual files (to avoid blocking recursive DELETE)
          }
          
          query.AddRestConstraint(DICOM_TAG_SERIES_INSTANCE_UID, path[2],
                                  true /* case sensitive */, true /* mandatory tag */);
          level = ResourceType_Series;
        }

        if (path.size() == 4)
        {
          if (path[3] == SERIES_INFO)
          {
            return true;  // Allow deletion of virtual files (to avoid blocking recursive DELETE)
          }
          else if (boost::ends_with(path[3], ".dcm"))
          {
            const std::string sopInstanceUid = path[3].substr(0, path[3].size() - 4);

            query.AddRestConstraint(DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUid,
                                    true /* case sensitive */, true /* mandatory tag */);
            level = ResourceType_Instance;
          }
          else
          {
            return false;
          }
        }

        DicomDeleteVisitor visitor(context_, level);
        context_.Apply(visitor, query, level, 0 /* since */, 0 /* no limit */);
        return true;
      }
      else
      {
        return false;  // read-only
      }
    }
    else if (path[0] == BY_PATIENTS ||
             path[0] == BY_STUDIES ||
             path[0] == BY_DATES)
    {
      if (allowDicomDelete_)
      {
        return GetRootNode(path[0]).DeleteItem(UriComponents(path.begin() + 1, path.end()));
      }
      else
      {
        return false;  // read-only
      }
    }
    else if (allowUpload_ &&
             path[0] == UPLOADS)
    {
      return uploads_.DeleteItem(UriComponents(path.begin() + 1, path.end()));
    }
    else
    {
      return false;
    }
  }

  
  void OrthancWebDav::Start() 
  {
    if (uploadRunning_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (allowUpload_)
    {
      LOG(INFO) << "Starting the WebDAV upload thread";
      uploadRunning_ = true;
      uploadThread_ = boost::thread(UploadWorker, this);
    }
  }

  
  void OrthancWebDav::Stop() 
  {
    if (uploadRunning_)
    {
      LOG(INFO) << "Stopping the WebDAV upload thread";
      uploadRunning_ = false;
      if (uploadThread_.joinable())
      {
        uploadThread_.join();
      }
    }
  }
}
