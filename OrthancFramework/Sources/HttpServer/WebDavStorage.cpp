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


#include "../PrecompiledHeaders.h"
#include "WebDavStorage.h"

#include "../OrthancException.h"
#include "../SystemToolbox.h"
#include "../TemporaryFile.h"
#include "../Toolbox.h"

namespace Orthanc
{
  class WebDavStorage::StorageFile : public boost::noncopyable
  {
  private:
    std::unique_ptr<TemporaryFile>  file_;
    std::string                     content_;
    MimeType                        mime_;
    boost::posix_time::ptime        time_;

    void Touch()
    {
      time_ = boost::posix_time::second_clock::universal_time();
    }
    
  public:
    StorageFile() :
      mime_(MimeType_Binary)
    {
      Touch();
    }
    
    void SetContent(const std::string& content,
                    MimeType mime,
                    bool isMemory)
    {
      if (isMemory)
      {
        content_ = content;
        file_.reset();
      }
      else
      {
        content_.clear();
        file_.reset(new TemporaryFile);
        file_->Write(content);
      }
      
      mime_ = mime;
      Touch();
    }

    MimeType GetMimeType() const
    {
      return mime_;
    }

    void GetContent(std::string& target) const
    {
      if (file_.get() == NULL)
      {
        target = content_;
      }
      else
      {
        file_->Read(target);
      }
    }

    const boost::posix_time::ptime& GetTime() const
    {
      return time_;
    }

    uint64_t GetContentLength() const
    {
      if (file_.get() == NULL)
      {
        return content_.size();
      }
      else
      {
        return file_->GetFileSize();
      }
    }
  };


  class WebDavStorage::StorageFolder : public boost::noncopyable
  {
  private:
    typedef std::map<std::string, StorageFile*>    Files;
    typedef std::map<std::string, StorageFolder*>  Subfolders;

    Files       files_;
    Subfolders  subfolders_;

    void CheckName(const std::string& name)
    {
      if (name.empty() ||
          name.find('/') != std::string::npos ||
          name.find('\\') != std::string::npos ||
          name.find('\0') != std::string::npos)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Bad resource name for WebDAV: " + name);
      }
    }

    bool IsExisting(const std::string& name) const
    {
      return (files_.find(name) != files_.end() ||
              subfolders_.find(name) != subfolders_.end());
    }

  public:
    ~StorageFolder()
    {
      for (Files::iterator it = files_.begin(); it != files_.end(); ++it)
      {
        assert(it->second != NULL);
        delete it->second;
      }        

      for (Subfolders::iterator it = subfolders_.begin(); it != subfolders_.end(); ++it)
      {
        assert(it->second != NULL);
        delete it->second;
      }        
    }

    const StorageFile* LookupFile(const std::string& name) const
    {
      Files::const_iterator found = files_.find(name);
      if (found == files_.end())
      {
        return NULL;
      }
      else
      {
        assert(found->second != NULL);
        return found->second;
      }
    }

    bool CreateSubfolder(const std::string& name)
    {
      CheckName(name);

      if (IsExisting(name))
      {
        LOG(ERROR) << "WebDAV folder already existing: " << name;
        return false;
      }
      else
      {
        subfolders_[name] = new StorageFolder;
        return true;
      }
    }

    bool StoreFile(const std::string& name,
                   const std::string& content,
                   MimeType mime,
                   bool isMemory)
    {
      CheckName(name);

      if (subfolders_.find(name) != subfolders_.end())
      {
        LOG(ERROR) << "WebDAV folder already existing: " << name;
        return false;
      }

      Files::iterator found = files_.find(name);
      if (found == files_.end())
      {
        std::unique_ptr<StorageFile> f(new StorageFile);
        f->SetContent(content, mime, isMemory);
        files_[name] = f.release();
      }
      else
      {
        assert(found->second != NULL);
        found->second->SetContent(content, mime, isMemory);
      }
      
      return true;
    }

    StorageFolder* LookupFolder(const std::vector<std::string>& path)
    {
      if (path.empty())
      {
        return this;
      }
      else
      {
        Subfolders::const_iterator found = subfolders_.find(path[0]);
        if (found == subfolders_.end())
        {
          return NULL;
        }
        else
        {
          assert(found->second != NULL);

          std::vector<std::string> p(path.begin() + 1, path.end());
          return found->second->LookupFolder(p);
        }
      }
    }

    void ListCollection(Collection& collection) const
    {
      for (Files::const_iterator it = files_.begin(); it != files_.end(); ++it)
      {
        assert(it->second != NULL);
        
        std::unique_ptr<File> f(new File(it->first));
        f->SetContentLength(it->second->GetContentLength());
        f->SetCreationTime(it->second->GetTime());
        collection.AddResource(f.release());
      }

      for (Subfolders::const_iterator it = subfolders_.begin(); it != subfolders_.end(); ++it)
      {
        collection.AddResource(new Folder(it->first));
      }
    }
  };


  WebDavStorage::StorageFolder* WebDavStorage::LookupParentFolder(const std::vector<std::string>& path)
  {
    if (path.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    std::vector<std::string> p(path.begin(), path.end() - 1);      
    return root_->LookupFolder(p);
  }
    

  WebDavStorage::WebDavStorage(bool isMemory) :
    root_(new StorageFolder),
    isMemory_(isMemory)
  {
  }
  

  bool WebDavStorage::IsExistingFolder(const std::vector<std::string>& path)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    
    return (root_->LookupFolder(path) != NULL);
  }

  
  bool WebDavStorage::ListCollection(Collection& collection,
                                     const std::vector<std::string>& path)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    
    const StorageFolder* folder = root_->LookupFolder(path);
    if (folder == NULL)
    {
      return false;
    }
    else
    {
      folder->ListCollection(collection);
      return true;
    }
  }

  
  bool WebDavStorage::GetFileContent(MimeType& mime,
                                     std::string& content,
                                     boost::posix_time::ptime& modificationTime, 
                                     const std::vector<std::string>& path)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    
    const StorageFolder* folder = LookupParentFolder(path);    
    if (folder == NULL)
    {
      return false;
    }
    else
    {
      const StorageFile* file = folder->LookupFile(path.back());
      if (file == NULL)
      {
        return false;
      }
      else
      {
        mime = file->GetMimeType();
        file->GetContent(content);
        modificationTime = file->GetTime();
        return true;
      }
    }
  }

  
  bool WebDavStorage::StoreFile(const std::string& content,
                                const std::vector<std::string>& path)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    
    StorageFolder* folder = LookupParentFolder(path);
    if (folder == NULL)
    {
      LOG(WARNING) << "Inexisting folder in WebDAV: " << Toolbox::FlattenUri(path);
      return false;
    }
    else
    {
      LOG(INFO) << "Storing " << content.size()
                << " bytes in WebDAV bucket: " << Toolbox::FlattenUri(path);;

      MimeType mime = SystemToolbox::AutodetectMimeType(path.back());
      return folder->StoreFile(path.back(), content, mime, isMemory_);
    }
  }

  
  bool WebDavStorage::CreateFolder(const std::vector<std::string>& path)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    StorageFolder* folder = LookupParentFolder(path);      
    if (folder == NULL)
    {
      LOG(WARNING) << "Inexisting folder in WebDAV: " << Toolbox::FlattenUri(path);
      return false;
    }
    else
    {
      LOG(INFO) << "Creating folder in WebDAV bucket: " << Toolbox::FlattenUri(path);
      return folder->CreateSubfolder(path.back());
    }
  }
}
