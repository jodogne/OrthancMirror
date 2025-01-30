/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "SimpleInstanceOrdering.h"

#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Toolbox.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "ServerEnumerations.h"
#include "ServerIndex.h"
#include "ResourceFinder.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>


namespace Orthanc
{
  struct SimpleInstanceOrdering::Instance : public boost::noncopyable
  {
  private:
    std::string   instanceId_;
    uint32_t      indexInSeries_;
    FileInfo      fileInfo_;


  public:
    Instance(const std::string& instanceId,
             uint32_t indexInSeries,
             const FileInfo& fileInfo) :
      instanceId_(instanceId),
      indexInSeries_(indexInSeries),
      fileInfo_(fileInfo)
    {
    }

    const std::string& GetIdentifier() const
    {
      return instanceId_;
    }

    uint32_t GetIndexInSeries() const
    {
      return indexInSeries_;
    }
 
    const FileInfo& GetFileInfo() const
    {
      return fileInfo_;
    }
  };

  bool SimpleInstanceOrdering::IndexInSeriesComparator(const SimpleInstanceOrdering::Instance* a,
                                                       const SimpleInstanceOrdering::Instance* b)
  {
    return a->GetIndexInSeries() < b->GetIndexInSeries();
  }  


  SimpleInstanceOrdering::SimpleInstanceOrdering(ServerIndex& index,
                                                 const std::string& seriesId) :
    hasDuplicateIndexInSeries_(false)
  {
    FindRequest request(ResourceType_Instance);
    request.SetOrthancSeriesId(seriesId);
    request.SetRetrieveMetadata(true);
    request.SetRetrieveAttachments(true);

    FindResponse response;
    index.ExecuteFind(response, request);

    std::set<uint32_t> allIndexInSeries;

    for (size_t i = 0; i < response.GetSize(); ++i)
    {
      const FindResponse::Resource& resource = response.GetResourceByIndex(i);
      std::string instanceId = resource.GetIdentifier();

      std::string strIndexInSeries;
      uint32_t indexInSeries = 0;
      FileInfo fileInfo;
      int64_t revisionNotUsed;
			
      if (resource.LookupMetadata(strIndexInSeries, ResourceType_Instance, MetadataType_Instance_IndexInSeries))
      {
        SerializationToolbox::ParseUnsignedInteger32(indexInSeries, strIndexInSeries);
      }

      if (resource.LookupAttachment(fileInfo, revisionNotUsed, FileContentType_Dicom))
      {
        allIndexInSeries.insert(indexInSeries);
        instances_.push_back(new SimpleInstanceOrdering::Instance(instanceId, indexInSeries, fileInfo));
      }
    }

    // if there are duplicates, the set will be smaller than the vector
    hasDuplicateIndexInSeries_ = allIndexInSeries.size() != instances_.size(); 

    std::sort(instances_.begin(), instances_.end(), IndexInSeriesComparator);
  }

  SimpleInstanceOrdering::~SimpleInstanceOrdering()
  {
    for (std::vector<Instance*>::iterator
           it = instances_.begin(); it != instances_.end(); ++it)
    {
      if (*it != NULL)
      {
        delete *it;
      }
    }
  }

  const std::string& SimpleInstanceOrdering::GetInstanceId(size_t index) const
  {
    return instances_[index]->GetIdentifier();
  }

  uint32_t SimpleInstanceOrdering::GetInstanceIndexInSeries(size_t index) const
  {
    if (hasDuplicateIndexInSeries_)
    {
      // if there are duplicates, we count the instances from 0
      return index; 
    }
    else
    {
      // if there are no duplicates, we return the real index in series (that may not start at 0 but that is more human friendly)
      return instances_[index]->GetIndexInSeries();
    }
  }

  const FileInfo& SimpleInstanceOrdering::GetInstanceFileInfo(size_t index) const
  {
    return instances_[index]->GetFileInfo();
  }
}
