/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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



void OrthancContext::Redirect(OrthancPluginRestOutput* output,
                              const std::string& s)
{
  Check();
  OrthancPluginRedirect(context_, output, s.c_str());
}
