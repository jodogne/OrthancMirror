/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "IImageWriter.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

namespace Orthanc
{
#if ORTHANC_SANDBOXED == 0
  void IImageWriter::WriteToFileInternal(const std::string& path,
                                         unsigned int width,
                                         unsigned int height,
                                         unsigned int pitch,
                                         PixelFormat format,
                                         const void* buffer)
  {
    std::string compressed;
    WriteToMemoryInternal(compressed, width, height, pitch, format, buffer);
    SystemToolbox::WriteFile(compressed, path);
  }
#endif

  void IImageWriter::WriteToMemory(IImageWriter& writer,
                                   std::string &compressed,
                                   const ImageAccessor &accessor)
  {
    writer.WriteToMemoryInternal(compressed, accessor.GetWidth(), accessor.GetHeight(),
                                 accessor.GetPitch(), accessor.GetFormat(), accessor.GetConstBuffer());
  }

#if ORTHANC_SANDBOXED == 0
  void IImageWriter::WriteToFile(IImageWriter& writer,
                                 const std::string &path,
                                 const ImageAccessor &accessor)
  {
    writer.WriteToFileInternal(path, accessor.GetWidth(), accessor.GetHeight(),
                               accessor.GetPitch(), accessor.GetFormat(), accessor.GetConstBuffer());
  }
#endif
}
