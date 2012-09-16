/**
 * Orthanc - A Lightweight, RESTful DICOM Store
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


#pragma once

#include <string>

/**
 * GUID vs. UUID
 * The simple answer is: no difference, they are the same thing. Treat
 * them as a 16 byte (128 bits) value that is used as a unique
 * value. In Microsoft-speak they are called GUIDs, but call them
 * UUIDs when not using Microsoft-speak.
 * http://stackoverflow.com/questions/246930/is-there-any-difference-between-a-guid-and-a-uuid
 **/

namespace Orthanc
{
  namespace Toolbox
  {
    std::string GenerateUuid();

    bool IsUuid(const std::string& str);
  }
}
