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


#include "../PrecompiledHeaders.h"
#include "FontRegistry.h"

#include "../OrthancException.h"

#include <memory>

namespace Orthanc
{
  FontRegistry::~FontRegistry()
  {
    for (Fonts::iterator it = fonts_.begin(); it != fonts_.end(); ++it)
    {
      delete *it;
    }
  }


  void FontRegistry::AddFromMemory(const std::string& font)
  {
    std::unique_ptr<Font> f(new Font);
    f->LoadFromMemory(font);
    fonts_.push_back(f.release());
  }


#if ORTHANC_SANDBOXED == 0
  void FontRegistry::AddFromFile(const std::string& path)
  {
    std::unique_ptr<Font> f(new Font);
    f->LoadFromFile(path);
    fonts_.push_back(f.release());
  }
#endif

  size_t FontRegistry::GetSize() const
  {
    return fonts_.size();
  }

  const Font& FontRegistry::GetFont(size_t i) const
  {
    if (i >= fonts_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return *fonts_[i];
    }
  }

  const Font* FontRegistry::FindFont(const std::string& fontName) const
  {
    for (Fonts::const_iterator it = fonts_.begin(); it != fonts_.end(); ++it)
    {
      if ((*it)->GetName() == fontName)
      {
        return *it;
      }
    }

    return NULL;
  }
}
