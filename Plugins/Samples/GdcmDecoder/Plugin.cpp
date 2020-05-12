/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../../../Core/Compatibility.h"
#include "../../../Core/DicomFormat/DicomMap.h"
#include "../../../Core/Toolbox.h"
#include "GdcmDecoderCache.h"

static OrthancPlugins::GdcmDecoderCache  cache_;
static bool restrictTransferSyntaxes_ = false;
static std::set<std::string> enabledTransferSyntaxes_;


static bool ExtractTransferSyntax(std::string& transferSyntax,
                                  const void* dicom,
                                  const uint32_t size)
{
  Orthanc::DicomMap header;
  if (!Orthanc::DicomMap::ParseDicomMetaInformation(header, reinterpret_cast<const char*>(dicom), size))
  {
    return false;
  }

  const Orthanc::DicomValue* tag = header.TestAndGetValue(0x0002, 0x0010);
  if (tag == NULL ||
      tag->IsNull() ||
      tag->IsBinary())
  {
    return false;
  }
  else
  {
    // Stripping spaces should not be required, as this is a UI value
    // representation whose stripping is supported by the Orthanc
    // core, but let's be careful...
    transferSyntax = Orthanc::Toolbox::StripSpaces(tag->GetContent());
    return true;
  }
}


static bool IsTransferSyntaxEnabled(const void* dicom,
                                    const uint32_t size)
{
  std::string formattedSize;

  {
    char tmp[16];
    sprintf(tmp, "%0.1fMB", static_cast<float>(size) / (1024.0f * 1024.0f));
    formattedSize.assign(tmp);
  }

  if (!restrictTransferSyntaxes_)
  {
    LOG(INFO) << "Decoding one DICOM instance of " << formattedSize << " using GDCM";
    return true;
  }

  std::string transferSyntax;
  if (!ExtractTransferSyntax(transferSyntax, dicom, size))
  {
    LOG(INFO) << "Cannot extract the transfer syntax of this instance of "
              << formattedSize << ", will use GDCM to decode it";
    return true;
  }
  else if (enabledTransferSyntaxes_.find(transferSyntax) != enabledTransferSyntaxes_.end())
  {
    // Decoding for this transfer syntax is enabled
    LOG(INFO) << "Using GDCM to decode this instance of " << formattedSize
              << " with transfer syntax " << transferSyntax;
    return true;
  }
  else
  {
    LOG(INFO) << "Won't use GDCM to decode this instance of " << formattedSize
              << ", as its transfer syntax " << transferSyntax << " is disabled";
    return false;
  }
}


static OrthancPluginErrorCode DecodeImageCallback(OrthancPluginImage** target,
                                                  const void* dicom,
                                                  const uint32_t size,
                                                  uint32_t frameIndex)
{
  try
  {
    if (!IsTransferSyntaxEnabled(dicom, size))
    {
      *target = NULL;
      return OrthancPluginErrorCode_Success;
    }

    std::unique_ptr<OrthancPlugins::OrthancImage> image;

#if 0
    // Do not use the cache
    OrthancPlugins::GdcmImageDecoder decoder(dicom, size);
    image.reset(new OrthancPlugins::OrthancImage(decoder.Decode(frameIndex)));
#else
    image.reset(cache_.Decode(dicom, size, frameIndex));
#endif

    *target = image->Release();

    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    *target = NULL;

    LOG(WARNING) << "Cannot decode image using GDCM: " << e.What();
    return OrthancPluginErrorCode_Plugin;
  }
  catch (std::runtime_error& e)
  {
    *target = NULL;

    LOG(WARNING) << "Cannot decode image using GDCM: " << e.what();
    return OrthancPluginErrorCode_Plugin;
  }
  catch (...)
  {
    *target = NULL;

    LOG(WARNING) << "Native exception while decoding image using GDCM";
    return OrthancPluginErrorCode_Plugin;
  }
}



/**
 * We force the redefinition of the "ORTHANC_PLUGINS_API" macro, that
 * was left empty with gcc until Orthanc SDK 1.5.7 (no "default"
 * visibility). This causes the version script, if run from "Holy
 * Build Box", to make private the 4 global functions of the plugin.
 **/

#undef ORTHANC_PLUGINS_API

#ifdef WIN32
#  define ORTHANC_PLUGINS_API __declspec(dllexport)
#elif __GNUC__ >= 4
#  define ORTHANC_PLUGINS_API __attribute__ ((visibility ("default")))
#else
#  define ORTHANC_PLUGINS_API
#endif


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    static const char* const KEY_GDCM = "Gdcm";
    static const char* const KEY_ENABLE_GDCM = "EnableGdcm";
    static const char* const KEY_RESTRICT_TRANSFER_SYNTAXES = "RestrictTransferSyntaxes";

    OrthancPlugins::SetGlobalContext(context);
    LOG(INFO) << "Initializing the advanced decoder of medical images using GDCM";


    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    OrthancPluginSetDescription(context, "Advanced decoder of medical images using GDCM.");

    OrthancPlugins::OrthancConfiguration global;

    bool enabled = true;
    
    if (global.IsSection(KEY_GDCM))
    {
      OrthancPlugins::OrthancConfiguration config;
      global.GetSection(config, KEY_GDCM);

      enabled = config.GetBooleanValue(KEY_ENABLE_GDCM, true);

      if (config.LookupSetOfStrings(enabledTransferSyntaxes_, KEY_RESTRICT_TRANSFER_SYNTAXES, false))
      {
        restrictTransferSyntaxes_ = true;
        
        for (std::set<std::string>::const_iterator it = enabledTransferSyntaxes_.begin();
             it != enabledTransferSyntaxes_.end(); ++it)
        {
          LOG(WARNING) << "Orthanc will use GDCM to decode transfer syntax: " << *it;
        }
      }
    }

    if (enabled)
    {
      OrthancPluginRegisterDecodeImageCallback(context, DecodeImageCallback);
    }
    else
    {
      LOG(WARNING) << "The advanced decoder of medical images using GDCM is disabled";
    }
    
    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    LOG(INFO) << "Finalizing the advanced decoder of medical images using GDCM";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "gdcm-decoder";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return PLUGIN_VERSION;
  }
}
