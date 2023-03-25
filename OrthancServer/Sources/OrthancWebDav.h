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


#pragma once

#include "../../OrthancFramework/Sources/HttpServer/WebDavStorage.h"
#include "../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../../OrthancFramework/Sources/Toolbox.h"


namespace Orthanc
{
  class ServerContext;
  
  class OrthancWebDav : public IWebDavBucket
  {
  private:
    typedef std::map<ResourceType, std::string>  Templates;

    class DicomDeleteVisitor;
    class DicomFileVisitor;
    class DicomIdentifiersVisitor;  
    class InstancesOfSeries;
    class InternalNode;
    class ListOfResources;
    class ListOfStudiesByDate;
    class ListOfStudiesByMonth;
    class ListOfStudiesByYear;
    class OrthancJsonVisitor;
    class ResourcesIndex;
    class RootNode;
    class SingleDicomResource;

    class INode : public boost::noncopyable
    {
    public:
      virtual ~INode()
      {
      }

      virtual bool ListCollection(IWebDavBucket::Collection& target,
                                  const UriComponents& path) = 0;

      virtual bool GetFileContent(MimeType& mime,
                                  std::string& content,
                                  boost::posix_time::ptime& time, 
                                  const UriComponents& path) = 0;

      virtual bool DeleteItem(const UriComponents& path) = 0;
    };


    void AddVirtualFile(Collection& collection,
                        const UriComponents& path,
                        const std::string& filename);
    
    static void UploadWorker(OrthancWebDav* that);

    void Upload(const std::string& path);

    INode& GetRootNode(const std::string& rootPath);
  
    ServerContext&          context_;
    bool                    allowDicomDelete_;
    bool                    allowUpload_;
    std::unique_ptr<INode>  patients_;
    std::unique_ptr<INode>  studies_;
    std::unique_ptr<INode>  dates_;
    Templates               patientsTemplates_;
    Templates               studiesTemplates_;
    WebDavStorage           uploads_;
    SharedMessageQueue      uploadQueue_;
    boost::thread           uploadThread_;
    bool                    uploadRunning_;
  
  public:
    OrthancWebDav(ServerContext& context,
                  bool allowDicomDelete,
                  bool allowUpload);

    virtual ~OrthancWebDav()
    {
      Stop();
    }

    virtual bool IsExistingFolder(const UriComponents& path) ORTHANC_OVERRIDE;

    virtual bool ListCollection(Collection& collection,
                                const UriComponents& path) ORTHANC_OVERRIDE;

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& modificationTime, 
                                const UriComponents& path) ORTHANC_OVERRIDE;
  
    virtual bool StoreFile(const std::string& content,
                           const UriComponents& path) ORTHANC_OVERRIDE;

    virtual bool CreateFolder(const UriComponents& path) ORTHANC_OVERRIDE;

    virtual bool DeleteItem(const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual void Start() ORTHANC_OVERRIDE;

    virtual void Stop() ORTHANC_OVERRIDE;
  };
}
