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


#include "OrthancContext.h"

#include <stdexcept>


void OrthancContext::Check()
{
  if (context_ == NULL)
  {
    throw std::runtime_error("The Orthanc plugin context is not initialized");
  }
}


OrthancContext& OrthancContext::GetInstance()
{
  static OrthancContext instance;
  return instance;
}


OrthancContext::~OrthancContext()
{
  if (context_ != NULL)
  {
    throw std::runtime_error("The Orthanc plugin was not properly finalized");
  }
}


void OrthancContext::ExtractGetArguments(Arguments& arguments,
                                         const OrthancPluginHttpRequest& request)
{
  Check();
  arguments.clear();

  for (uint32_t i = 0; i < request.getCount; i++)
  {
    arguments[request.getKeys[i]] = request.getValues[i];
  }
}


void OrthancContext::LogError(const std::string& s)
{
  Check();
  OrthancPluginLogError(context_, s.c_str());
}


void OrthancContext::LogWarning(const std::string& s)
{
  Check();
  OrthancPluginLogWarning(context_, s.c_str());
}


void OrthancContext::LogInfo(const std::string& s)
{
  Check();
  OrthancPluginLogInfo(context_, s.c_str());
}
  

void OrthancContext::Register(const std::string& uri,
                              OrthancPluginRestCallback callback)
{
  Check();
  OrthancPluginRegisterRestCallback(context_, uri.c_str(), callback);
}


void OrthancContext::GetDicomForInstance(std::string& result,
                                         const std::string& instanceId)
{
  Check();
  OrthancPluginMemoryBuffer buffer;
    
  if (OrthancPluginGetDicomForInstance(context_, &buffer, instanceId.c_str()))
  {
    throw std::runtime_error("No DICOM instance with Orthanc ID: " + instanceId);
  }

  if (buffer.size == 0)
  {
    result.clear();
  }
  else
  {
    result.assign(reinterpret_cast<char*>(buffer.data), buffer.size);
  }

  OrthancPluginFreeMemoryBuffer(context_, &buffer);
}


void OrthancContext::CompressAndAnswerPngImage(OrthancPluginRestOutput* output,
                                               const Orthanc::ImageAccessor& accessor)
{
  Check();

  OrthancPluginPixelFormat format;
  switch (accessor.GetFormat())
  {
    case Orthanc::PixelFormat_Grayscale8:
      format = OrthancPluginPixelFormat_Grayscale8;
      break;

    case Orthanc::PixelFormat_Grayscale16:
      format = OrthancPluginPixelFormat_Grayscale16;
      break;

    case Orthanc::PixelFormat_SignedGrayscale16:
      format = OrthancPluginPixelFormat_SignedGrayscale16;
      break;

    case Orthanc::PixelFormat_RGB24:
      format = OrthancPluginPixelFormat_RGB24;
      break;

    case Orthanc::PixelFormat_RGBA32:
      format = OrthancPluginPixelFormat_RGBA32;
      break;

    default:
      throw std::runtime_error("Unsupported pixel format");
  }

  OrthancPluginCompressAndAnswerPngImage(context_, output, format, accessor.GetWidth(),
                                         accessor.GetHeight(), accessor.GetPitch(), accessor.GetConstBuffer());
}
