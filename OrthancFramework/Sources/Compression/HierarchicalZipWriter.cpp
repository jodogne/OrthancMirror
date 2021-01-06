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


#include "../PrecompiledHeaders.h"
#include "HierarchicalZipWriter.h"

#include "../Toolbox.h"
#include "../OrthancException.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  std::string HierarchicalZipWriter::Index::KeepAlphanumeric(const std::string& source)
  {
    std::string result;

    bool lastSpace = false;

    result.reserve(source.size());
    for (size_t i = 0; i < source.size(); i++)
    {
      char c = source[i];
      if (c == '^')
        c = ' ';

      if (c <= 127 && 
          c >= 0)
      {
        if (isspace(c)) 
        {
          if (!lastSpace)
          {
            lastSpace = true;
            result.push_back(' ');
          }
        }
        else if (isalnum(c) || 
                 c == '.' || 
                 c == '_')
        {
          result.push_back(c);
          lastSpace = false;
        }
      }
    }

    return Toolbox::StripSpaces(result);
  }

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

  std::string HierarchicalZipWriter::Index::EnsureUniqueFilename(const char* filename)
  {
    std::string standardized = KeepAlphanumeric(filename);

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

  std::string HierarchicalZipWriter::Index::OpenFile(const char* name)
  {
    return GetCurrentDirectoryPath() + EnsureUniqueFilename(name);
  }

  void HierarchicalZipWriter::Index::OpenDirectory(const char* name)
  {
    std::string d = EnsureUniqueFilename(name);

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


  HierarchicalZipWriter::HierarchicalZipWriter(const char* path)
  {
    writer_.SetOutputPath(path);
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

  void HierarchicalZipWriter::OpenFile(const char* name)
  {
    std::string p = indexer_.OpenFile(name);
    writer_.OpenFile(p.c_str());
  }

  void HierarchicalZipWriter::OpenDirectory(const char* name)
  {
    indexer_.OpenDirectory(name);
  }

  void HierarchicalZipWriter::CloseDirectory()
  {
    indexer_.CloseDirectory();
  }

  std::string HierarchicalZipWriter::GetCurrentDirectoryPath() const
  {
    return indexer_.GetCurrentDirectoryPath();
  }

  void HierarchicalZipWriter::Write(const void *data, size_t length)
  {
    writer_.Write(data, length);
  }

  void HierarchicalZipWriter::Write(const std::string &data)
  {
    writer_.Write(data);
  }
}
