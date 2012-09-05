/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "FileStorage.h"

// http://stackoverflow.com/questions/1576272/storing-large-number-of-files-in-file-system
// http://stackoverflow.com/questions/446358/storing-a-large-number-of-images

#include "PalanthirException.h"
#include "Toolbox.h"
#include "Uuid.h"

#include <boost/filesystem/fstream.hpp>

namespace Palanthir
{
  boost::filesystem::path FileStorage::GetPath(const std::string& uuid) const
  {
    namespace fs = boost::filesystem;

    if (!Toolbox::IsUuid(uuid))
    {
      throw PalanthirException(ErrorCode_ParameterOutOfRange);
    }

    fs::path path = root_;

    path /= std::string(&uuid[0], &uuid[2]);
    path /= std::string(&uuid[2], &uuid[4]);
    path /= uuid;
    path.make_preferred();

    return path;
  }

  FileStorage::FileStorage(std::string root)
  {
    namespace fs = boost::filesystem;

    root_ = fs::absolute(root);

    if (fs::exists(root))
    {
      if (!fs::is_directory(root))
      {
        throw PalanthirException("The file storage root directory is a file");
      }
    }
    else
    {
      if (!fs::create_directories(root))
      {
        throw PalanthirException("Unable to create the file storage root directory");
      }
    }
  }

  std::string FileStorage::CreateFileWithoutCompression(const void* content, size_t size)
  {
    std::string uuid;
    boost::filesystem::path path;
    
    for (;;)
    {
      uuid = Toolbox::GenerateUuid();
      path = GetPath(uuid);

      if (!boost::filesystem::exists(path))
      {
        // OK, this is indeed a new file
        break;
      }

      // Extremely improbable case: This Uuid has already been created
      // in the past. Try again.
    }

    if (boost::filesystem::exists(path.parent_path()))
    {
      if (!boost::filesystem::is_directory(path.parent_path()))
      {
        throw PalanthirException("The subdirectory to be created is already occupied by a regular file");        
      }
    }
    else
    {
      if (!boost::filesystem::create_directories(path.parent_path()))
      {
        throw PalanthirException("Unable to create a subdirectory in the file storage");        
      }
    }

    boost::filesystem::ofstream f;
    f.open(path, std::ofstream::out | std::ios::binary);
    if (!f.good())
    {
      throw PalanthirException("Unable to create a new file in the file storage");
    }

    if (size != 0)
    {
      f.write(static_cast<const char*>(content), size);
      if (!f.good())
      {
        f.close();
        throw PalanthirException("Unable to write to the new file in the file storage");
      }
    }

    f.close();

    return uuid;
  } 


  std::string FileStorage::Create(const void* content, size_t size)
  {
    if (!HasBufferCompressor() || size == 0)
    {
      return CreateFileWithoutCompression(content, size);
    }
    else
    {
      std::string compressed;
      compressor_->Compress(compressed, content, size);
      assert(compressed.size() > 0);
      return CreateFileWithoutCompression(&compressed[0], compressed.size());
    }
  }


  std::string FileStorage::Create(const std::vector<uint8_t>& content)
  {
    if (content.size() == 0)
      return Create(NULL, 0);
    else
      return Create(&content[0], content.size());
  }

  std::string FileStorage::Create(const std::string& content)
  {
    if (content.size() == 0)
      return Create(NULL, 0);
    else
      return Create(&content[0], content.size());
  }

  void FileStorage::ReadFile(std::string& content,
                             const std::string& uuid) const
  {
    content.clear();

    if (HasBufferCompressor())
    {
      std::string compressed;
      Toolbox::ReadFile(compressed, GetPath(uuid).string());

      if (compressed.size() != 0)
      {
        compressor_->Uncompress(content, compressed);
      }
    }
    else
    {
      Toolbox::ReadFile(content, GetPath(uuid).string());
    }
  }


  uintmax_t FileStorage::GetCompressedSize(const std::string& uuid) const
  {
    boost::filesystem::path path = GetPath(uuid);
    return boost::filesystem::file_size(path);
  }



  void FileStorage::ListAllFiles(std::set<std::string>& result) const
  {
    namespace fs = boost::filesystem;

    result.clear();

    if (fs::exists(root_) && fs::is_directory(root_))
    {
      for (fs::recursive_directory_iterator current(root_), end; current != end ; ++current)
      {
        if (fs::is_regular_file(current->status()))
        {
          try
          {
            fs::path d = current->path();
            std::string uuid = d.filename().string();
            if (Toolbox::IsUuid(uuid))
            {
              fs::path p0 = d.parent_path().parent_path().parent_path();
              std::string p1 = d.parent_path().parent_path().filename().string();
              std::string p2 = d.parent_path().filename().string();
              if (p1.length() == 2 &&
                  p2.length() == 2 &&
                  p1 == uuid.substr(0, 2) &&
                  p2 == uuid.substr(2, 2) &&
                  p0 == root_)
              {
                result.insert(uuid);
              }
            }
          }
          catch (fs::filesystem_error)
          {
          }
        }
      }
    }
  }


  void FileStorage::Clear()
  {
    namespace fs = boost::filesystem;
    typedef std::set<std::string> List;

    List result;
    ListAllFiles(result);

    for (List::const_iterator it = result.begin(); it != result.end(); it++)
    {
      Remove(*it);
    }
  }


  void FileStorage::Remove(const std::string& uuid)
  {
    namespace fs = boost::filesystem;

    fs::path p = GetPath(uuid);
    fs::remove(p);

    // Remove the two parent directories, ignoring the error code if
    // these directories are not empty
    boost::system::error_code err;
    fs::remove(p.parent_path(), err);
    fs::remove(p.parent_path().parent_path(), err);
  }


  uintmax_t FileStorage::GetCapacity() const
  {
    return boost::filesystem::space(root_).capacity;
  }

  uintmax_t FileStorage::GetAvailableSpace() const
  {
    return boost::filesystem::space(root_).available;
  }
}
