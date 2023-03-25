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
#include "PamReader.h"

#include "../Endianness.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include <stdlib.h>  // For malloc/free
#include <boost/algorithm/string/find.hpp>
#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  static void GetPixelFormat(PixelFormat& format,
                             unsigned int& bytesPerChannel,
                             const unsigned int& maxValue,
                             const unsigned int& channelCount,
                             const std::string& tupleType)
  {
    if (tupleType == "GRAYSCALE" &&
        channelCount == 1)
    {
      switch (maxValue)
      {
        case 255:
          format = PixelFormat_Grayscale8;
          bytesPerChannel = 1;
          return;

        case 65535:
          format = PixelFormat_Grayscale16;
          bytesPerChannel = 2;
          return;

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }
    else if (tupleType == "RGB" &&
             channelCount == 3)
    {
      switch (maxValue)
      {
        case 255:
          format = PixelFormat_RGB24;
          bytesPerChannel = 1;
          return;

        case 65535:
          format = PixelFormat_RGB48;
          bytesPerChannel = 2;
          return;

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  
  typedef std::map<std::string, std::string>  Parameters;

  
  static std::string LookupStringParameter(const Parameters& parameters,
                                           const std::string& key)
  {
    Parameters::const_iterator found = parameters.find(key);

    if (found == parameters.end())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else
    {
      return found->second;
    }
  }
  

  static unsigned int LookupIntegerParameter(const Parameters& parameters,
                                             const std::string& key)
  {
    try
    {
      int value = boost::lexical_cast<int>(LookupStringParameter(parameters, key));

      if (value < 0)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        return static_cast<unsigned int>(value);
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
  

  void PamReader::ParseContent()
  {
    static const std::string headerDelimiter = "ENDHDR\n";
    
    boost::iterator_range<std::string::const_iterator> headerRange =
      boost::algorithm::find_first(content_, headerDelimiter);

    if (!headerRange)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    std::string header(static_cast<const std::string&>(content_).begin(), headerRange.begin());

    std::vector<std::string> lines;
    Toolbox::TokenizeString(lines, header, '\n');

    if (lines.size() < 2 ||
        lines.front() != "P7" ||
        !lines.back().empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Parameters parameters;
    
    for (size_t i = 1; i + 1 < lines.size(); i++)
    {
      std::vector<std::string> tokens;
      Toolbox::TokenizeString(tokens, lines[i], ' ');

      if (tokens.size() != 2)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        parameters[tokens[0]] = tokens[1];
      }
    }

    const unsigned int width = LookupIntegerParameter(parameters, "WIDTH");
    const unsigned int height = LookupIntegerParameter(parameters, "HEIGHT");
    const unsigned int channelCount = LookupIntegerParameter(parameters, "DEPTH");
    const unsigned int maxValue = LookupIntegerParameter(parameters, "MAXVAL");
    const std::string tupleType = LookupStringParameter(parameters, "TUPLTYPE");

    unsigned int bytesPerChannel;
    PixelFormat format;
    GetPixelFormat(format, bytesPerChannel, maxValue, channelCount, tupleType);

    unsigned int pitch = width * channelCount * bytesPerChannel;

    if (content_.size() != header.size() + headerDelimiter.size() + pitch * height)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    size_t offset = content_.size() - pitch * height;

    {
      intptr_t bufferAddr = reinterpret_cast<intptr_t>(&content_[offset]);
      if((bufferAddr % 8) == 0)
        LOG(TRACE) << "PamReader::ParseContent() image address = " << bufferAddr;
      else
        LOG(TRACE) << "PamReader::ParseContent() image address = " << bufferAddr << " (not a multiple of 8!)";
    }
    
    // if we want to enforce alignment, we need to use a freshly allocated
    // buffer, since we have no alignment guarantees on the original one
    if (enforceAligned_)
    {
      if (alignedImageBuffer_ != NULL)
        free(alignedImageBuffer_);
      alignedImageBuffer_ = malloc(pitch * height);
      memcpy(alignedImageBuffer_, &content_[offset], pitch* height);
      content_ = "";
      AssignWritable(format, width, height, pitch, alignedImageBuffer_);
    }
    else
    {
      AssignWritable(format, width, height, pitch, &content_[offset]);
    }

    // Byte swapping if needed
    if (bytesPerChannel != 1 &&
        bytesPerChannel != 2)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (Toolbox::DetectEndianness() == Endianness_Little &&
        bytesPerChannel == 2)
    {
      for (unsigned int h = 0; h < height; ++h)
      {
        uint16_t* pixel = reinterpret_cast<uint16_t*>(GetRow(h));
        
        for (unsigned int w = 0; w < width; ++w, ++pixel)
        {
          /**
           * This is Little-Endian computer, and PAM uses
           * Big-Endian. Need to do a 16-bit swap. We DON'T use
           * "htobe16()", as the latter only works if the "pixel"
           * pointer is 16-bit aligned (which is not the case if
           * "offset" is an odd number), and the trick that was used
           * in Orthanc <= 1.8.0 (i.e. make a "memcpy()" to a local
           * uint16_t variable) doesn't seem work for WebAssembly. We
           * thus use a plain old C implementation. Check out issue
           * #99: https://bugs.orthanc-server.com/show_bug.cgi?id=99
           *
           * Here is the crash log on WebAssembly (2019-08-05):
           * 
           * Uncaught abort(alignment fault) at Error
           * at jsStackTrace
           * at stackTrace
           * at abort
           * at alignfault
           * at SAFE_HEAP_LOAD_i32_2_2 (wasm-function[251132]:39)
           * at __ZN7Orthanc9PamReader12ParseContentEv (wasm-function[11457]:8088)
           **/
          uint8_t* srcdst = reinterpret_cast<uint8_t*>(pixel);
          uint8_t tmp = srcdst[0];
          srcdst[0] = srcdst[1];
          srcdst[1] = tmp;
        }
      }
    }
  }

  PamReader::PamReader(bool enforceAligned) :
    enforceAligned_(enforceAligned),
    alignedImageBuffer_(NULL)
  {
  }

  
#if ORTHANC_SANDBOXED == 0
  void PamReader::ReadFromFile(const std::string& filename)
  {
    SystemToolbox::ReadFile(content_, filename);
    ParseContent();
  }
#endif
  

  void PamReader::ReadFromMemory(const std::string& buffer)
  {
    content_ = buffer;
    ParseContent();
  }

  void PamReader::ReadFromMemory(const void* buffer,
                                 size_t size)
  {
    content_.assign(reinterpret_cast<const char*>(buffer), size);
    ParseContent();
  }

  PamReader::~PamReader()
  {
    if (alignedImageBuffer_ != NULL)
    {
      free(alignedImageBuffer_);
    }
  }
}
