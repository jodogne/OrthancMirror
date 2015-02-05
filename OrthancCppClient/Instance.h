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


#pragma once

#include <string>
#include <json/value.h>

#include "OrthancClientException.h"
#include "../Core/IDynamicObject.h"
#include "../Core/ImageFormats/PngReader.h"

namespace OrthancClient
{
  class OrthancConnection;

  /**
   * {summary}{Connection to an instance stored in %Orthanc.}
   * {description}{This class encapsulates a connection to an image instance
   * from a remote instance of %Orthanc.}
   **/
  class LAAW_API Instance : public Orthanc::IDynamicObject
  {
  private:
    const OrthancConnection& connection_;
    std::string id_;
    Json::Value tags_;
    std::auto_ptr<Orthanc::PngReader> reader_;
    Orthanc::ImageExtractionMode mode_;
    std::auto_ptr<std::string> dicom_;
    std::string content_;

    void DownloadImage();

    void DownloadDicom();

  public:
     /**
     * {summary}{Create a connection to some image instance.}
     * {param}{connection The remote instance of %Orthanc.}
     * {param}{id The %Orthanc identifier of the image instance.}
     **/
    Instance(const OrthancConnection& connection,
             const char* id);

    
    /**
     * {summary}{Get the %Orthanc identifier of this identifier.}
     * {returns}{The identifier.}
     **/
    const char* GetId() const
    {
      return id_.c_str();
    }


    /**
     * {summary}{Set the extraction mode for the 2D image corresponding to this instance.}
     * {param}{mode The extraction mode.}
     **/
    void SetImageExtractionMode(Orthanc::ImageExtractionMode mode);

    /**
     * {summary}{Get the extraction mode for the 2D image corresponding to this instance.}
     * {returns}{The extraction mode.}
     **/
    Orthanc::ImageExtractionMode GetImageExtractionMode() const
    {
      return mode_;
    }

    
    /**
     * {summary}{Get the string value of some DICOM tag of this instance.}
     * {param}{tag The name of the tag of interest.}
     * {returns}{The value of the tag.}
     **/
    const char* GetTagAsString(const char* tag) const;

    /**
     * {summary}{Get the floating point value that is stored in some DICOM tag of this instance.}
     * {param}{tag The name of the tag of interest.}
     * {returns}{The value of the tag.}
     **/
    float GetTagAsFloat(const char* tag) const;

    /**
     * {summary}{Get the integer value that is stored in some DICOM tag of this instance.}
     * {param}{tag The name of the tag of interest.}
     * {returns}{The value of the tag.}
     **/
    int32_t GetTagAsInt(const char* tag) const;

    
    /**
     * {summary}{Get the width of the 2D image.}
     * {description}{Get the width of the 2D image that is encoded by this DICOM instance.}
     * {returns}{The width.}
     **/
    uint32_t GetWidth();

    /**
     * {summary}{Get the height of the 2D image.}
     * {description}{Get the height of the 2D image that is encoded by this DICOM instance.}
     * {returns}{The height.}
     **/
    uint32_t GetHeight();

    /**
     * {summary}{Get the number of bytes between two lines of the image (pitch).}
     * {description}{Get the number of bytes between two lines of the image in the memory buffer returned by GetBuffer(). This value depends on the extraction mode for the image.}
     * {returns}{The pitch.}
     **/
    uint32_t GetPitch();

    /**
     * {summary}{Get the format of the pixels of the 2D image.}
     * {description}{Return the memory layout that is used for the 2D image that is encoded by this DICOM instance. This value depends on the extraction mode for the image.}
     * {returns}{The pixel format.}
     **/
    Orthanc::PixelFormat GetPixelFormat();

    /**
     * {summary}{Access the memory buffer in which the raw pixels of the 2D image are stored.}
     * {returns}{A pointer to the memory buffer.}
     **/
    const void* GetBuffer();

    /**
     * {summary}{Access the memory buffer in which the raw pixels of some line of the 2D image are stored.}
     * {param}{y The line of interest.}
     * {returns}{A pointer to the memory buffer.}
     **/
    const void* GetBuffer(uint32_t y);

    /**
     * {summary}{Get the size of the DICOM file corresponding to this instance.}
     * {returns}{The file size.}
     **/
    const uint64_t GetDicomSize();

    /**
     * {summary}{Get a pointer to the content of the DICOM file corresponding to this instance.}
     * {returns}{The DICOM file.}
     **/
    const void* GetDicom();

    /**
     * {summary}{Discard the downloaded 2D image, so as to make room in memory.}
     **/
    void DiscardImage();

    /**
     * {summary}{Discard the downloaded DICOM file, so as to make room in memory.}
     **/
    void DiscardDicom();

    LAAW_API_INTERNAL void SplitVectorOfFloats(std::vector<float>& target,
                                               const char* tag);

    /**
     * {summary}{Load a raw tag from the DICOM file.}
     * {param}{path The path to the tag of interest (e.g. "0020-000d").}
     **/
    void LoadTagContent(const char* path);

    /**
     * {summary}{Return the value of the raw tag that was loaded by LoadContent.}
     * {returns}{The tag value.}
     **/
    const char* GetLoadedTagContent() const;
  };
}
