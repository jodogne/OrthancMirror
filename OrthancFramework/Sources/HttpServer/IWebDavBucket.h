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


#pragma once

#if !defined(ORTHANC_ENABLE_PUGIXML)
#  error The macro ORTHANC_ENABLE_PUGIXML must be defined
#endif

#if ORTHANC_ENABLE_PUGIXML != 1
#  error XML support is required to use this file
#endif

#include "../Compatibility.h"
#include "../Enumerations.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/noncopyable.hpp>
#include <pugixml.hpp>

#include <list>
#include <set>
#include <stdint.h>

namespace Orthanc
{
  class HttpOutput;
  
  class IWebDavBucket : public boost::noncopyable
  {
  public:
    class Resource : public boost::noncopyable
    {
    private:
      std::string               displayName_;
      bool                      hasModificationTime_;
      boost::posix_time::ptime  creationTime_;
      boost::posix_time::ptime  modificationTime_;

    public:
      explicit Resource(const std::string& displayName);

      virtual ~Resource()
      {
      }

      void SetCreationTime(const boost::posix_time::ptime& t);

      void SetModificationTime(const boost::posix_time::ptime& t);

      const std::string& GetDisplayName() const
      {
        return displayName_;
      }

      const boost::posix_time::ptime& GetCreationTime() const
      {
        return creationTime_;
      }

      const boost::posix_time::ptime& GetModificationTime() const
      {
        return modificationTime_;
      }

      virtual void Format(pugi::xml_node& node,
                          const std::string& parentPath) const = 0;
    };


    class File : public Resource
    {
    private:
      uint64_t  contentLength_;
      MimeType  mime_;

    public:
      explicit File(const std::string& displayName);

      void SetContentLength(uint64_t contentLength)
      {
        contentLength_ = contentLength;
      }

      void SetMimeType(MimeType mime)
      {
        mime_ = mime;
      }

      uint64_t GetContentLength() const
      {
        return contentLength_;
      }

      MimeType GetMimeType() const
      {
        return mime_;
      }

      void SetCreated(bool created);

      virtual void Format(pugi::xml_node& node,
                          const std::string& parentPath) const ORTHANC_OVERRIDE;
    };


    class Folder : public Resource
    {
    public:
      explicit Folder(const std::string& displayName) :
        Resource(displayName)
      {
      }
      
      virtual void Format(pugi::xml_node& node,
                          const std::string& parentPath) const ORTHANC_OVERRIDE;
    };


    class Collection : public boost::noncopyable
    {
    private:
      std::list<Resource*>  resources_;

    public:
      ~Collection();

      size_t GetSize() const
      {
        return resources_.size();
      }

      void ListDisplayNames(std::set<std::string>& target);

      void AddResource(Resource* resource);  // Takes ownership

      void Format(std::string& target,
                  const std::string& parentPath) const;
    };


    virtual ~IWebDavBucket()
    {
    }

    virtual bool IsExistingFolder(const std::vector<std::string>& path) = 0;

    virtual bool ListCollection(Collection& collection,
                                const std::vector<std::string>& path) = 0;

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& modificationTime, 
                                const std::vector<std::string>& path) = 0;

    // "false" returns indicate a read-only target
    virtual bool StoreFile(const std::string& content,
                           const std::vector<std::string>& path) = 0;

    virtual bool CreateFolder(const std::vector<std::string>& path) = 0;

    virtual bool DeleteItem(const std::vector<std::string>& path) = 0;

    virtual void Start() = 0;

    // During the shutdown of the Web server, give a chance to the
    // bucket to end its pending operations
    virtual void Stop() = 0;


    static void AnswerFakedProppatch(HttpOutput& output,
                                     const std::string& uri);

    static void AnswerFakedLock(HttpOutput& output,
                                const std::string& uri);

    static void AnswerFakedUnlock(HttpOutput& output);
  };
}
