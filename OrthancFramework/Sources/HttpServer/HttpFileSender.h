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

#include "HttpOutput.h"
#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

namespace Orthanc
{
  class ORTHANC_PUBLIC HttpFileSender : public IHttpStreamAnswer
  {
  private:
    std::string contentType_;
    std::string filename_;

  public:
    void SetContentType(MimeType contentType);

    void SetContentType(const std::string& contentType);

    const std::string& GetContentType() const;

    void SetContentFilename(const std::string& filename);

    const std::string& GetContentFilename() const;


    /**
     * Implementation of the IHttpStreamAnswer interface.
     **/

    virtual HttpCompression SetupHttpCompression(bool /*gzipAllowed*/,
                                                 bool /*deflateAllowed*/) ORTHANC_OVERRIDE;

    virtual bool HasContentFilename(std::string& filename) ORTHANC_OVERRIDE;
    
    virtual std::string GetContentType() ORTHANC_OVERRIDE;
  };
}
