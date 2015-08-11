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

#include "HttpFileSender.h"
#include "../FileStorage/FilesystemStorage.h"

#include <fstream>

namespace Orthanc
{
  class FilesystemHttpSender : public HttpFileSender
  {
  private:
    boost::filesystem::path path_;
    std::ifstream           file_;
    uint64_t                size_;
    std::string             chunk_;
    size_t                  chunkSize_;

    void Open();

  public:
    FilesystemHttpSender(const char* path);

    FilesystemHttpSender(const boost::filesystem::path& path);

    FilesystemHttpSender(const FilesystemStorage& storage,
                         const std::string& uuid);


    /**
     * Implementation of the IHttpStreamAnswer interface.
     **/

    virtual uint64_t GetContentLength()
    {
      return size_;
    }

    virtual bool ReadNextChunk();

    virtual const char* GetChunkContent()
    {
      return chunk_.c_str();
    }

    virtual size_t GetChunkSize()
    {
      return chunkSize_;
    }
  };
}
