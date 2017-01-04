/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017 Osimis, Belgium
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


#include "GdcmImageDecoder.h"

#include "OrthancImageWrapper.h"

#include <gdcmImageReader.h>
#include <gdcmImageApplyLookupTable.h>
#include <gdcmImageChangePlanarConfiguration.h>
#include <gdcmImageChangePhotometricInterpretation.h>
#include <stdexcept>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>


namespace OrthancPlugins
{
  struct GdcmImageDecoder::PImpl
  {
    const void*           dicom_;
    size_t                size_;

    gdcm::ImageReader reader_;
    std::auto_ptr<gdcm::ImageApplyLookupTable> lut_;
    std::auto_ptr<gdcm::ImageChangePhotometricInterpretation> photometric_;
    std::auto_ptr<gdcm::ImageChangePlanarConfiguration> interleaved_;
    std::string decoded_;

    PImpl(const void* dicom,
          size_t size) :
      dicom_(dicom),
      size_(size)
    {
    }


    const gdcm::DataSet& GetDataSet() const
    {
      return reader_.GetFile().GetDataSet();
    }


    const gdcm::Image& GetImage() const
    {
      if (interleaved_.get() != NULL)
      {
        return interleaved_->GetOutput();
      }

      if (lut_.get() != NULL)
      {
        return lut_->GetOutput();
      }

      if (photometric_.get() != NULL)
      {
        return photometric_->GetOutput();
      }

      return reader_.GetImage();
    }


    void Decode()
    {
      // Change photometric interpretation or apply LUT, if required
      {
        const gdcm::Image& image = GetImage();
        if (image.GetPixelFormat().GetSamplesPerPixel() == 1 &&
            image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::PALETTE_COLOR)
        {
          lut_.reset(new gdcm::ImageApplyLookupTable());
          lut_->SetInput(image);
          if (!lut_->Apply())
          {
            throw std::runtime_error( "GDCM cannot apply the lookup table");
          }
        }
        else if (image.GetPixelFormat().GetSamplesPerPixel() == 1)
        {
          if (image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME1 &&
              image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME2)
          {
            photometric_.reset(new gdcm::ImageChangePhotometricInterpretation());
            photometric_->SetInput(image);
            photometric_->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);
            if (!photometric_->Change() ||
                GetImage().GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME2)
            {
              throw std::runtime_error("GDCM cannot change the photometric interpretation");
            }
          }      
        }
        else 
        {
          if (image.GetPixelFormat().GetSamplesPerPixel() == 3 &&
              image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::RGB &&
              (image.GetTransferSyntax() != gdcm::TransferSyntax::JPEG2000Lossless ||
               image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::YBR_RCT))
          {
            photometric_.reset(new gdcm::ImageChangePhotometricInterpretation());
            photometric_->SetInput(image);
            photometric_->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::RGB);
            if (!photometric_->Change() ||
                GetImage().GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::RGB)
            {
              throw std::runtime_error("GDCM cannot change the photometric interpretation");
            }
          }
        }
      }

