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

#include "ImageAccessor.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

namespace Orthanc
{
  class ORTHANC_PUBLIC PamReader : public ImageAccessor
  {
  private:
    void ParseContent();
    
    /**
    Whether we want to use the default malloc alignment in the image buffer,
    at the expense of an extra copy
    */
    bool enforceAligned_;

    /**
    This is actually a copy of wrappedContent_, but properly aligned.

    It is only used if the enforceAligned parameter is set to true in the
    constructor.
    */
    void* alignedImageBuffer_;
    
    /**
    Points somewhere in the content_ buffer.      
    */
    ImageAccessor wrappedContent_;

    /**
    Raw content (file bytes or answer from the server, for instance). 
    */
    std::string content_;

  public:
    /**
    See doc for field enforceAligned_. Setting "enforceAligned" is slower,
    but avoids possible crashes due to non-aligned memory access. It is
    recommended to set this parameter to "true".
    */
    explicit PamReader(bool enforceAligned);

    virtual ~PamReader();

#if ORTHANC_SANDBOXED == 0
    void ReadFromFile(const std::string& filename);
#endif

    void ReadFromMemory(const std::string& buffer);

    void ReadFromMemory(const void* buffer,
                        size_t size);
  };
}
