/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "HierarchicalZipWriter.h"

#include "../Toolbox.h"
#include "../OrthancException.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  std::string HierarchicalZipWriter::Index::GetCurrentDirectoryPath() const
  {
    std::string result;

    Stack::const_iterator it = stack_.begin();
    ++it;  // Skip the root node (to avoid absolute paths)

    while (it != stack_.end())
    {
      result += (*it)->name_ + "/";
      ++it;
    }

    return result;
  }

  std::string HierarchicalZipWriter::Index::EnsureUniqueFilename(const std::string& filename,
                                                                 bool allowUtf8)
  {
    const std::string standardized = Toolbox::NormalizePath(filename, allowUtf8);

    if (standardized.find('/') != std::string::npos)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, "Your filename must not contain slashes or backslashes");
    }

    Directory& d = *stack_.back();
    Directory::Content::iterator it = d.content_.find(standardized);

    if (it == d.content_.end())
    {
      d.content_[standardized] = 1;
      return standardized;
    }
    else
    {
      it->second++;
      return standardized + "-" + boost::lexical_cast<std::string>(it->second);
    }    
  }

  HierarchicalZipWriter::Index::Index()
  {
    stack_.push_back(new Directory);
  }

  HierarchicalZipWriter::Index::~Index()
  {
    for (Stack::iterator it = stack_.begin(); it != stack_.end(); ++it)
    {
      delete *it;
    }
  }

  bool HierarchicalZipWriter::Index::IsRoot() const
  {
    return stack_.size() == 1;
  }

  std::string HierarchicalZipWriter::Index::OpenFile(const std::string& name,
                                                     bool allowUtf8)
  {
    return GetCurrentDirectoryPath() + EnsureUniqueFilename(name, allowUtf8);
  }

  void HierarchicalZipWriter::Index::OpenDirectory(const std::string& name,
                                                   bool allowUtf8)
  {
    std::string d = EnsureUniqueFilename(name, allowUtf8);

    // Push the new directory onto the stack
    stack_.push_back(new Directory);
    stack_.back()->name_ = d;
  }

  void HierarchicalZipWriter::Index::CloseDirectory()
  {
    if (IsRoot())
    {
      // Cannot close the root node
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    delete stack_.back();
    stack_.pop_back();
  }


  HierarchicalZipWriter::HierarchicalZipWriter(const boost::filesystem::path& path)
  {
    writer_.SetOutputPath(path);
    writer_.Open();
  }

  
  HierarchicalZipWriter::HierarchicalZipWriter(ZipWriter::IOutputStream* stream,
                                               bool isZip64)
  {
    writer_.AcquireOutputStream(stream, isZip64);
    writer_.Open();    
  }


  HierarchicalZipWriter::~HierarchicalZipWriter()
  {
    writer_.Close();
  }

  void HierarchicalZipWriter::SetZip64(bool isZip64)
  {
    writer_.SetZip64(isZip64);
  }

  bool HierarchicalZipWriter::IsZip64() const
  {
    return writer_.IsZip64();
  }

  void HierarchicalZipWriter::SetCompressionLevel(uint8_t level)
  {
    writer_.SetCompressionLevel(level);
  }

  uint8_t HierarchicalZipWriter::GetCompressionLevel() const
  {
    return writer_.GetCompressionLevel();
  }

  void HierarchicalZipWriter::SetAppendToExisting(bool append)
  {
    writer_.SetAppendToExisting(append);
  }

  bool HierarchicalZipWriter::IsAppendToExisting() const
  {
    return writer_.IsAppendToExisting();
  }

  void HierarchicalZipWriter::OpenFile(const std::string& name)
  {
    std::string p = indexer_.OpenFile(name, IsAllowUtf8());
    writer_.OpenFile(p.c_str());
  }

  void HierarchicalZipWriter::OpenDirectory(const std::string& name)
  {
    indexer_.OpenDirectory(name, IsAllowUtf8());
  }

  void HierarchicalZipWriter::CloseDirectory()
  {
    indexer_.CloseDirectory();
  }

  std::string HierarchicalZipWriter::GetCurrentDirectoryPath() const
  {
    return indexer_.GetCurrentDirectoryPath();
  }

  void HierarchicalZipWriter::Write(const void *data,
                                    size_t length)
  {
    writer_.Write(data, length);
  }

  void HierarchicalZipWriter::Write(const std::string& data)
  {
    writer_.Write(data);
  }

  HierarchicalZipWriter* HierarchicalZipWriter::CreateToMemory(std::string& target,
                                                               bool isZip64)
  {
    return new HierarchicalZipWriter(new ZipWriter::MemoryStream(target), isZip64);
  }

  void HierarchicalZipWriter::CancelStream()
  {
    writer_.CancelStream();
  }

  void HierarchicalZipWriter::Close()
  {
    writer_.Close();
  }

  uint64_t HierarchicalZipWriter::GetArchiveSize() const
  {
    return writer_.GetArchiveSize();
  }
}
