/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "PrecompiledHeadersServer.h"
#include "SliceOrdering.h"

#include "../Core/Logging.h"
#include "../Core/Toolbox.h"
#include "ServerEnumerations.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>


namespace Orthanc
{
  static bool TokenizeVector(std::vector<float>& result,
                             const std::string& value,
                             unsigned int expectedSize)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, value, '\\');

    if (tokens.size() != expectedSize)
    {
      return false;
    }

    result.resize(tokens.size());

    for (size_t i = 0; i < tokens.size(); i++)
    {
      try
      {
        result[i] = boost::lexical_cast<float>(tokens[i]);
      }
      catch (boost::bad_lexical_cast&)
      {
        return false;
      }
    }

    return true;
  }


  static bool TokenizeVector(std::vector<float>& result,
                             const DicomMap& map,
                             const DicomTag& tag,
                             unsigned int expectedSize)
  {
    const DicomValue* value = map.TestAndGetValue(tag);

    if (value == NULL ||
        value->IsNull() ||
        value->IsBinary())
    {
      return false;
    }
    else
    {
      return TokenizeVector(result, value->GetContent(), expectedSize);
    }
  }


  struct SliceOrdering::Instance : public boost::noncopyable
  {
  private:
    std::string   instanceId_;
    bool          hasPosition_;
    Vector        position_;   
    bool          hasIndexInSeries_;
    size_t        indexInSeries_;
    unsigned int  framesCount_;

  public:
    Instance(ServerIndex& index,
             const std::string& instanceId) :
      instanceId_(instanceId),
      framesCount_(1)
    {
      DicomMap instance;
      if (!index.GetMainDicomTags(instance, instanceId, ResourceType_Instance, ResourceType_Instance))
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }

      const DicomValue* frames = instance.TestAndGetValue(DICOM_TAG_NUMBER_OF_FRAMES);
      if (frames != NULL &&
          !frames->IsNull() &&
          !frames->IsBinary())
      {
        try
        {
          framesCount_ = boost::lexical_cast<unsigned int>(frames->GetContent());
        }
        catch (boost::bad_lexical_cast&)
        {
        }
      }
      
      std::vector<float> tmp;
      hasPosition_ = TokenizeVector(tmp, instance, DICOM_TAG_IMAGE_POSITION_PATIENT, 3);

      if (hasPosition_)
      {
        position_[0] = tmp[0];
        position_[1] = tmp[1];
        position_[2] = tmp[2];
      }

      std::string s;
      hasIndexInSeries_ = false;

      try
      {
        if (index.LookupMetadata(s, instanceId, MetadataType_Instance_IndexInSeries))
        {
          indexInSeries_ = boost::lexical_cast<size_t>(s);
          hasIndexInSeries_ = true;
        }
      }
      catch (boost::bad_lexical_cast&)
      {
      }
    }

    const std::string& GetIdentifier() const
    {
      return instanceId_;
    }

    bool HasPosition() const
    {
      return hasPosition_;
    }

    float ComputeRelativePosition(const Vector& normal) const
    {
      assert(HasPosition());
      return (normal[0] * position_[0] + 
              normal[1] * position_[1] +
              normal[2] * position_[2]);
    }

    bool HasIndexInSeries() const
    {
      return hasIndexInSeries_;
    }
    
    size_t GetIndexInSeries() const
    {
      assert(HasIndexInSeries());
      return indexInSeries_;
    }

    unsigned int GetFramesCount() const
    {
      return framesCount_;
    }
  };


  class SliceOrdering::PositionComparator
  {
  private:
    const Vector&  normal_;

  public:
    PositionComparator(const Vector& normal) : normal_(normal)
    {
    }
    
    int operator() (const Instance* a,
                    const Instance* b) const
    {
      return a->ComputeRelativePosition(normal_) < b->ComputeRelativePosition(normal_);
    }
  };


  bool SliceOrdering::IndexInSeriesComparator(const SliceOrdering::Instance* a,
                                              const SliceOrdering::Instance* b)
  {
    return a->GetIndexInSeries() < b->GetIndexInSeries();
  }  


  void SliceOrdering::ComputeNormal()
  {
    DicomMap series;
    if (!index_.GetMainDicomTags(series, seriesId_, ResourceType_Series, ResourceType_Series))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    std::vector<float> cosines;
    hasNormal_ = TokenizeVector(cosines, series, DICOM_TAG_IMAGE_ORIENTATION_PATIENT, 6);

    if (hasNormal_)
    {
      normal_[0] = cosines[1] * cosines[5] - cosines[2] * cosines[4];
      normal_[1] = cosines[2] * cosines[3] - cosines[0] * cosines[5];
      normal_[2] = cosines[0] * cosines[4] - cosines[1] * cosines[3];
    }
  }


  void SliceOrdering::CreateInstances()
  {
    std::list<std::string> instancesId;
    index_.GetChildren(instancesId, seriesId_);

    instances_.reserve(instancesId.size());
    for (std::list<std::string>::const_iterator
           it = instancesId.begin(); it != instancesId.end(); ++it)
    {
      instances_.push_back(new Instance(index_, *it));
    }
  }
  

  bool SliceOrdering::SortUsingPositions()
  {
    if (instances_.size() <= 1)
    {
      // One single instance: It is sorted by default
      return true;
    }

    if (!hasNormal_)
    {
      return false;
    }

    for (size_t i = 0; i < instances_.size(); i++)
    {
      assert(instances_[i] != NULL);
      if (!instances_[i]->HasPosition())
      {
        return false;
      }
    }

    PositionComparator comparator(normal_);
    std::sort(instances_.begin(), instances_.end(), comparator);

    float a = instances_[0]->ComputeRelativePosition(normal_);
    for (size_t i = 1; i < instances_.size(); i++)
    {
      float b = instances_[i]->ComputeRelativePosition(normal_);

      if (std::fabs(b - a) <= 10.0f * std::numeric_limits<float>::epsilon())
      {
        // Not enough space between two slices along the normal of the volume
        return false;
      }

      a = b;
    }

    // This is a 3D volume
    isVolume_ = true;
    return true;
  }


  bool SliceOrdering::SortUsingIndexInSeries()
  {
    if (instances_.size() <= 1)
    {
      // One single instance: It is sorted by default
      return true;
    }

    for (size_t i = 0; i < instances_.size(); i++)
    {
      assert(instances_[i] != NULL);
      if (!instances_[i]->HasIndexInSeries())
      {
        return false;
      }
    }

    std::sort(instances_.begin(), instances_.end(), IndexInSeriesComparator);
    
    for (size_t i = 1; i < instances_.size(); i++)
    {
      if (instances_[i - 1]->GetIndexInSeries() == instances_[i]->GetIndexInSeries())
      {
        // The current "IndexInSeries" occurs 2 times: Not a proper ordering
        LOG(WARNING) << "This series contains 2 slices with the same index, trying to display it anyway";
        break;
      }
    }

    return true;
  }


  SliceOrdering::SliceOrdering(ServerIndex& index,
                               const std::string& seriesId) :
    index_(index),
    seriesId_(seriesId),
    isVolume_(false)
  {
    ComputeNormal();
    CreateInstances();

    if (!SortUsingPositions() &&
        !SortUsingIndexInSeries())
    {
      LOG(ERROR) << "Unable to order the slices of the series " << seriesId;
      throw OrthancException(ErrorCode_CannotOrderSlices);
    }
  }


  SliceOrdering::~SliceOrdering()
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


  const std::string& SliceOrdering::GetInstanceId(size_t index) const
  {
    if (index >= instances_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return instances_[index]->GetIdentifier();
    }
  }


  unsigned int SliceOrdering::GetFramesCount(size_t index) const
  {
    if (index >= instances_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return instances_[index]->GetFramesCount();
    }
  }


  void SliceOrdering::Format(Json::Value& result) const
  {
    result = Json::objectValue;
    result["Type"] = (isVolume_ ? "Volume" : "Sequence");
    
    Json::Value tmp = Json::arrayValue;
    for (size_t i = 0; i < GetInstancesCount(); i++)
    {
      tmp.append(GetBasePath(ResourceType_Instance, GetInstanceId(i)) + "/file");
    }

    result["Dicom"] = tmp;

    Json::Value slicesShort = Json::arrayValue;

    tmp.clear();
    for (size_t i = 0; i < GetInstancesCount(); i++)
    {
      std::string base = GetBasePath(ResourceType_Instance, GetInstanceId(i));
      for (size_t j = 0; j < GetFramesCount(i); j++)
      {
        tmp.append(base + "/frames/" + boost::lexical_cast<std::string>(j));
      }

      Json::Value tmp2 = Json::arrayValue;
      tmp2.append(GetInstanceId(i));
      tmp2.append(0);
      tmp2.append(GetFramesCount(i));
      
      slicesShort.append(tmp2);
    }

    result["Slices"] = tmp;
    result["SlicesShort"] = slicesShort;
  }
}
