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


#include "BufferCompressor.h"

namespace Orthanc
{
  void BufferCompressor::Compress(std::string& output,
                                  const std::vector<uint8_t>& input)
  {
    if (input.size() > 0)
      Compress(output, &input[0], input.size());
    else
      Compress(output, NULL, 0);
  }

  void BufferCompressor::Uncompress(std::string& output,
                                    const std::vector<uint8_t>& input)
  {
    if (input.size() > 0)
      Uncompress(output, &input[0], input.size());
    else
      Uncompress(output, NULL, 0);
  }

  void BufferCompressor::Compress(std::string& output,
                                  const std::string& input)
  {
    if (input.size() > 0)
      Compress(output, &input[0], input.size());
    else
      Compress(output, NULL, 0);
  }

  void BufferCompressor::Uncompress(std::string& output,
                                    const std::string& input)
  {
    if (input.size() > 0)
      Uncompress(output, &input[0], input.size());
    else
      Uncompress(output, NULL, 0);
  }
}
