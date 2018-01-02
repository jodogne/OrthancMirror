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

#include "ImageAccessor.h"

#include <stdint.h>
#include <vector>
#include <map>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class Font : public boost::noncopyable
  {
  private:
    struct Character
    {
      unsigned int  width_;
      unsigned int  height_;
      unsigned int  top_;
      unsigned int  advance_;
      std::vector<uint8_t>  bitmap_;
    };

    typedef std::map<char, Character*>  Characters;

    std::string   name_;
    unsigned int  size_;
    Characters    characters_;
    unsigned int  maxHeight_;

    void DrawCharacter(ImageAccessor& target,
                       const Character& character,
                       int x,
                       int y,
                       const uint8_t color[4]) const;

    void DrawInternal(ImageAccessor& target,
                      const std::string& utf8,
                      int x,
                      int y,
                      const uint8_t color[4]) const;

  public:
    Font() : 
      size_(0), 
      maxHeight_(0)
    {
    }

    ~Font();

    void LoadFromMemory(const std::string& font);

#if ORTHANC_SANDBOXED == 0
    void LoadFromFile(const std::string& path);
#endif

    const std::string& GetName() const
    {
      return name_;
    }

    unsigned int GetSize() const
    {
      return size_;
    }

    void Draw(ImageAccessor& target,
              const std::string& utf8,
              int x,
              int y,
              uint8_t grayscale) const;

    void Draw(ImageAccessor& target,
              const std::string& utf8,
              int x,
              int y,
              uint8_t r,
              uint8_t g,
              uint8_t b) const;
  };
}
