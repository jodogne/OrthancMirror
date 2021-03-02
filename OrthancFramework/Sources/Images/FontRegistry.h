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


#pragma once

#include "Font.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC FontRegistry : public boost::noncopyable
  {
  private:
    typedef std::vector<Font*>  Fonts;

    Fonts  fonts_;

  public:
    ~FontRegistry();

    void AddFromMemory(const std::string& font);

#if ORTHANC_SANDBOXED == 0
    void AddFromFile(const std::string& path);
#endif

    size_t GetSize() const;

    const Font& GetFont(size_t i) const;

    const Font* FindFont(const std::string& fontName) const;
  };
}
