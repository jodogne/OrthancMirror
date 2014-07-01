/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


#include <map>
#include <string>
#include <stdexcept>
#include <sstream>

#include "OrthancContext.h"
#include "../../../Core/ImageFormats/ImageProcessing.h"

#include <gdcmReader.h>
#include <gdcmImageReader.h>


static bool GetOrthancPixelFormat(Orthanc::PixelFormat& format,
                                  const gdcm::Image& image)
{
  if (image.GetPlanarConfiguration() != 0 && 
      image.GetPixelFormat().GetSamplesPerPixel() != 1)
  {
    OrthancContext::GetInstance().LogError("Planar configurations are not supported");
    return false;
  }

  if (image.GetPixelFormat().GetSamplesPerPixel() == 1)
  {
    switch (image.GetPixelFormat().GetScalarType())
    {
      case gdcm::PixelFormat::UINT8:
        format = Orthanc::PixelFormat_Grayscale8;
        return true;

      case gdcm::PixelFormat::UINT16:
        format = Orthanc::PixelFormat_Grayscale16;
        return true;

      case gdcm::PixelFormat::INT16:
        format = Orthanc::PixelFormat_SignedGrayscale16;
        return true;

      default:
        return false;
    }
  }
  else if (image.GetPixelFormat().GetSamplesPerPixel() == 3 &&
           image.GetPixelFormat().GetScalarType() == gdcm::PixelFormat::UINT8)
  {
    format = Orthanc::PixelFormat_RGB24;
  }
  else if (image.GetPixelFormat().GetSamplesPerPixel() == 4 &&
           image.GetPixelFormat().GetScalarType() == gdcm::PixelFormat::UINT8)
  {
    format = Orthanc::PixelFormat_RGBA32;
  }

  return false;
}


ORTHANC_PLUGINS_API int32_t DecodeImage(OrthancPluginRestOutput* output,
                                        const char* url,
                                        const OrthancPluginHttpRequest* request)
{
  std::string instance(request->groups[0]);
  std::string outputFormat(request->groups[1]);
  OrthancContext::GetInstance().LogWarning("Using GDCM to decode instance " + instance);

  // Download the request DICOM instance from Orthanc into a memory buffer
  std::string dicom;
  OrthancContext::GetInstance().GetDicomForInstance(dicom, instance);

  // Prepare a memory stream over the DICOM instance
  std::stringstream stream(dicom);

  // Parse the DICOM instance using GDCM
  gdcm::ImageReader imageReader;
  imageReader.SetStream(stream);
  if (!imageReader.Read())
  {
    OrthancContext::GetInstance().LogError("GDCM cannot extract an image from this DICOM instance");
    return -1;  // Error
  }

  gdcm::Image& image = imageReader.GetImage();

  // Log information about the decoded image
  char tmp[1024];
  sprintf(tmp, "Image format: %dx%d %s with %d color channel(s)", image.GetRows(), image.GetColumns(), 
          image.GetPixelFormat().GetScalarTypeAsString(), image.GetPixelFormat().GetSamplesPerPixel());
  OrthancContext::GetInstance().LogWarning(tmp);

  // Create a read-only accessor to the bitmap decoded by GDCM
  Orthanc::PixelFormat format;
  if (!GetOrthancPixelFormat(format, image))
  {
    OrthancContext::GetInstance().LogError("This sample plugin does not support this image format");
    return -1;  // Error
  }

  Orthanc::ImageAccessor decodedImage;
  std::vector<char> decodedBuffer(image.GetBufferLength());

  if (decodedBuffer.size())
  {
    image.GetBuffer(&decodedBuffer[0]);
    unsigned int pitch = image.GetColumns() * ::Orthanc::GetBytesPerPixel(format);
    decodedImage.AssignWritable(format, image.GetColumns(), image.GetRows(), pitch, &decodedBuffer[0]);
  }
  else
  {
    // Empty image
    decodedImage.AssignWritable(format, 0, 0, 0, NULL);
  }


  // Convert the pixel format from GDCM to the format requested by the REST query
  Orthanc::ImageBuffer converted;
  converted.SetWidth(decodedImage.GetWidth());
  converted.SetHeight(decodedImage.GetHeight());

  if (outputFormat == "preview")
  {
    if (format == Orthanc::PixelFormat_RGB24 ||
        format == Orthanc::PixelFormat_RGBA32)
    {
      // Do not rescale color image
      converted.SetFormat(Orthanc::PixelFormat_RGB24);
    }
    else
    {
      converted.SetFormat(Orthanc::PixelFormat_Grayscale8);

      // Rescale the image to the [0,255] range
      int64_t a, b;
      Orthanc::ImageProcessing::GetMinMaxValue(a, b, decodedImage);

      float offset = -a;
      float scaling = 255.0f / static_cast<float>(b - a);
      Orthanc::ImageProcessing::ShiftScale(decodedImage, offset, scaling);
    }
  }
  else if (outputFormat == "image-uint8")
  {
    converted.SetFormat(Orthanc::PixelFormat_Grayscale8);
  }
  else if (outputFormat == "image-uint16")
  {
    converted.SetFormat(Orthanc::PixelFormat_Grayscale16);
  }
  else if (outputFormat == "image-int16")
  {
    converted.SetFormat(Orthanc::PixelFormat_SignedGrayscale16);
  }
  else
  {
    OrthancContext::GetInstance().LogError("Unknown output format: " + outputFormat);
    return -1;
  }

  Orthanc::ImageAccessor convertedAccessor(converted.GetAccessor());
  Orthanc::ImageProcessing::Convert(convertedAccessor, decodedImage);


  // Compress the converted image as a PNG file
  OrthancContext::GetInstance().CompressAndAnswerPngImage(output, convertedAccessor);

  return 0;  // Success
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancContext::GetInstance().Initialize(context);
    OrthancContext::GetInstance().LogWarning("Initializing GDCM decoding");
    OrthancContext::GetInstance().Register("/instances/([^/]+)/(preview|image-uint8|image-uint16|image-int16)", DecodeImage);
    return 0;
  }

  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancContext::GetInstance().LogWarning("Finalizing GDCM decoding");
    OrthancContext::GetInstance().Finalize();
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "gdcm-decoding";
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "1.0";
  }
}
