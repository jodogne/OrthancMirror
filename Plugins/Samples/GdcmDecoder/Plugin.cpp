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


#include "GdcmDecoderCache.h"
#include "OrthancImageWrapper.h"

#include <orthanc/OrthancCPlugin.h>

static OrthancPluginContext* context_ = NULL;
static OrthancPlugins::GdcmDecoderCache  cache_;


static OrthancPluginErrorCode DecodeImageCallback(OrthancPluginImage** target,
                                                  const void* dicom,
                                                  const uint32_t size,
                                                  uint32_t frameIndex)
{
  try
  {
    std::auto_ptr<OrthancPlugins::OrthancImageWrapper> image;

#if 0
    // Do not use the cache
    OrthancPlugins::GdcmImageDecoder decoder(dicom, size);
    image.reset(new OrthancPlugins::OrthancImageWrapper(context_, decoder.Decode(context_, frameIndex)));
#else
    image.reset(cache_.Decode(context_, dicom, size, frameIndex));
#endif

    *target = image->Release();

    return OrthancPluginErrorCode_Success;
  }
  catch (std::runtime_error& e)
  {
    *target = NULL;

    std::string s = "Cannot decode image using GDCM: " + std::string(e.what());
    OrthancPluginLogError(context_, s.c_str());
    return OrthancPluginErrorCode_Plugin;
  }
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;
    OrthancPluginLogWarning(context_, "Initializing the advanced decoder of medical images using GDCM");


    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context_) == 0)
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

    OrthancPluginSetDescription(context_, "Advanced decoder of medical images using GDCM.");
    OrthancPluginRegisterDecodeImageCallback(context_, DecodeImageCallback);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "gdcm-decoder";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return GDCM_DECODER_VERSION;
  }
}
