/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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


#pragma once

#include <string>
#include <json/value.h>

#include "OrthancClientException.h"
#include "../Core/IDynamicObject.h"
#include "../Core/FileFormats/PngReader.h"

namespace OrthancClient
{
  class OrthancConnection;

  class LAAW_API Instance : public Orthanc::IDynamicObject
  {
  private:
    const OrthancConnection& connection_;
    std::string id_;
    Json::Value tags_;
    std::auto_ptr<Orthanc::PngReader> reader_;
    Orthanc::ImageExtractionMode mode_;
    std::auto_ptr<std::string> dicom_;

    void DownloadImage();

    void DownloadDicom();

  public:
    Instance(const OrthancConnection& connection,
             const char* id);

    const char* GetId() const
    {
      return id_.c_str();
    }

    void SetImageExtractionMode(Orthanc::ImageExtractionMode mode);

    Orthanc::ImageExtractionMode GetImageExtractionMode() const
    {
      return mode_;
    }

    const char* GetTagAsString(const char* tag) const;

    float GetTagAsFloat(const char* tag) const;

    int32_t GetTagAsInt(const char* tag) const;

    uint32_t GetWidth();

    uint32_t GetHeight();

    uint32_t GetPitch();

    Orthanc::PixelFormat GetPixelFormat();

    const void* GetBuffer();

    const void* GetBuffer(uint32_t y);

    void DiscardImage();

    void DiscardDicom();

    const uint64_t GetDicomSize();

    const void* GetDicom();

    LAAW_API_INTERNAL void SplitVectorOfFloats(std::vector<float>& target,
                                               const char* tag);
  };
}
