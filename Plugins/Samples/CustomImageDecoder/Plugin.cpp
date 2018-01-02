/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include <orthanc/OrthancCPlugin.h>

static OrthancPluginContext* context_ = NULL;


static OrthancPluginErrorCode DecodeImageCallback(OrthancPluginImage** target,
                                                  const void* dicom,
                                                  const uint32_t size,
                                                  uint32_t frameIndex)
{
  *target = OrthancPluginCreateImage(context_, OrthancPluginPixelFormat_RGB24, 512, 512);

  memset(OrthancPluginGetImageBuffer(context_, *target), 128,
         OrthancPluginGetImageHeight(context_, *target) * OrthancPluginGetImagePitch(context_, *target));

  return OrthancPluginDrawText(context_, *target, 0, "Hello world", 100, 50, 255, 0, 0);
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context_ = c;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    OrthancPluginRegisterDecodeImageCallback(context_, DecodeImageCallback);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "custom-image-decoder";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.0";
  }
}
