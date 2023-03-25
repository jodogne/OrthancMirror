/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "Font.h"

#if !defined(ORTHANC_ENABLE_LOCALE)
#  error ORTHANC_ENABLE_LOCALE must be defined to use this file
#endif

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include "../OrthancException.h"
#include "../Toolbox.h"
#include "Image.h"
#include "ImageProcessing.h"

#include <stdio.h>
#include <memory>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  Font::Font() :
    size_(0),
    maxHeight_(0)
  {
  }


  Font::~Font()
  {
    for (Characters::iterator it = characters_.begin();
         it != characters_.end(); ++it)
    {
      delete it->second;
    }
  }


  void Font::LoadFromMemory(const std::string& font)
  {
    Json::Value v;
    if (!Toolbox::ReadJson(v, font) ||
        v.type() != Json::objectValue ||
        !v.isMember("Name") ||
        !v.isMember("Size") ||
        !v.isMember("Characters") ||
        v["Name"].type() != Json::stringValue ||
        v["Size"].type() != Json::intValue ||
        v["Characters"].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFont);
    }

    name_ = v["Name"].asString();
    size_ = v["Size"].asUInt();
    maxHeight_ = 0;

    Json::Value::Members characters = v["Characters"].getMemberNames();

    for (size_t i = 0; i < characters.size(); i++)
    {
      const Json::Value& info = v["Characters"][characters[i]];
      if (info.type() != Json::objectValue ||
          !info.isMember("Advance") ||
          !info.isMember("Bitmap") ||
          !info.isMember("Height") ||
          !info.isMember("Top") ||
          !info.isMember("Width") ||
          info["Advance"].type() != Json::intValue ||
          info["Bitmap"].type() != Json::arrayValue ||
          info["Height"].type() != Json::intValue ||
          info["Top"].type() != Json::intValue ||
          info["Width"].type() != Json::intValue)
      {
        throw OrthancException(ErrorCode_BadFont);
      }

      std::unique_ptr<Character> c(new Character);
      
      c->advance_ = info["Advance"].asUInt();
      c->height_ = info["Height"].asUInt();
      c->top_ = info["Top"].asUInt();
      c->width_ = info["Width"].asUInt();
      c->bitmap_.resize(info["Bitmap"].size());

      if (c->height_ > maxHeight_)
      {
        maxHeight_ = c->height_;
      }
      
      for (Json::Value::ArrayIndex j = 0; j < info["Bitmap"].size(); j++)
      {
        if (info["Bitmap"][j].type() != Json::intValue)
        {
          throw OrthancException(ErrorCode_BadFont);
        }

        int value = info["Bitmap"][j].asInt();
        if (value < 0 || value > 255)
        {
          throw OrthancException(ErrorCode_BadFont);
        }

        c->bitmap_[j] = static_cast<uint8_t>(value);
      }

      int index = boost::lexical_cast<int>(characters[i]);
      if (index < 0 || index > 255)
      {
        throw OrthancException(ErrorCode_BadFont);
      }

      characters_[static_cast<char>(index)] = c.release();
    }
  }


#if ORTHANC_SANDBOXED == 0
  void Font::LoadFromFile(const std::string& path)
  {
    std::string font;
    SystemToolbox::ReadFile(font, path);
    LoadFromMemory(font);
  }
