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


#pragma once

#include <stdint.h>
#include <string>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class ZipWriter
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    bool isZip64_;
    bool hasFileInZip_;
    bool append_;
    uint8_t compressionLevel_;
    std::string path_;

  public:
    ZipWriter();

    ~ZipWriter();

    void SetZip64(bool isZip64);

    bool IsZip64() const
    {
      return isZip64_;
    }

    void SetCompressionLevel(uint8_t level);

    uint8_t GetCompressionLevel() const
    {
      return compressionLevel_;
    }

    void SetAppendToExisting(bool append);
    
    bool IsAppendToExisting() const
    {
      return append_;
    }
    
    void Open();

    void Close();

    bool IsOpen() const;

    void SetOutputPath(const char* path);

    const std::string& GetOutputPath() const
    {
      return path_;
    }

    void OpenFile(const char* path);

    void Write(const char* data, size_t length);

    void Write(const std::string& data);
  };
}
