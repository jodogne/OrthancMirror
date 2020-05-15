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
#include "../../../Core/MultiThreading/Semaphore.h"
#include "../../../Core/Toolbox.h"
#include "GdcmDecoderCache.h"

#include <gdcmImageChangeTransferSyntax.h>
#include <gdcmImageReader.h>
#include <gdcmImageWriter.h>
#include <gdcmUIDGenerator.h>
#include <gdcmAttribute.h>


static OrthancPlugins::GdcmDecoderCache  cache_;
static bool restrictTransferSyntaxes_ = false;
static std::set<std::string> enabledTransferSyntaxes_;
static bool hasThrottling_ = false;
static std::unique_ptr<Orthanc::Semaphore> throttlingSemaphore_;

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
    std::unique_ptr<Orthanc::Semaphore::Locker> locker;
    
    if (hasThrottling_)
    {
      if (throttlingSemaphore_.get() == NULL)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
      else
      {
        locker.reset(new Orthanc::Semaphore::Locker(*throttlingSemaphore_));
      }
    }

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


OrthancPluginErrorCode TranscoderCallback(
  OrthancPluginMemoryBuffer* transcoded /* out */,
  uint8_t*                   hasSopInstanceUidChanged /* out */,
  const void*                buffer,
  uint64_t                   size,
  const char* const*         allowedSyntaxes,
  uint32_t                   countSyntaxes,
  uint8_t                    allowNewSopInstanceUid)
{
  try
  {
    std::unique_ptr<Orthanc::Semaphore::Locker> locker;
    
    if (hasThrottling_)
    {
      if (throttlingSemaphore_.get() == NULL)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
      else
      {
        locker.reset(new Orthanc::Semaphore::Locker(*throttlingSemaphore_));
      }
    }

    std::string dicom(reinterpret_cast<const char*>(buffer), size);
    std::stringstream stream(dicom);

    gdcm::ImageReader reader;
    reader.SetStream(stream);
    if (!reader.Read())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "GDCM cannot decode the image");
    }

    // First check that transcoding is mandatory
    for (uint32_t i = 0; i < countSyntaxes; i++)
    {
      gdcm::TransferSyntax syntax(gdcm::TransferSyntax::GetTSType(allowedSyntaxes[i]));
      if (syntax.IsValid() &&
          reader.GetImage().GetTransferSyntax() == syntax)
      {
        // Same transfer syntax as in the source, return a copy of the
        // source buffer
        OrthancPlugins::MemoryBuffer orthancBuffer(buffer, size);
        *transcoded = orthancBuffer.Release();
        *hasSopInstanceUidChanged = false;
        return OrthancPluginErrorCode_Success;
      }
    }

    for (uint32_t i = 0; i < countSyntaxes; i++)
    {
      gdcm::TransferSyntax syntax(gdcm::TransferSyntax::GetTSType(allowedSyntaxes[i]));
      if (syntax.IsValid())
      {
        gdcm::ImageChangeTransferSyntax change;
        change.SetTransferSyntax(syntax);
        change.SetInput(reader.GetImage());

        if (change.Change())
        {
          if (syntax == gdcm::TransferSyntax::JPEGBaselineProcess1 ||
              syntax == gdcm::TransferSyntax::JPEGExtendedProcess2_4 ||
              syntax == gdcm::TransferSyntax::JPEGLSNearLossless ||
              syntax == gdcm::TransferSyntax::JPEG2000 ||
              syntax == gdcm::TransferSyntax::JPEG2000Part2)
          {
            // In the case of a lossy compression, generate new SOP instance UID
            gdcm::UIDGenerator generator;
            std::string uid = generator.Generate();
            if (uid.size() == 0)
            {
              throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError,
                                              "GDCM cannot generate a UID");
            }

            gdcm::Attribute<0x0008,0x0018> sopInstanceUid;
            sopInstanceUid.SetValue(uid);
            reader.GetFile().GetDataSet().Replace(sopInstanceUid.GetAsDataElement());
            *hasSopInstanceUidChanged = 1;
          }
          else
          {
            *hasSopInstanceUidChanged = 0;
          }
      
          // GDCM was able to change the transfer syntax, serialize it
          // to the output buffer
          gdcm::ImageWriter writer;
          writer.SetImage(change.GetOutput());
          writer.SetFile(reader.GetFile());

          std::stringstream ss;
          writer.SetStream(ss);
          if (writer.Write())
          {
            std::string s = ss.str();
            OrthancPlugins::MemoryBuffer orthancBuffer(s.empty() ? NULL : s.c_str(), s.size());
            *transcoded = orthancBuffer.Release();

            return OrthancPluginErrorCode_Success;
          }
          else
          {
            throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError,
                                            "GDCM cannot serialize the image");
          }
        }
      }
    }
    
    throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
  }
  catch (Orthanc::OrthancException& e)
  {
    LOG(INFO) << "Cannot transcode image using GDCM: " << e.What();
    return OrthancPluginErrorCode_Plugin;
  }
  catch (std::runtime_error& e)
  {
    LOG(INFO) << "Cannot transcode image using GDCM: " << e.what();
    return OrthancPluginErrorCode_Plugin;
  }
  catch (...)
  {
    LOG(INFO) << "Native exception while decoding image using GDCM";
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
    static const char* const KEY_ENABLE_GDCM = "Enable";
    static const char* const KEY_THROTTLING = "Throttling";
    static const char* const KEY_RESTRICT_TRANSFER_SYNTAXES = "RestrictTransferSyntaxes";

    try
    {
      OrthancPlugins::SetGlobalContext(context);
      Orthanc::Logging::Initialize(context);
      LOG(INFO) << "Initializing the decoder/transcoder of medical images using GDCM";

      /* Check the version of the Orthanc core */
      if (!OrthancPlugins::CheckMinimalOrthancVersion(0, 9, 5))
      {
        LOG(ERROR) << "Your version of Orthanc (" << std::string(context->orthancVersion)
                   << ") must be above 0.9.5 to run this plugin";
        return -1;
      }

      OrthancPluginSetDescription(context, "Decoder/transcoder of medical images using GDCM.");

      OrthancPlugins::OrthancConfiguration global;

      bool enabled = true;
      hasThrottling_ = false;
    
      if (global.IsSection(KEY_GDCM))
      {
        OrthancPlugins::OrthancConfiguration config;
        global.GetSection(config, KEY_GDCM);

        enabled = config.GetBooleanValue(KEY_ENABLE_GDCM, true);

        if (enabled &&
            config.LookupSetOfStrings(enabledTransferSyntaxes_, KEY_RESTRICT_TRANSFER_SYNTAXES, false))
        {
          restrictTransferSyntaxes_ = true;
        
          for (std::set<std::string>::const_iterator it = enabledTransferSyntaxes_.begin();
               it != enabledTransferSyntaxes_.end(); ++it)
          {
            LOG(WARNING) << "Orthanc will use GDCM to decode transfer syntax: " << *it;
          }
        }

        unsigned int throttling;
        if (enabled &&
            config.LookupUnsignedIntegerValue(throttling, KEY_THROTTLING))
        {
          if (throttling == 0)
          {
            LOG(ERROR) << "Bad value for option \"" << KEY_THROTTLING
                       << "\": Must be a strictly positive integer";
            return -1;
          }
          else
          {
            LOG(WARNING) << "Throttling GDCM to " << throttling << " concurrent thread(s)";
            hasThrottling_ = true;
            throttlingSemaphore_.reset(new Orthanc::Semaphore(throttling));
          }
        }
      }

      if (enabled)
      {
        if (!hasThrottling_)
        {
          LOG(WARNING) << "GDCM throttling is disabled";
        }

        OrthancPluginRegisterDecodeImageCallback(context, DecodeImageCallback);

        if (OrthancPlugins::CheckMinimalOrthancVersion(1, 7, 0))
        {
          OrthancPluginRegisterTranscoderCallback(context, TranscoderCallback);
        }
        else
        {
          LOG(WARNING) << "Your version of Orthanc (" << std::string(context->orthancVersion)
                       << ") must be above 1.7.0 to benefit from transcoding";
        }
      }
      else
      {
        LOG(WARNING) << "The decoder/transcoder of medical images using GDCM is disabled";
      }
    
      return 0;
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "Exception while initializing the GDCM plugin: " << e.What();
      return -1;
    }
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    LOG(INFO) << "Finalizing the decoder/transcoder of medical images using GDCM";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "gdcm";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return PLUGIN_VERSION;
  }
}
