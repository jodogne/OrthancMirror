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


#pragma once

#include "../OrthancFramework.h"

#include "ImageAccessor.h"

#include <stdint.h>
#include <vector>
#include <map>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC Font : public boost::noncopyable
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
    Font();

    ~Font();

    void LoadFromMemory(const std::string& font);

#if ORTHANC_SANDBOXED == 0
    void LoadFromFile(const std::string& path);
#endif

    const std::string& GetName() const;

    unsigned int GetSize() const;

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

    void ComputeTextExtent(unsigned int& width,
                           unsigned int& height,
                           const std::string& utf8) const;

    ImageAccessor* Render(const std::string& utf8,
                          PixelFormat format,
                          uint8_t r,
                          uint8_t g,
                          uint8_t b) const;

    ImageAccessor* RenderAlpha(const std::string& utf8) const;
  };
}
