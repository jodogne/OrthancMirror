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


#include "../Core/PrecompiledHeaders.h"
#include "Instance.h"

#include "OrthancConnection.h"

#include <boost/lexical_cast.hpp>

namespace OrthancClient
{
  void Instance::DownloadImage()
  {
    if (reader_.get() == NULL)
    {
      const char* suffix;
      switch (mode_)
      {
        case Orthanc::ImageExtractionMode_Preview:
          suffix = "preview";
          break;
          
        case Orthanc::ImageExtractionMode_UInt8:
          suffix = "image-uint8";
          break;
          
        case Orthanc::ImageExtractionMode_UInt16:
          suffix = "image-uint16";
          break;
          
        case Orthanc::ImageExtractionMode_Int16:
          suffix = "image-int16";
          break;
          
        default:
          throw OrthancClientException(Orthanc::ErrorCode_NotImplemented);
      }

      Orthanc::HttpClient client(connection_.GetHttpClient());
      client.SetUrl(std::string(connection_.GetOrthancUrl()) +  "/instances/" + id_ + "/" + suffix);
      std::string png;

      if (!client.Apply(png))
      {
        throw OrthancClientException(Orthanc::ErrorCode_NotImplemented);
      }
     
      reader_.reset(new Orthanc::PngReader);
      reader_->ReadFromMemory(png);
    }
  }

  void Instance::DownloadDicom()
  {
    if (dicom_.get() == NULL)
    {
      Orthanc::HttpClient client(connection_.GetHttpClient());
      client.SetUrl(std::string(connection_.GetOrthancUrl()) +  "/instances/" + id_ + "/file");

      dicom_.reset(new std::string);

      if (!client.Apply(*dicom_))
      {
        throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
      }
    }
  }

  Instance::Instance(const OrthancConnection& connection,
                     const char* id) :
    connection_(connection),
    id_(id),
    mode_(Orthanc::ImageExtractionMode_Int16)
  {
    Orthanc::HttpClient client(connection_.GetHttpClient());
            
    client.SetUrl(std::string(connection_.GetOrthancUrl()) + "/instances/" + id_ + "/simplified-tags");
    Json::Value v;
    if (!client.Apply(tags_))
    {
      throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
    }
  }

  const char* Instance::GetTagAsString(const char* tag) const
  {
    if (tags_.isMember(tag))
    {
      return tags_[tag].asCString();
    }
    else
    {
      throw OrthancClientException(Orthanc::ErrorCode_InexistentItem);
    }
  }

  float Instance::GetTagAsFloat(const char* tag) const
  {
    std::string value = GetTagAsString(tag);

    try
    {
      return boost::lexical_cast<float>(value);
    }
    catch (boost::bad_lexical_cast)
    {
      throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
    }
  }

  int Instance::GetTagAsInt(const char* tag) const
  {
    std::string value = GetTagAsString(tag);

    try
    {
      return boost::lexical_cast<int>(value);
    }
    catch (boost::bad_lexical_cast)
    {
      throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
    }
  }

  unsigned int Instance::GetWidth()
  {
    DownloadImage();
    return reader_->GetWidth();
  }

  unsigned int Instance::GetHeight() 
  {
    DownloadImage();
    return reader_->GetHeight();
  }

  unsigned int Instance::GetPitch()
  {
    DownloadImage();
    return reader_->GetPitch();
  }

  Orthanc::PixelFormat Instance::GetPixelFormat()
  {
    DownloadImage();
    return reader_->GetFormat();
  }

  const void* Instance::GetBuffer()
  {
    DownloadImage();
    return reader_->GetConstBuffer();
  }

  const void* Instance::GetBuffer(unsigned int y)
  {
    DownloadImage();
    return reader_->GetConstRow(y);
  }

  void Instance::DiscardImage()
  {
    reader_.reset();
  }

  void Instance::DiscardDicom()
  {
    dicom_.reset();
  }


  void Instance::SetImageExtractionMode(Orthanc::ImageExtractionMode mode)
  {
    if (mode_ == mode)
    {
      return;
    }

    DiscardImage();
    mode_ = mode;
  }


  void Instance::SplitVectorOfFloats(std::vector<float>& target,
                                     const char* tag)
  {
    const std::string value = GetTagAsString(tag);

    target.clear();

    try
    {
      std::string tmp;
      for (size_t i = 0; i < value.size(); i++)
      {
        if (value[i] == '\\')
        {
          target.push_back(boost::lexical_cast<float>(tmp));
          tmp.clear();
        }
        else
        {
          tmp.push_back(value[i]);
        }
      }

      target.push_back(boost::lexical_cast<float>(tmp));
    }
    catch (boost::bad_lexical_cast)
    {
      // Unable to parse the Image Orientation Patient.
      throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
    }
  }


  const uint64_t Instance::GetDicomSize()
  {
    DownloadDicom();
    assert(dicom_.get() != NULL);
    return dicom_->size();
  }

  const void* Instance::GetDicom()
  {
    DownloadDicom();
    assert(dicom_.get() != NULL);

    if (dicom_->size() == 0)
    {
      return NULL;
    }
    else
    {
      return &((*dicom_) [0]);
    }
  }


  void Instance::LoadTagContent(const char* path)
  {
    Orthanc::HttpClient client(connection_.GetHttpClient());
    client.SetUrl(std::string(connection_.GetOrthancUrl()) + "/instances/" + id_ + "/content/" + path);

    if (!client.Apply(content_))
    {
      throw OrthancClientException(Orthanc::ErrorCode_UnknownResource);
    }
  }


  const char* Instance::GetLoadedTagContent() const
  {
    return content_.c_str();
  }
}