#endif

  const std::string &Font::GetName() const
  {
    return name_;
  }

  unsigned int Font::GetSize() const
  {
    return size_;
  }


  static unsigned int MyMin(unsigned int a,
                            unsigned int b)
  {
    return a < b ? a : b;
  }


  void Font::DrawCharacter(ImageAccessor& target,
                           const Character& character,
                           int x,
                           int y,
                           const uint8_t color[4]) const
  {
    // Compute the bounds of the character
    if (x >= static_cast<int>(target.GetWidth()) ||
        y >= static_cast<int>(target.GetHeight()))
    {
      // The character is out of the image
      return;
    }

    unsigned int left = x < 0 ? -x : 0;
    unsigned int top = y < 0 ? -y : 0;
    unsigned int width = MyMin(character.width_, target.GetWidth() - x);
    unsigned int height = MyMin(character.height_, target.GetHeight() - y);

    unsigned int bpp = target.GetBytesPerPixel();

    // Blit the font bitmap OVER the target image
    // https://en.wikipedia.org/wiki/Alpha_compositing

    for (unsigned int cy = top; cy < height; cy++)
    {
      uint8_t* p = reinterpret_cast<uint8_t*>(target.GetRow(y + cy)) + (x + left) * bpp;
      unsigned int pos = cy * character.width_ + left;

      switch (target.GetFormat())
      {
        case PixelFormat_Grayscale8:
        {
          assert(bpp == 1);
          for (unsigned int cx = left; cx < width; cx++, pos++, p++)
          {
            uint16_t alpha = character.bitmap_[pos];
            uint16_t value = alpha * static_cast<uint16_t>(color[0]) + (255 - alpha) * static_cast<uint16_t>(*p);
            *p = static_cast<uint8_t>(value >> 8);
          }

          break;
        }

        case PixelFormat_RGB24:
        {
          assert(bpp == 3);
          for (unsigned int cx = left; cx < width; cx++, pos++, p += 3)
          {
            uint16_t alpha = character.bitmap_[pos];
            for (uint8_t i = 0; i < 3; i++)
            {
              uint16_t value = alpha * static_cast<uint16_t>(color[i]) + (255 - alpha) * static_cast<uint16_t>(p[i]);
              p[i] = static_cast<uint8_t>(value >> 8);
            }
          }

          break;
        }

        case PixelFormat_RGBA32:
        case PixelFormat_BGRA32:
        {
          assert(bpp == 4);

          for (unsigned int cx = left; cx < width; cx++, pos++, p += 4)
          {
            float alpha = static_cast<float>(character.bitmap_[pos]) / 255.0f;
            float beta = (1.0f - alpha) * static_cast<float>(p[3]) / 255.0f;
            float denom = 1.0f / (alpha + beta);

            for (uint8_t i = 0; i < 3; i++)
            {
              p[i] = static_cast<uint8_t>((alpha * static_cast<float>(color[i]) +
                                           beta * static_cast<float>(p[i])) * denom);
            }

            p[3] = static_cast<uint8_t>(255.0f * (alpha + beta));
          }

          break;
        }

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }

  }


  void Font::DrawInternal(ImageAccessor& target,
                          const std::string& utf8,
                          int x,
                          int y,
                          const uint8_t color[4]) const
  {
    if (target.GetFormat() != PixelFormat_Grayscale8 &&
        target.GetFormat() != PixelFormat_RGB24 &&
        target.GetFormat() != PixelFormat_RGBA32 &&
        target.GetFormat() != PixelFormat_BGRA32)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    int a = x;

#if ORTHANC_ENABLE_LOCALE == 1
    std::string s = Toolbox::ConvertFromUtf8(utf8, Encoding_Latin1);
#else
    // If the locale support is disabled, simply drop non-ASCII
    // characters from the source UTF-8 string
    std::string s = Toolbox::ConvertToAscii(utf8);
#endif

    for (size_t i = 0; i < s.size(); i++)
    {
      if (s[i] == '\n')
      {
        // Go to the next line
        a = x;
        y += maxHeight_ + 1;
      }
      else
      {
        Characters::const_iterator c = characters_.find(s[i]);
        if (c != characters_.end())
        {
          DrawCharacter(target, *c->second, a, y + static_cast<int>(c->second->top_), color);
          a += c->second->advance_;
        }
      }
    }
  }


  void Font::Draw(ImageAccessor& target,
                  const std::string& utf8,
                  int x,
                  int y,
                  uint8_t grayscale) const
  {
    uint8_t color[4] = { grayscale, grayscale, grayscale, 255 };
    DrawInternal(target, utf8, x, y, color);
  }


  void Font::Draw(ImageAccessor& target,
                  const std::string& utf8,
                  int x,
                  int y,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b) const
  {
    uint8_t color[4];

    switch (target.GetFormat())
    {
      case PixelFormat_BGRA32:
        color[0] = b;
        color[1] = g;
        color[2] = r;
        color[3] = 255;
        break;

      default:
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = 255;
        break;
    }
    
    DrawInternal(target, utf8, x, y, color);
  }


  void Font::ComputeTextExtent(unsigned int& width,
                               unsigned int& height,
                               const std::string& utf8) const
  {
    width = 0;
    height = 0;
    
#if ORTHANC_ENABLE_LOCALE == 1
    std::string s = Toolbox::ConvertFromUtf8(utf8, Encoding_Latin1);
#else
    // If the locale support is disabled, simply drop non-ASCII
    // characters from the source UTF-8 string
    std::string s = Toolbox::ConvertToAscii(utf8);
#endif

    // Compute the text extent
    unsigned int x = 0;
    unsigned int y = 0;
    
    for (size_t i = 0; i < s.size(); i++)
    {
      if (s[i] == '\n')
      {
        // Go to the next line
        x = 0;
        y += (maxHeight_ + 1);
      }
      else
      {
        Characters::const_iterator c = characters_.find(s[i]);
        if (c != characters_.end())
        {
          x += c->second->advance_;

          unsigned int bottom = y + c->second->top_ + c->second->height_;
          if (bottom > height)
          {
            height = bottom;
          }
          
          if (x > width)
          {
            width = x;
          }
        }
      }
    }
  }


  ImageAccessor* Font::Render(const std::string& utf8,
                              PixelFormat format,
                              uint8_t r,
                              uint8_t g,
                              uint8_t b) const
  {
    unsigned int width, height;
    ComputeTextExtent(width, height, utf8);
    
    std::unique_ptr<ImageAccessor>  target(new Image(format, width, height, false));
    ImageProcessing::Set(*target, 0, 0, 0, 255);
    Draw(*target, utf8, 0, 0, r, g, b);

    return target.release();
  }


  ImageAccessor* Font::RenderAlpha(const std::string& utf8) const
  {
    unsigned int width, height;
    ComputeTextExtent(width, height, utf8);

    std::unique_ptr<ImageAccessor>  target(new Image(PixelFormat_Grayscale8, width, height, false));
    ImageProcessing::Set(*target, 0);
    Draw(*target, utf8, 0, 0, 255);

    return target.release();
  }
}
