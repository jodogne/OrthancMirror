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

#include "../OrthancFramework.h"  // For ORTHANC_PUBLIC

#include <boost/noncopyable.hpp>
#include <istream>
#include <stdint.h>
#include <string>


namespace Orthanc
{
  /**
   * This class is used to extract blocks of given size from a
   * stream. Bytes from the stream are buffered until the requested
   * size is available, and the full block can be returned.
   **/
  class ORTHANC_PUBLIC StreamBlockReader : public boost::noncopyable
  {
  private:
    std::istream&  stream_;
    std::string    block_;
    size_t         blockPos_;
    uint64_t       processedBytes_;

  public:
    explicit StreamBlockReader(std::istream& stream);

    /**
     * Schedule the size of the next block to be extracted from the
     * stream.
     **/
    void Schedule(size_t blockSize);

    /**
     * Extract the block whose size was configured by the previous
     * call to "Schedule()". Returns "false" iff not enough bytes are
     * available from the stream yet: In this case, try again later.
     **/
    bool Read(std::string& block);
    
    uint64_t GetProcessedBytes() const;
  };
}
