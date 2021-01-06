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

#if !defined(ORTHANC_ENABLE_PNG)
#  error The macro ORTHANC_ENABLE_PNG must be defined
#endif

#if ORTHANC_ENABLE_PNG != 1
#  error PNG support must be enabled to include this file
#endif

#include "ImageAccessor.h"

#include "../Enumerations.h"

#include <vector>
#include <stdint.h>
#include <boost/shared_ptr.hpp>

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

namespace Orthanc
{
  class ORTHANC_PUBLIC PngReader : public ImageAccessor
  {
  private:
    struct PngRabi;

    std::vector<uint8_t> data_;

    void CheckHeader(const void* header);

    void Read(PngRabi& rabi);

  public:
    PngReader();

#if ORTHANC_SANDBOXED == 0
    void ReadFromFile(const std::string& filename);
#endif

    void ReadFromMemory(const void* buffer,
                        size_t size);

    void ReadFromMemory(const std::string& buffer);
  };
}
