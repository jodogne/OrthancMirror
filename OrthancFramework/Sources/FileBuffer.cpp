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


#include "PrecompiledHeaders.h"
#include "FileBuffer.h"

#include "TemporaryFile.h"
#include "OrthancException.h"

#include <boost/filesystem/fstream.hpp>


namespace Orthanc
{
  class FileBuffer::PImpl
  {
  private:
    TemporaryFile                file_;
    boost::filesystem::ofstream  stream_;
    bool                         isWriting_;

  public:
    PImpl() :
      isWriting_(true)
    {
      stream_.open(file_.GetPath(), std::ofstream::out | std::ofstream::binary);
      if (!stream_.good())
      {
        throw OrthancException(ErrorCode_CannotWriteFile);
      }
    }

    ~PImpl()
    {
      if (isWriting_)
      {
        stream_.close();
      }
    }

    void Append(const char* buffer,
                size_t size)
    {
      if (!isWriting_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      if (size > 0)
      {
        stream_.write(buffer, size);
        if (!stream_.good())
        {
          stream_.close();
          throw OrthancException(ErrorCode_FileStorageCannotWrite);
        }
      }
    }

    void Read(std::string& target)
    {
      if (isWriting_)
      {
        stream_.close();
        isWriting_ = false;
      }

      file_.Read(target);
    }
  };

    
  FileBuffer::FileBuffer() :
    pimpl_(new PImpl)
  {
  }


  void FileBuffer::Append(const char* buffer,
                          size_t size)
  {
    assert(pimpl_.get() != NULL);
    pimpl_->Append(buffer, size);
  }


  void FileBuffer::Read(std::string& target)
  {
    assert(pimpl_.get() != NULL);
    pimpl_->Read(target);
  }
}
