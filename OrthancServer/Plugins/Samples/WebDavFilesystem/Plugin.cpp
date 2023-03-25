/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/thread/mutex.hpp>


class Resource : public boost::noncopyable
{
private:
  boost::posix_time::ptime  dateTime_;

public:
  Resource() :
    dateTime_(boost::posix_time::second_clock::universal_time())      
  {
  }
    
  virtual ~Resource()
  {
  }

  const boost::posix_time::ptime& GetDateTime() const
  {
    return dateTime_;
  }

  virtual bool IsFolder() const = 0;

  virtual Resource* LookupPath(const std::vector<std::string>& path) = 0;
};


class File : public Resource
{
private:
  std::string  content_;
    
public:
  File(const void* data,
       size_t size) :
    content_(reinterpret_cast<const char*>(data), size)
  {
  }

  const std::string& GetContent() const
  {
    return content_;
  }

  virtual bool IsFolder() const
  {
    return false;
  }
    
  virtual Resource* LookupPath(const std::vector<std::string>& path)
  {
    if (path.empty())
    {
      return this;
    }
    else
    {
      return NULL;
    }
  }
};


class Folder : public Resource
{
private:
  typedef std::map<std::string, Resource*>  Content;

  Content content_;

public:
  virtual ~Folder()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }

  virtual bool IsFolder() const
  {
    return true;
  }

  virtual Resource* LookupPath(const std::vector<std::string>& path)
  {
    if (path.empty())
    {
      return this;
    }
    else
    {
      Content::const_iterator found = content_.find(path[0]);
      if (found == content_.end())
      {
        return NULL;
      }
      else
      {          
        std::vector<std::string> childPath(path.size() - 1);
          
        for (size_t i = 0; i < childPath.size(); i++)
        {
          childPath[i] = path[i + 1];
        }
          
        return found->second->LookupPath(childPath);
      }
    }
  }

  void ListContent(std::list<OrthancPlugins::IWebDavCollection::FileInfo>& files,
                   std::list<OrthancPlugins::IWebDavCollection::FolderInfo>& subfolders) const
  {
    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);

      const std::string dateTime = boost::posix_time::to_iso_string(it->second->GetDateTime());
        
      if (it->second->IsFolder())
      {
        subfolders.push_back(OrthancPlugins::IWebDavCollection::FolderInfo(it->first, dateTime));
      }
      else
      {
        const File& f = dynamic_cast<const File&>(*it->second);
        files.push_back(OrthancPlugins::IWebDavCollection::FileInfo(it->first, f.GetContent().size(), dateTime));
      }
    }
  }

  void StoreFile(const std::string& name,
                 File* f)
  {
    std::unique_ptr<File> protection(f);

    if (content_.find(name) != content_.end())
    {
      OrthancPlugins::LogError("Already existing: " + name);
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadRequest);
    }
    else
    {
      content_[name] = protection.release();
    }
  }

  void CreateSubfolder(const std::string& name)
  {
    if (content_.find(name) != content_.end())
    {
      OrthancPlugins::LogError("Already existing: " + name);
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadRequest);
    }
    else
    {
      content_[name] = new Folder;
    }
  }

  void DeleteItem(const std::string& name)
  {
    Content::iterator found = content_.find(name);

    if (found == content_.end())
    {
      OrthancPlugins::LogError("Cannot delete inexistent path: " + name);
      ORTHANC_PLUGINS_THROW_EXCEPTION(InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      delete found->second;
      content_.erase(found);
    }
  }
};


class WebDavFilesystem : public OrthancPlugins::IWebDavCollection
{
private:
  boost::mutex               mutex_;
  std::unique_ptr<Resource>  root_;

  static std::vector<std::string> GetParentPath(const std::vector<std::string>& path)
  {
    if (path.empty())
    {
      OrthancPlugins::LogError("Empty path");
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
    else
    {
      std::vector<std::string> p(path.size() - 1);
          
      for (size_t i = 0; i < p.size(); i++)
      {
        p[i] = path[i];
      }

      return p;
    }
  }

public:
  WebDavFilesystem() :
    root_(new Folder)
  {
  }
  
  virtual bool IsExistingFolder(const std::vector<std::string>& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* resource = root_->LookupPath(path);
    return (resource != NULL &&
            resource->IsFolder());
  }

  virtual bool ListFolder(std::list<FileInfo>& files,
                          std::list<FolderInfo>& subfolders,
                          const std::vector<std::string>& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* resource = root_->LookupPath(path);
    if (resource != NULL &&
        resource->IsFolder())
    {
      dynamic_cast<Folder&>(*resource).ListContent(files, subfolders);
      return true;
    }
    else
    {
      return false;
    }
  }
  
  virtual bool GetFile(std::string& content /* out */,
                       std::string& mime /* out */,
                       std::string& dateTime /* out */,
                       const std::vector<std::string>& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* resource = root_->LookupPath(path);
    if (resource != NULL &&
        !resource->IsFolder())
    {
      const File& file = dynamic_cast<const File&>(*resource);
      content = file.GetContent();
      mime = "";  // Let the Orthanc core autodetect the MIME type
      dateTime = boost::posix_time::to_iso_string(file.GetDateTime());
      return true;
    }
    else
    {
      return false;
    }
  }

  virtual bool StoreFile(const std::vector<std::string>& path,
                         const void* data,
                         size_t size)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* parent = root_->LookupPath(GetParentPath(path));
    if (parent != NULL &&
        parent->IsFolder())
    {
      dynamic_cast<Folder&>(*parent).StoreFile(path.back(), new File(data, size));
      return true;
    }
    else
    {
      return false;
    }
  }
  
  virtual bool CreateFolder(const std::vector<std::string>& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* parent = root_->LookupPath(GetParentPath(path));
    if (parent != NULL &&
        parent->IsFolder())
    {
      dynamic_cast<Folder&>(*parent).CreateSubfolder(path.back());
      return true;
    }
    else
    {
      return false;
    }
  }

  virtual bool DeleteItem(const std::vector<std::string>& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    Resource* parent = root_->LookupPath(GetParentPath(path));
    if (parent != NULL &&
        parent->IsFolder())
    {
      dynamic_cast<Folder&>(*parent).DeleteItem(path.back());
      return true;
    }
    else
    {
      return false;
    }    
  }
};



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);
    OrthancPluginLogWarning(c, "WebDAV plugin is initializing");

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(c, info);
      return -1;
    }

    static WebDavFilesystem filesystem;
    OrthancPlugins::IWebDavCollection::Register("/webdav-plugin", filesystem);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "WebDAV plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "webdav-sample";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.0";
  }
}
