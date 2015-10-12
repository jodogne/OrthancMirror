/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "../Core/Toolbox.h"

#include <boost/lexical_cast.hpp>


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
        value->IsNull())
    {
      return false;
    }
    else
    {
      return TokenizeVector(result, value->AsString(), expectedSize);
    }
  }


  struct SliceOrdering::Instance
  {
    std::string   instanceId_;
    bool          hasPosition_;
    Vector        position_;   
    bool          hasIndexInSeries_;
    size_t        indexInSeries_;

    Instance(ServerIndex& index,
          const std::string& instanceId) :
      instanceId_(instanceId)
    {
      DicomMap instance;
      if (!index.GetMainDicomTags(instance, instanceId, ResourceType_Instance, ResourceType_Instance))
      {
        throw OrthancException(ErrorCode_UnknownResource);
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
  };


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
    if (!hasNormal_)
    {
      return false;
    }

    for (size_t i = 0; i < instances_.size(); i++)
    {
      assert(instances_[i] != NULL);
      if (!instances_[i]->hasPosition_)
      {
        return false;
      }
    }

    

    return true;
  }


  SliceOrdering::SliceOrdering(ServerIndex& index,
                               const std::string& seriesId) :
    index_(index),
    seriesId_(seriesId)
  {
    ComputeNormal();
    CreateInstances();
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
}
