/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "DicomInstanceToStore.h"

namespace Orthanc
{
  DicomInstanceToStore::DicomInstanceToStore() :
    hasBuffer_(false),
    parsed_(NULL),
    summary_(NULL),
    json_(NULL)
  {
  }

  void DicomInstanceToStore::SetBuffer(const std::string& dicom)
  {
    hasBuffer_ = true;
    bufferSize_ = dicom.size();

    if (dicom.size() == 0)
    {
      buffer_ = NULL;
    }
    else
    {
      buffer_ = &dicom[0];
    }
  }


  void DicomInstanceToStore::SetBuffer(const char* buffer, 
                                       size_t size)
  {
    hasBuffer_ = true;
    buffer_ = buffer;
    bufferSize_ = size;
  }


  void DicomInstanceToStore::SetMetadata(ResourceType level,
                                         MetadataType metadata,
                                         const std::string& value)
  {
    metadata_[std::make_pair(level, metadata)] = value;
  }


  void DicomInstanceToStore::ComputeMissingInformation()
  {
    // TODO

    assert(hasBuffer_ && (buffer_ != NULL || bufferSize_ == 0));
  }



  const char* DicomInstanceToStore::GetBuffer()
  {
    ComputeMissingInformation();
    return buffer_;
  }


  size_t DicomInstanceToStore::GetBufferSize()
  {
    ComputeMissingInformation();
    return bufferSize_;
  }
}
