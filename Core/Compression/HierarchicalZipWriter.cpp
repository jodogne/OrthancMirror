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
}