      // Possibly convert planar configuration to interleaved
      {
        const gdcm::Image& image = GetImage();
        if (image.GetPlanarConfiguration() != 0 && 
            image.GetPixelFormat().GetSamplesPerPixel() != 1)
        {
          interleaved_.reset(new gdcm::ImageChangePlanarConfiguration());
          interleaved_->SetInput(image);
          if (!interleaved_->Change() ||
              GetImage().GetPlanarConfiguration() != 0)
          {
            throw std::runtime_error("GDCM cannot change the planar configuration to interleaved");
          }
        }
      }
    }
  };

  GdcmImageDecoder::GdcmImageDecoder(const void* dicom,
                                     size_t size) :
    pimpl_(new PImpl(dicom, size))
  {
    // Setup a stream to the memory buffer
    using namespace boost::iostreams;
    basic_array_source<char> source(reinterpret_cast<const char*>(dicom), size);
    stream<basic_array_source<char> > stream(source);

    // Parse the DICOM instance using GDCM
    pimpl_->reader_.SetStream(stream);
    if (!pimpl_->reader_.Read())
    {
      throw std::runtime_error("Bad file format");
    }

    pimpl_->Decode();
  }


  OrthancPluginPixelFormat GdcmImageDecoder::GetFormat() const
  {
    const gdcm::Image& image = pimpl_->GetImage();

    if (image.GetPixelFormat().GetSamplesPerPixel() == 1 &&
        (image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::MONOCHROME1 ||
         image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::MONOCHROME2))
    {
      switch (image.GetPixelFormat())
      {
        case gdcm::PixelFormat::UINT16:
          return OrthancPluginPixelFormat_Grayscale16;

        case gdcm::PixelFormat::INT16:
          return OrthancPluginPixelFormat_SignedGrayscale16;

        case gdcm::PixelFormat::UINT8:
          return OrthancPluginPixelFormat_Grayscale8;

        default:
          throw std::runtime_error("Unsupported pixel format");
      }
    }
    else if (image.GetPixelFormat().GetSamplesPerPixel() == 3 &&
             (image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::RGB ||
              image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::YBR_RCT))
    {
      switch (image.GetPixelFormat())
      {
        case gdcm::PixelFormat::UINT8:
          return OrthancPluginPixelFormat_RGB24;

        default:
          break;
      }      
    }

    throw std::runtime_error("Unsupported pixel format");
  }


  unsigned int GdcmImageDecoder::GetWidth() const
  {
    return pimpl_->GetImage().GetColumns();
  }


  unsigned int GdcmImageDecoder::GetHeight() const
  {
    return pimpl_->GetImage().GetRows();
  }

  
  unsigned int GdcmImageDecoder::GetFramesCount() const
  {
    return pimpl_->GetImage().GetDimension(2);
  }


  size_t GdcmImageDecoder::GetBytesPerPixel(OrthancPluginPixelFormat format)
  {
    switch (format)
    {
      case OrthancPluginPixelFormat_Grayscale8:
        return 1;

      case OrthancPluginPixelFormat_Grayscale16:
      case OrthancPluginPixelFormat_SignedGrayscale16:
        return 2;

      case OrthancPluginPixelFormat_RGB24:
        return 3;

      default:
        throw std::runtime_error("Unsupport pixel format");
    }
  }


  OrthancPluginImage* GdcmImageDecoder::Decode(OrthancPluginContext* context,
                                               unsigned int frameIndex) const
  {
    unsigned int frames = GetFramesCount();
    unsigned int width = GetWidth();
    unsigned int height = GetHeight();
    OrthancPluginPixelFormat format = GetFormat();
    size_t bpp = GetBytesPerPixel(format);

    if (frameIndex >= frames)
    {
      throw std::runtime_error("Inexistent frame index");
    }

    std::string& decoded = pimpl_->decoded_;
    OrthancImageWrapper target(context, format, width, height);

    if (width == 0 ||
        height == 0)
    {
      return target.Release();
    }

    if (decoded.empty())
    {
      decoded.resize(pimpl_->GetImage().GetBufferLength());
      pimpl_->GetImage().GetBuffer(&decoded[0]);
    }

    const void* sourceBuffer = &decoded[0];

    if (target.GetPitch() == bpp * width &&
        frames == 1)
    {
      assert(decoded.size() == target.GetPitch() * target.GetHeight());      
      memcpy(target.GetBuffer(), sourceBuffer, decoded.size());
    }
    else 
    {
      size_t targetPitch = target.GetPitch();
      size_t sourcePitch = width * bpp;

      const char* a = &decoded[sourcePitch * height * frameIndex];
      char* b = target.GetBuffer();

      for (uint32_t y = 0; y < height; y++)
      {
        memcpy(b, a, sourcePitch);
        a += sourcePitch;
        b += targetPitch;
      }
    }

    return target.Release();
  }
}
