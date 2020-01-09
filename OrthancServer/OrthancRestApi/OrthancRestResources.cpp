/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2019 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../Core/Compression/GzipCompressor.h"
#include "../../Core/DicomParsing/DicomWebJsonVisitor.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../Core/HttpServer/HttpContentNegociation.h"
#include "../../Core/Images/ImageProcessing.h"
#include "../../Core/Logging.h"
#include "../DefaultDicomImageDecoder.h"
#include "../OrthancConfiguration.h"
#include "../Search/DatabaseLookup.h"
#include "../ServerContext.h"
#include "../ServerToolbox.h"
#include "../SliceOrdering.h"

#include "../../Plugins/Engine/OrthancPlugins.h"


namespace Orthanc
{
  static void AnswerDicomAsJson(RestApiCall& call,
                                const Json::Value& dicom,
                                DicomToJsonFormat mode)
  {
    if (mode != DicomToJsonFormat_Full)
    {
      Json::Value simplified;
      ServerToolbox::SimplifyTags(simplified, dicom, mode);
      call.GetOutput().AnswerJson(simplified);
    }
    else
    {
      call.GetOutput().AnswerJson(dicom);
    }
  }


  static DicomToJsonFormat GetDicomFormat(const RestApiGetCall& call)
  {
    if (call.HasArgument("simplify"))
    {
      return DicomToJsonFormat_Human;
    }
    else if (call.HasArgument("short"))
    {
      return DicomToJsonFormat_Short;
    }
    else
    {
      return DicomToJsonFormat_Full;
    }
  }


  static void AnswerDicomAsJson(RestApiGetCall& call,
                                const Json::Value& dicom)
  {
    AnswerDicomAsJson(call, dicom, GetDicomFormat(call));
  }


  static void ParseSetOfTags(std::set<DicomTag>& target,
                             const RestApiGetCall& call,
                             const std::string& argument)
  {
    target.clear();

    if (call.HasArgument(argument))
    {
      std::vector<std::string> tags;
      Toolbox::TokenizeString(tags, call.GetArgument(argument, ""), ',');

      for (size_t i = 0; i < tags.size(); i++)
      {
        target.insert(FromDcmtkBridge::ParseTag(tags[i]));
      }
    }
  }


  // List all the patients, studies, series or instances ----------------------
 
  static void AnswerListOfResources(RestApiOutput& output,
                                    ServerIndex& index,
                                    const std::list<std::string>& resources,
                                    ResourceType level,
                                    bool expand)
  {
    Json::Value answer = Json::arrayValue;

    for (std::list<std::string>::const_iterator
           resource = resources.begin(); resource != resources.end(); ++resource)
    {
      if (expand)
      {
        Json::Value item;
        if (index.LookupResource(item, *resource, level))
        {
          answer.append(item);
        }
      }
      else
      {
        answer.append(*resource);
      }
    }

    output.AnswerJson(answer);
  }


  template <enum ResourceType resourceType>
  static void ListResources(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::list<std::string> result;

    if (call.HasArgument("limit") ||
        call.HasArgument("since"))
    {
      if (!call.HasArgument("limit"))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Missing \"limit\" argument for GET request against: " +
                               call.FlattenUri());
      }

      if (!call.HasArgument("since"))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Missing \"since\" argument for GET request against: " +
                               call.FlattenUri());
      }

      size_t since = boost::lexical_cast<size_t>(call.GetArgument("since", ""));
      size_t limit = boost::lexical_cast<size_t>(call.GetArgument("limit", ""));
      index.GetAllUuids(result, resourceType, since, limit);
    }
    else
    {
      index.GetAllUuids(result, resourceType);
    }


    AnswerListOfResources(call.GetOutput(), index, result, resourceType, call.HasArgument("expand"));
  }

  template <enum ResourceType resourceType>
  static void GetSingleResource(RestApiGetCall& call)
  {
    Json::Value result;
    if (OrthancRestApi::GetIndex(call).LookupResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }

  template <enum ResourceType resourceType>
  static void DeleteSingleResource(RestApiDeleteCall& call)
  {
    Json::Value result;
    if (OrthancRestApi::GetContext(call).DeleteResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  // Get information about a single patient -----------------------------------
 
  static void IsProtectedPatient(RestApiGetCall& call)
  {
    std::string publicId = call.GetUriComponent("id", "");
    bool isProtected = OrthancRestApi::GetIndex(call).IsProtectedPatient(publicId);
    call.GetOutput().AnswerBuffer(isProtected ? "1" : "0", MimeType_PlainText);
  }


  static void SetPatientProtection(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::string body;
    call.BodyToString(body);
    body = Toolbox::StripSpaces(body);

    if (body == "0")
    {
      context.GetIndex().SetProtectedPatient(publicId, false);
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
    else if (body == "1")
    {
      context.GetIndex().SetProtectedPatient(publicId, true);
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
    else
    {
      // Bad request
    }
  }


  // Get information about a single instance ----------------------------------
 
  static void GetInstanceFile(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    IHttpHandler::Arguments::const_iterator accept = call.GetHttpHeaders().find("accept");
    if (accept != call.GetHttpHeaders().end())
    {
      // New in Orthanc 1.5.4
      try
      {
        MimeType mime = StringToMimeType(accept->second.c_str());

        if (mime == MimeType_DicomWebJson ||
            mime == MimeType_DicomWebXml)
        {
          DicomWebJsonVisitor visitor;
          
          {
            ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
            locker.GetDicom().Apply(visitor);
          }

          if (mime == MimeType_DicomWebJson)
          {
            std::string s = visitor.GetResult().toStyledString();
            call.GetOutput().AnswerBuffer(s, MimeType_DicomWebJson);
          }
          else
          {
            std::string xml;
            visitor.FormatXml(xml);
            call.GetOutput().AnswerBuffer(xml, MimeType_DicomWebXml);
          }
          
          return;
        }
      }
      catch (OrthancException&)
      {
      }
    }

    context.AnswerAttachment(call.GetOutput(), publicId, FileContentType_Dicom);
  }


  static void ExportInstanceFile(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::string dicom;
    context.ReadDicom(dicom, publicId);

    std::string target;
    call.BodyToString(target);
    SystemToolbox::WriteFile(dicom, target);

    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  template <DicomToJsonFormat format>
  static void GetInstanceTags(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, "ignore-length");
    
    if (format != DicomToJsonFormat_Full ||
        !ignoreTagLength.empty())
    {
      Json::Value full;
      context.ReadDicomAsJson(full, publicId, ignoreTagLength);
      AnswerDicomAsJson(call, full, format);
    }
    else
    {
      // This path allows to avoid the JSON decoding if no
      // simplification is asked, and if no "ignore-length" argument
      // is present
      std::string full;
      context.ReadDicomAsJson(full, publicId);
      call.GetOutput().AnswerBuffer(full, MimeType_Json);
    }
  }


  static void GetInstanceTagsBis(RestApiGetCall& call)
  {
    switch (GetDicomFormat(call))
    {
      case DicomToJsonFormat_Human:
        GetInstanceTags<DicomToJsonFormat_Human>(call);
        break;

      case DicomToJsonFormat_Short:
        GetInstanceTags<DicomToJsonFormat_Short>(call);
        break;

      case DicomToJsonFormat_Full:
        GetInstanceTags<DicomToJsonFormat_Full>(call);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  
  static void ListFrames(RestApiGetCall& call)
  {
    std::string publicId = call.GetUriComponent("id", "");

    unsigned int numberOfFrames;
      
    {
      ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
      numberOfFrames = locker.GetDicom().GetFramesCount();
    }
    
    Json::Value result = Json::arrayValue;
    for (unsigned int i = 0; i < numberOfFrames; i++)
    {
      result.append(i);
    }
    
    call.GetOutput().AnswerJson(result);
  }


  namespace
  {
    class ImageToEncode
    {
    private:
      std::auto_ptr<ImageAccessor>&  image_;
      ImageExtractionMode            mode_;
      bool                           invert_;
      MimeType                       format_;
      std::string                    answer_;

    public:
      ImageToEncode(std::auto_ptr<ImageAccessor>& image,
                    ImageExtractionMode mode,
                    bool invert) :
        image_(image),
        mode_(mode),
        invert_(invert)
      {
      }

      void Answer(RestApiOutput& output)
      {
        output.AnswerBuffer(answer_, format_);
      }

      void EncodeUsingPng()
      {
        format_ = MimeType_Png;
        DicomImageDecoder::ExtractPngImage(answer_, image_, mode_, invert_);
      }

      void EncodeUsingPam()
      {
        format_ = MimeType_Pam;
        DicomImageDecoder::ExtractPamImage(answer_, image_, mode_, invert_);
      }

      void EncodeUsingJpeg(uint8_t quality)
      {
        format_ = MimeType_Jpeg;
        DicomImageDecoder::ExtractJpegImage(answer_, image_, mode_, invert_, quality);
      }
    };

    class EncodePng : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;

    public:
      EncodePng(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype)
      {
        assert(type == "image");
        assert(subtype == "png");
        image_.EncodeUsingPng();
      }
    };

    class EncodePam : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;

    public:
      EncodePam(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype)
      {
        assert(type == "image");
        assert(subtype == "x-portable-arbitrarymap");
        image_.EncodeUsingPam();
      }
    };

    class EncodeJpeg : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;
      unsigned int    quality_;

    public:
      EncodeJpeg(ImageToEncode& image,
                 const RestApiGetCall& call) :
        image_(image)
      {
        std::string v = call.GetArgument("quality", "90" /* default JPEG quality */);
        bool ok = false;

        try
        {
          quality_ = boost::lexical_cast<unsigned int>(v);
          ok = (quality_ >= 1 && quality_ <= 100);
        }
        catch (boost::bad_lexical_cast&)
        {
        }

        if (!ok)
        {
          throw OrthancException(
            ErrorCode_BadRequest,
            "Bad quality for a JPEG encoding (must be a number between 0 and 100): " + v);
        }
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype)
      {
        assert(type == "image");
        assert(subtype == "jpeg");
        image_.EncodeUsingJpeg(quality_);
      }
    };
  }


  void LookupWindowingTags(const ParsedDicomFile& dicom, float& windowCenter, float& windowWidth, float& rescaleSlope, float& rescaleIntercept, bool& invert)
  {
    DicomMap dicomTags;
    dicom.ExtractDicomSummary(dicomTags);


    unsigned int bitsStored = boost::lexical_cast<unsigned int>(dicomTags.GetStringValue(Orthanc::DICOM_TAG_BITS_STORED, "8", false));
    windowWidth = static_cast<float>(2 << (bitsStored - 1));
    windowCenter = windowWidth / 2;
    rescaleSlope = 1.0f;
    rescaleIntercept = 0.0f;
    invert = false;

    if (dicomTags.HasTag(Orthanc::DICOM_TAG_WINDOW_CENTER) && dicomTags.HasTag(Orthanc::DICOM_TAG_WINDOW_WIDTH))
    {
      windowCenter = boost::lexical_cast<float>(dicomTags.GetStringValue(Orthanc::DICOM_TAG_WINDOW_CENTER, "", false));
      windowWidth = boost::lexical_cast<float>(dicomTags.GetStringValue(Orthanc::DICOM_TAG_WINDOW_WIDTH, "", false));
    }

    if (dicomTags.HasTag(Orthanc::DICOM_TAG_RESCALE_SLOPE) && dicomTags.HasTag(Orthanc::DICOM_TAG_RESCALE_INTERCEPT))
    {
      rescaleSlope = boost::lexical_cast<float>(dicomTags.GetStringValue(Orthanc::DICOM_TAG_RESCALE_SLOPE, "", false));
      rescaleIntercept = boost::lexical_cast<float>(dicomTags.GetStringValue(Orthanc::DICOM_TAG_RESCALE_INTERCEPT, "", false));
    }

    PhotometricInterpretation photometric;
    if (dicom.LookupPhotometricInterpretation(photometric))
    {
      invert = (photometric == PhotometricInterpretation_Monochrome1);
    }
  }

  template <enum ImageExtractionMode mode>
  static void GetImage(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast&)
    {
      return;
    }

    bool invert = false;
    float windowCenter = 128.0f;
    float windowWidth = 256.0f;
    float rescaleSlope = 1.0f;
    float rescaleIntercept = 0.0f;

    std::auto_ptr<ImageAccessor> decoded;

    try
    {
      std::string publicId = call.GetUriComponent("id", "");

#if ORTHANC_ENABLE_PLUGINS == 1
      if (context.GetPlugins().HasCustomImageDecoder())
      {
        // TODO create a cache of file
        std::string dicomContent;
        context.ReadDicom(dicomContent, publicId);
        decoded.reset(context.GetPlugins().DecodeUnsafe(dicomContent.c_str(), dicomContent.size(), frame));

        /**
         * Note that we call "DecodeUnsafe()": We do not fallback to
         * the builtin decoder if no installed decoder plugin is able
         * to decode the image. This allows us to take advantage of
         * the cache below.
         **/

        if (mode == ImageExtractionMode_Preview &&
            decoded.get() != NULL)
        {
          // TODO Optimize this lookup for photometric interpretation:
          // It should be implemented by the plugin to avoid parsing
          // twice the DICOM file
          ParsedDicomFile parsed(dicomContent);
          
          LookupWindowingTags(dicomContent, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
        }
      }
#endif

      if (decoded.get() == NULL)
      {
        // Use Orthanc's built-in decoder, using the cache to speed-up
        // things on multi-frame images
        ServerContext::DicomCacheLocker locker(context, publicId);        
        decoded.reset(DicomImageDecoder::Decode(locker.GetDicom(), frame));
        LookupWindowingTags(locker.GetDicom(), windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);

        if (mode != ImageExtractionMode_Preview)
        {
          invert = false;
        }
      }
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_ParameterOutOfRange || e.GetErrorCode() == ErrorCode_UnknownResource)
      {
        // The frame number is out of the range for this DICOM
        // instance, the resource is not existent
      }
      else
      {
        std::string root = "";
        for (size_t i = 1; i < call.GetFullUri().size(); i++)
        {
          root += "../";
        }

        call.GetOutput().Redirect(root + "app/images/unsupported.png");
      }
      return;
    }

    if (mode == ImageExtractionMode_Preview
        && (decoded->GetFormat() == Orthanc::PixelFormat_Grayscale8 || decoded->GetFormat() == Orthanc::PixelFormat_Grayscale16))
    {
      ImageProcessing::ApplyWindowing(*decoded, *decoded, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
      invert = false; // don't invert it later on when encoding it, it has been inverted in the ApplyWindowing function
    }

    ImageToEncode image(decoded, mode, invert);

    HttpContentNegociation negociation;
    EncodePng png(image);
    negociation.Register(MIME_PNG, png);

    EncodeJpeg jpeg(image, call);
    negociation.Register(MIME_JPEG, jpeg);

    EncodePam pam(image);
    negociation.Register(MIME_PAM, pam);

    if (negociation.Apply(call.GetHttpHeaders()))
    {
      image.Answer(call.GetOutput());
    }
  }


  static void GetMatlabImage(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast&)
    {
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string dicomContent;
    context.ReadDicom(dicomContent, publicId);

#if ORTHANC_ENABLE_PLUGINS == 1
    IDicomImageDecoder& decoder = context.GetPlugins();
#else
    DefaultDicomImageDecoder decoder;  // This is Orthanc's built-in decoder
#endif

    std::auto_ptr<ImageAccessor> decoded(decoder.Decode(dicomContent.c_str(), dicomContent.size(), frame));

    std::string result;
    decoded->ToMatlabString(result);

    call.GetOutput().AnswerBuffer(result, MimeType_PlainText);
  }


  template <bool GzipCompression>
  static void GetRawFrame(RestApiGetCall& call)
  {
    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast&)
    {
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string raw;
    MimeType mime;

    {
      ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
      locker.GetDicom().GetRawFrame(raw, mime, frame);
    }

    if (GzipCompression)
    {
      GzipCompressor gzip;
      std::string compressed;
      gzip.Compress(compressed, raw.empty() ? NULL : raw.c_str(), raw.size());
      call.GetOutput().AnswerBuffer(compressed, MimeType_Gzip);
    }
    else
    {
      call.GetOutput().AnswerBuffer(raw, mime);
    }
  }


  static void GetResourceStatistics(RestApiGetCall& call)
  {
    static const uint64_t MEGA_BYTES = 1024 * 1024;

    std::string publicId = call.GetUriComponent("id", "");

    ResourceType type;
    uint64_t diskSize, uncompressedSize, dicomDiskSize, dicomUncompressedSize;
    unsigned int countStudies, countSeries, countInstances;
    OrthancRestApi::GetIndex(call).GetResourceStatistics(
      type, diskSize, uncompressedSize, countStudies, countSeries, 
      countInstances, dicomDiskSize, dicomUncompressedSize, publicId);

    Json::Value result = Json::objectValue;
    result["DiskSize"] = boost::lexical_cast<std::string>(diskSize);
    result["DiskSizeMB"] = static_cast<unsigned int>(diskSize / MEGA_BYTES);
    result["UncompressedSize"] = boost::lexical_cast<std::string>(uncompressedSize);
    result["UncompressedSizeMB"] = static_cast<unsigned int>(uncompressedSize / MEGA_BYTES);

    result["DicomDiskSize"] = boost::lexical_cast<std::string>(dicomDiskSize);
    result["DicomDiskSizeMB"] = static_cast<unsigned int>(dicomDiskSize / MEGA_BYTES);
    result["DicomUncompressedSize"] = boost::lexical_cast<std::string>(dicomUncompressedSize);
    result["DicomUncompressedSizeMB"] = static_cast<unsigned int>(dicomUncompressedSize / MEGA_BYTES);

    switch (type)
    {
      // Do NOT add "break" below this point!
      case ResourceType_Patient:
        result["CountStudies"] = countStudies;

      case ResourceType_Study:
        result["CountSeries"] = countSeries;

      case ResourceType_Series:
        result["CountInstances"] = countInstances;

      case ResourceType_Instance:
      default:
        break;
    }

    call.GetOutput().AnswerJson(result);
  }



  // Handling of metadata -----------------------------------------------------

  static void CheckValidResourceType(RestApiCall& call)
  {
    std::string resourceType = call.GetUriComponent("resourceType", "");
    StringToResourceType(resourceType.c_str());
  }


  static void ListMetadata(RestApiGetCall& call)
  {
    CheckValidResourceType(call);
    
    std::string publicId = call.GetUriComponent("id", "");
    std::map<MetadataType, std::string> metadata;

    OrthancRestApi::GetIndex(call).GetAllMetadata(metadata, publicId);

    Json::Value result;

    if (call.HasArgument("expand"))
    {
      result = Json::objectValue;
      
      for (std::map<MetadataType, std::string>::const_iterator 
             it = metadata.begin(); it != metadata.end(); ++it)
      {
        std::string key = EnumerationToString(it->first);
        result[key] = it->second;
      }      
    }
    else
    {
      result = Json::arrayValue;
      
      for (std::map<MetadataType, std::string>::const_iterator 
             it = metadata.begin(); it != metadata.end(); ++it)
      {       
        result.append(EnumerationToString(it->first));
      }
    }

    call.GetOutput().AnswerJson(result);
  }


  static void GetMetadata(RestApiGetCall& call)
  {
    CheckValidResourceType(call);
    
    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    std::string value;
    if (OrthancRestApi::GetIndex(call).LookupMetadata(value, publicId, metadata))
    {
      call.GetOutput().AnswerBuffer(value, MimeType_PlainText);
    }
  }


  static void DeleteMetadata(RestApiDeleteCall& call)
  {
    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    if (IsUserMetadata(metadata))  // It is forbidden to modify internal metadata
    {      
      OrthancRestApi::GetIndex(call).DeleteMetadata(publicId, metadata);
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  static void SetMetadata(RestApiPutCall& call)
  {
    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    std::string value;
    call.BodyToString(value);

    if (IsUserMetadata(metadata))  // It is forbidden to modify internal metadata
    {
      // It is forbidden to modify internal metadata
      OrthancRestApi::GetIndex(call).SetMetadata(publicId, metadata, value);
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }




  // Handling of attached files -----------------------------------------------

  static void ListAttachments(RestApiGetCall& call)
  {
    std::string resourceType = call.GetUriComponent("resourceType", "");
    std::string publicId = call.GetUriComponent("id", "");
    std::list<FileContentType> attachments;
    OrthancRestApi::GetIndex(call).ListAvailableAttachments(attachments, publicId, StringToResourceType(resourceType.c_str()));

    Json::Value result = Json::arrayValue;

    for (std::list<FileContentType>::const_iterator 
           it = attachments.begin(); it != attachments.end(); ++it)
    {
      result.append(EnumerationToString(*it));
    }

    call.GetOutput().AnswerJson(result);
  }


  static bool GetAttachmentInfo(FileInfo& info, RestApiCall& call)
  {
    CheckValidResourceType(call);
 
    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    return OrthancRestApi::GetIndex(call).LookupAttachment(info, publicId, contentType);
  }


  static void GetAttachmentOperations(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      Json::Value operations = Json::arrayValue;

      operations.append("compress");
      operations.append("compressed-data");

      if (info.GetCompressedMD5() != "")
      {
        operations.append("compressed-md5");
      }

      operations.append("compressed-size");
      operations.append("data");
      operations.append("is-compressed");

      if (info.GetUncompressedMD5() != "")
      {
        operations.append("md5");
      }

      operations.append("size");
      operations.append("uncompress");

      if (info.GetCompressedMD5() != "" &&
          info.GetUncompressedMD5() != "")
      {
        operations.append("verify-md5");
      }

      call.GetOutput().AnswerJson(operations);
    }
  }

  
  template <int uncompress>
  static void GetAttachmentData(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    CheckValidResourceType(call);
 
    std::string publicId = call.GetUriComponent("id", "");
    FileContentType type = StringToContentType(call.GetUriComponent("name", ""));

    if (uncompress)
    {
      context.AnswerAttachment(call.GetOutput(), publicId, type);
    }
    else
    {
      // Return the raw data (possibly compressed), as stored on the filesystem
      std::string content;
      context.ReadAttachment(content, publicId, type, false);
      call.GetOutput().AnswerBuffer(content, MimeType_Binary);
    }
  }


  static void GetAttachmentSize(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedSize()), MimeType_PlainText);
    }
  }


  static void GetAttachmentCompressedSize(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedSize()), MimeType_PlainText);
    }
  }


  static void GetAttachmentMD5(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call) &&
        info.GetUncompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedMD5()), MimeType_PlainText);
    }
  }


  static void GetAttachmentCompressedMD5(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call) &&
        info.GetCompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedMD5()), MimeType_PlainText);
    }
  }


  static void VerifyAttachment(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");

    FileInfo info;
    if (!GetAttachmentInfo(info, call) ||
        info.GetCompressedMD5() == "" ||
        info.GetUncompressedMD5() == "")
    {
      // Inexistent resource, or no MD5 available
      return;
    }

    bool ok = false;

    // First check whether the compressed data is correctly stored in the disk
    std::string data;
    context.ReadAttachment(data, publicId, StringToContentType(name), false);

    std::string actualMD5;
    Toolbox::ComputeMD5(actualMD5, data);
    
    if (actualMD5 == info.GetCompressedMD5())
    {
      // The compressed data is OK. If a compression algorithm was
      // applied to it, now check the MD5 of the uncompressed data.
      if (info.GetCompressionType() == CompressionType_None)
      {
        ok = true;
      }
      else
      {
        context.ReadAttachment(data, publicId, StringToContentType(name), true);        
        Toolbox::ComputeMD5(actualMD5, data);
        ok = (actualMD5 == info.GetUncompressedMD5());
      }
    }

    if (ok)
    {
      LOG(INFO) << "The attachment " << name << " of resource " << publicId << " has the right MD5";
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      LOG(INFO) << "The attachment " << name << " of resource " << publicId << " has bad MD5!";
    }
  }


  static void UploadAttachment(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    CheckValidResourceType(call);
 
    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");

    FileContentType contentType = StringToContentType(name);
    if (IsUserContentType(contentType) &&  // It is forbidden to modify internal attachments
        context.AddAttachment(publicId, StringToContentType(name), call.GetBodyData(), call.GetBodySize()))
    {
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  static void DeleteAttachment(RestApiDeleteCall& call)
  {
    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    bool allowed;
    if (IsUserContentType(contentType))
    {
      allowed = true;
    }
    else
    {
      OrthancConfiguration::ReaderLock lock;

      if (lock.GetConfiguration().GetBooleanParameter("StoreDicom", true) &&
          contentType == FileContentType_DicomAsJson)
      {
        allowed = true;
      }
      else
      {
        // It is forbidden to delete internal attachments, except for
        // the "DICOM as JSON" summary as of Orthanc 1.2.0 (this summary
        // would be automatically reconstructed on the next GET call)
        allowed = false;
      }
    }

    if (allowed) 
    {
      OrthancRestApi::GetIndex(call).DeleteAttachment(publicId, contentType);
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  template <enum CompressionType compression>
  static void ChangeAttachmentCompression(RestApiPostCall& call)
  {
    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    OrthancRestApi::GetContext(call).ChangeAttachmentCompression(publicId, contentType, compression);
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  static void IsAttachmentCompressed(RestApiGetCall& call)
  {
    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      std::string answer = (info.GetCompressionType() == CompressionType_None) ? "0" : "1";
      call.GetOutput().AnswerBuffer(answer, MimeType_PlainText);
    }
  }


  // Raw access to the DICOM tags of an instance ------------------------------

  static void GetRawContent(RestApiGetCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    locker.GetDicom().SendPathValue(call.GetOutput(), call.GetTrailingUri());
  }



  static bool ExtractSharedTags(Json::Value& shared,
                                ServerContext& context,
                                const std::string& publicId)
  {
    // Retrieve all the instances of this patient/study/series
    typedef std::list<std::string> Instances;
    Instances instances;
    context.GetIndex().GetChildInstances(instances, publicId);  // (*)

    // Loop over the instances
    bool isFirst = true;
    shared = Json::objectValue;

    for (Instances::const_iterator it = instances.begin();
         it != instances.end(); ++it)
    {
      // Get the tags of the current instance, in the simplified format
      Json::Value tags;

      try
      {
        context.ReadDicomAsJson(tags, *it);
      }
      catch (OrthancException&)
      {
        // Race condition: This instance has been removed since
        // (*). Ignore this instance.
        continue;
      }

      if (tags.type() != Json::objectValue)
      {
        return false;   // Error
      }

      // Only keep the tags that are mapped to a string
      Json::Value::Members members = tags.getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        const Json::Value& tag = tags[members[i]];
        if (tag.type() != Json::objectValue ||
            tag["Type"].type() != Json::stringValue ||
            tag["Type"].asString() != "String")
        {
          tags.removeMember(members[i]);
        }
      }

      if (isFirst)
      {
        // This is the first instance, keep its tags as such
        shared = tags;
        isFirst = false;
      }
      else
      {
        // Loop over all the members of the shared tags extracted so
        // far. If the value of one of these tags does not match its
        // value in the current instance, remove it.
        members = shared.getMemberNames();
        for (size_t i = 0; i < members.size(); i++)
        {
          if (!tags.isMember(members[i]) ||
              tags[members[i]]["Value"].asString() != shared[members[i]]["Value"].asString())
          {
            shared.removeMember(members[i]);
          }
        }
      }
    }

    return true;
  }


  static void GetSharedTags(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");

    Json::Value sharedTags;
    if (ExtractSharedTags(sharedTags, context, publicId))
    {
      // Success: Send the value of the shared tags
      AnswerDicomAsJson(call, sharedTags);
    }
  }


  static void GetModuleInternal(RestApiGetCall& call,
                                ResourceType resourceType,
                                DicomModule module)
  {
    if (!((resourceType == ResourceType_Patient && module == DicomModule_Patient) ||
          (resourceType == ResourceType_Study && module == DicomModule_Patient) ||
          (resourceType == ResourceType_Study && module == DicomModule_Study) ||
          (resourceType == ResourceType_Series && module == DicomModule_Series) ||
          (resourceType == ResourceType_Instance && module == DicomModule_Instance) ||
          (resourceType == ResourceType_Instance && module == DicomModule_Image)))
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, "ignore-length");

    typedef std::set<DicomTag> ModuleTags;
    ModuleTags moduleTags;
    DicomTag::AddTagsForModule(moduleTags, module);

    Json::Value tags;

    if (resourceType != ResourceType_Instance)
    {
      // Retrieve all the instances of this patient/study/series
      typedef std::list<std::string> Instances;
      Instances instances;
      context.GetIndex().GetChildInstances(instances, publicId);

      if (instances.empty())
      {
        return;   // Error: No instance (should never happen)
      }

      // Select one child instance
      publicId = instances.front();
    }

    context.ReadDicomAsJson(tags, publicId, ignoreTagLength);
    
    // Filter the tags of the instance according to the module
    Json::Value result = Json::objectValue;
    for (ModuleTags::const_iterator tag = moduleTags.begin(); tag != moduleTags.end(); ++tag)
    {
      std::string s = tag->Format();
      if (tags.isMember(s))
      {
        result[s] = tags[s];
      }      
    }

    AnswerDicomAsJson(call, result);
  }
    


  template <enum ResourceType resourceType, 
            enum DicomModule module>
  static void GetModule(RestApiGetCall& call)
  {
    GetModuleInternal(call, resourceType, module);
  }


  namespace
  {
    typedef std::list< std::pair<ResourceType, std::string> >  LookupResults;
  }


  static void AccumulateLookupResults(LookupResults& result,
                                      ServerIndex& index,
                                      const DicomTag& tag,
                                      const std::string& value,
                                      ResourceType level)
  {
    std::vector<std::string> tmp;
    index.LookupIdentifierExact(tmp, level, tag, value);

    for (size_t i = 0; i < tmp.size(); i++)
    {
      result.push_back(std::make_pair(level, tmp[i]));
    }
  }


  static void Lookup(RestApiPostCall& call)
  {
    std::string tag;
    call.BodyToString(tag);

    LookupResults resources;
    ServerIndex& index = OrthancRestApi::GetIndex(call);
    AccumulateLookupResults(resources, index, DICOM_TAG_PATIENT_ID, tag, ResourceType_Patient);
    AccumulateLookupResults(resources, index, DICOM_TAG_STUDY_INSTANCE_UID, tag, ResourceType_Study);
    AccumulateLookupResults(resources, index, DICOM_TAG_SERIES_INSTANCE_UID, tag, ResourceType_Series);
    AccumulateLookupResults(resources, index, DICOM_TAG_SOP_INSTANCE_UID, tag, ResourceType_Instance);

    Json::Value result = Json::arrayValue;    
    for (LookupResults::const_iterator 
           it = resources.begin(); it != resources.end(); ++it)
    {     
      ResourceType type = it->first;
      const std::string& id = it->second;
      
      Json::Value item = Json::objectValue;
      item["Type"] = EnumerationToString(type);
      item["ID"] = id;
      item["Path"] = GetBasePath(type, id);
    
      result.append(item);
    }

    call.GetOutput().AnswerJson(result);
  }


  namespace 
  {
    class FindVisitor : public ServerContext::ILookupVisitor
    {
    private:
      bool                    isComplete_;
      std::list<std::string>  resources_;

    public:
      FindVisitor() :
        isComplete_(false)
      {
      }

      virtual bool IsDicomAsJsonNeeded() const
      {
        return false;   // (*)
      }
      
      virtual void MarkAsComplete()
      {
        isComplete_ = true;  // Unused information as of Orthanc 1.5.0
      }

      virtual void Visit(const std::string& publicId,
                         const std::string& instanceId   /* unused     */,
                         const DicomMap& mainDicomTags   /* unused     */,
                         const Json::Value* dicomAsJson  /* unused (*) */) 
      {
        resources_.push_back(publicId);
      }

      void Answer(RestApiOutput& output,
                  ServerIndex& index,
                  ResourceType level,
                  bool expand) const
      {
        AnswerListOfResources(output, index, resources_, level, expand);
      }
    };
  }


  static void Find(RestApiPostCall& call)
  {
    static const char* const KEY_CASE_SENSITIVE = "CaseSensitive";
    static const char* const KEY_EXPAND = "Expand";
    static const char* const KEY_LEVEL = "Level";
    static const char* const KEY_LIMIT = "Limit";
    static const char* const KEY_QUERY = "Query";
    static const char* const KEY_SINCE = "Since";

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "The body must contain a JSON object");
    }
    else if (!request.isMember(KEY_LEVEL) ||
             request[KEY_LEVEL].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LEVEL) + "\" is missing, or should be a string");
    }
    else if (!request.isMember(KEY_QUERY) &&
             request[KEY_QUERY].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_QUERY) + "\" is missing, or should be a JSON object");
    }
    else if (request.isMember(KEY_CASE_SENSITIVE) && 
             request[KEY_CASE_SENSITIVE].type() != Json::booleanValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_CASE_SENSITIVE) + "\" should be a Boolean");
    }
    else if (request.isMember(KEY_LIMIT) && 
             request[KEY_LIMIT].type() != Json::intValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LIMIT) + "\" should be an integer");
    }
    else if (request.isMember(KEY_SINCE) &&
             request[KEY_SINCE].type() != Json::intValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_SINCE) + "\" should be an integer");
    }
    else
    {
      bool expand = false;
      if (request.isMember(KEY_EXPAND))
      {
        expand = request[KEY_EXPAND].asBool();
      }

      bool caseSensitive = false;
      if (request.isMember(KEY_CASE_SENSITIVE))
      {
        caseSensitive = request[KEY_CASE_SENSITIVE].asBool();
      }

      size_t limit = 0;
      if (request.isMember(KEY_LIMIT))
      {
        int tmp = request[KEY_LIMIT].asInt();
        if (tmp < 0)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Field \"" + std::string(KEY_LIMIT) + "\" should be a positive integer");
        }

        limit = static_cast<size_t>(tmp);
      }

      size_t since = 0;
      if (request.isMember(KEY_SINCE))
      {
        int tmp = request[KEY_SINCE].asInt();
        if (tmp < 0)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Field \"" + std::string(KEY_SINCE) + "\" should be a positive integer");
        }

        since = static_cast<size_t>(tmp);
      }

      ResourceType level = StringToResourceType(request[KEY_LEVEL].asCString());

      DatabaseLookup query;

      Json::Value::Members members = request[KEY_QUERY].getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        if (request[KEY_QUERY][members[i]].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadRequest,
                                 "Tag \"" + members[i] + "\" should be associated with a string");
        }

        const std::string value = request[KEY_QUERY][members[i]].asString();

        if (!value.empty())
        {
          // An empty string corresponds to an universal constraint,
          // so we ignore it. This mimics the behavior of class
          // "OrthancFindRequestHandler"
          query.AddRestConstraint(FromDcmtkBridge::ParseTag(members[i]), 
                                  value, caseSensitive, true);
        }
      }

      FindVisitor visitor;
      context.Apply(visitor, query, level, since, limit);
      visitor.Answer(call.GetOutput(), context.GetIndex(), level, expand);
    }
  }


  template <enum ResourceType start, 
            enum ResourceType end>
  static void GetChildResources(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::list<std::string> a, b, c;
    a.push_back(call.GetUriComponent("id", ""));

    ResourceType type = start;
    while (type != end)
    {
      b.clear();

      for (std::list<std::string>::const_iterator
             it = a.begin(); it != a.end(); ++it)
      {
        index.GetChildren(c, *it);
        b.splice(b.begin(), c);
      }

      type = GetChildResourceType(type);

      a.clear();
      a.splice(a.begin(), b);
    }

    Json::Value result = Json::arrayValue;

    for (std::list<std::string>::const_iterator
           it = a.begin(); it != a.end(); ++it)
    {
      Json::Value item;

      if (OrthancRestApi::GetIndex(call).LookupResource(item, *it, end))
      {
        result.append(item);
      }
    }

    call.GetOutput().AnswerJson(result);
  }


  static void GetChildInstancesTags(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");
    DicomToJsonFormat format = GetDicomFormat(call);

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, "ignore-length");

    // Retrieve all the instances of this patient/study/series
    typedef std::list<std::string> Instances;
    Instances instances;

    context.GetIndex().GetChildInstances(instances, publicId);  // (*)

    Json::Value result = Json::objectValue;

    for (Instances::const_iterator it = instances.begin();
         it != instances.end(); ++it)
    {
      Json::Value full;
      context.ReadDicomAsJson(full, *it, ignoreTagLength);

      if (format != DicomToJsonFormat_Full)
      {
        Json::Value simplified;
        ServerToolbox::SimplifyTags(simplified, full, format);
        result[*it] = simplified;
      }
      else
      {
        result[*it] = full;
      }
    }
    
    call.GetOutput().AnswerJson(result);
  }



  template <enum ResourceType start, 
            enum ResourceType end>
  static void GetParentResource(RestApiGetCall& call)
  {
    assert(start > end);

    ServerIndex& index = OrthancRestApi::GetIndex(call);
    
    std::string current = call.GetUriComponent("id", "");
    ResourceType currentType = start;
    while (currentType > end)
    {
      std::string parent;
      if (!index.LookupParent(parent, current))
      {
        // Error that could happen if the resource gets deleted by
        // another concurrent call
        return;
      }
      
      current = parent;
      currentType = GetParentResourceType(currentType);
    }

    assert(currentType == end);

    Json::Value result;
    if (index.LookupResource(result, current, end))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  static void ExtractPdf(RestApiGetCall& call)
  {
    const std::string id = call.GetUriComponent("id", "");

    std::string pdf;
    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    if (locker.GetDicom().ExtractPdf(pdf))
    {
      call.GetOutput().AnswerBuffer(pdf, MimeType_Pdf);
      return;
    }
  }


  static void OrderSlices(RestApiGetCall& call)
  {
    const std::string id = call.GetUriComponent("id", "");

    ServerIndex& index = OrthancRestApi::GetIndex(call);
    SliceOrdering ordering(index, id);

    Json::Value result;
    ordering.Format(result);
    call.GetOutput().AnswerJson(result);
  }


  static void GetInstanceHeader(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::string dicomContent;
    context.ReadDicom(dicomContent, publicId);

    // TODO Consider using "DicomMap::ParseDicomMetaInformation()" to
    // speed up things here

    ParsedDicomFile dicom(dicomContent);

    Json::Value header;
    dicom.HeaderToJson(header, DicomToJsonFormat_Full);

    AnswerDicomAsJson(call, header);
  }


  static void InvalidateTags(RestApiPostCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);
    
    // Loop over the instances, grouping them by parent studies so as
    // to avoid large memory consumption
    std::list<std::string> studies;
    index.GetAllUuids(studies, ResourceType_Study);

    for (std::list<std::string>::const_iterator 
           study = studies.begin(); study != studies.end(); ++study)
    {
      std::list<std::string> instances;
      index.GetChildInstances(instances, *study);

      for (std::list<std::string>::const_iterator 
             instance = instances.begin(); instance != instances.end(); ++instance)
      {
        index.DeleteAttachment(*instance, FileContentType_DicomAsJson);
      }
    }

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  template <enum ResourceType type>
  static void ReconstructResource(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    ServerToolbox::ReconstructResource(context, call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void ReconstructAllResources(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::list<std::string> studies;
    context.GetIndex().GetAllUuids(studies, ResourceType_Study);

    for (std::list<std::string>::const_iterator 
           study = studies.begin(); study != studies.end(); ++study)
    {
      ServerToolbox::ReconstructResource(context, *study);
    }
    
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  void OrthancRestApi::RegisterResources()
  {
    Register("/instances", ListResources<ResourceType_Instance>);
    Register("/patients", ListResources<ResourceType_Patient>);
    Register("/series", ListResources<ResourceType_Series>);
    Register("/studies", ListResources<ResourceType_Study>);

    Register("/instances/{id}", DeleteSingleResource<ResourceType_Instance>);
    Register("/instances/{id}", GetSingleResource<ResourceType_Instance>);
    Register("/patients/{id}", DeleteSingleResource<ResourceType_Patient>);
    Register("/patients/{id}", GetSingleResource<ResourceType_Patient>);
    Register("/series/{id}", DeleteSingleResource<ResourceType_Series>);
    Register("/series/{id}", GetSingleResource<ResourceType_Series>);
    Register("/studies/{id}", DeleteSingleResource<ResourceType_Study>);
    Register("/studies/{id}", GetSingleResource<ResourceType_Study>);

    Register("/instances/{id}/statistics", GetResourceStatistics);
    Register("/patients/{id}/statistics", GetResourceStatistics);
    Register("/studies/{id}/statistics", GetResourceStatistics);
    Register("/series/{id}/statistics", GetResourceStatistics);

    Register("/patients/{id}/shared-tags", GetSharedTags);
    Register("/series/{id}/shared-tags", GetSharedTags);
    Register("/studies/{id}/shared-tags", GetSharedTags);

    Register("/instances/{id}/module", GetModule<ResourceType_Instance, DicomModule_Instance>);
    Register("/patients/{id}/module", GetModule<ResourceType_Patient, DicomModule_Patient>);
    Register("/series/{id}/module", GetModule<ResourceType_Series, DicomModule_Series>);
    Register("/studies/{id}/module", GetModule<ResourceType_Study, DicomModule_Study>);
    Register("/studies/{id}/module-patient", GetModule<ResourceType_Study, DicomModule_Patient>);

    Register("/instances/{id}/file", GetInstanceFile);
    Register("/instances/{id}/export", ExportInstanceFile);
    Register("/instances/{id}/tags", GetInstanceTagsBis);
    Register("/instances/{id}/simplified-tags", GetInstanceTags<DicomToJsonFormat_Human>);
    Register("/instances/{id}/frames", ListFrames);

    Register("/instances/{id}/frames/{frame}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/frames/{frame}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/frames/{frame}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/frames/{frame}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/frames/{frame}/matlab", GetMatlabImage);
    Register("/instances/{id}/frames/{frame}/raw", GetRawFrame<false>);
    Register("/instances/{id}/frames/{frame}/raw.gz", GetRawFrame<true>);
    Register("/instances/{id}/pdf", ExtractPdf);
    Register("/instances/{id}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/matlab", GetMatlabImage);
    Register("/instances/{id}/header", GetInstanceHeader);

    Register("/patients/{id}/protected", IsProtectedPatient);
    Register("/patients/{id}/protected", SetPatientProtection);

    Register("/{resourceType}/{id}/metadata", ListMetadata);
    Register("/{resourceType}/{id}/metadata/{name}", DeleteMetadata);
    Register("/{resourceType}/{id}/metadata/{name}", GetMetadata);
    Register("/{resourceType}/{id}/metadata/{name}", SetMetadata);

    Register("/{resourceType}/{id}/attachments", ListAttachments);
    Register("/{resourceType}/{id}/attachments/{name}", DeleteAttachment);
    Register("/{resourceType}/{id}/attachments/{name}", GetAttachmentOperations);
    Register("/{resourceType}/{id}/attachments/{name}", UploadAttachment);
    Register("/{resourceType}/{id}/attachments/{name}/compress", ChangeAttachmentCompression<CompressionType_ZlibWithSize>);
    Register("/{resourceType}/{id}/attachments/{name}/compressed-data", GetAttachmentData<0>);
    Register("/{resourceType}/{id}/attachments/{name}/compressed-md5", GetAttachmentCompressedMD5);
    Register("/{resourceType}/{id}/attachments/{name}/compressed-size", GetAttachmentCompressedSize);
    Register("/{resourceType}/{id}/attachments/{name}/data", GetAttachmentData<1>);
    Register("/{resourceType}/{id}/attachments/{name}/is-compressed", IsAttachmentCompressed);
    Register("/{resourceType}/{id}/attachments/{name}/md5", GetAttachmentMD5);
    Register("/{resourceType}/{id}/attachments/{name}/size", GetAttachmentSize);
    Register("/{resourceType}/{id}/attachments/{name}/uncompress", ChangeAttachmentCompression<CompressionType_None>);
    Register("/{resourceType}/{id}/attachments/{name}/verify-md5", VerifyAttachment);

    Register("/tools/invalidate-tags", InvalidateTags);
    Register("/tools/lookup", Lookup);
    Register("/tools/find", Find);

    Register("/patients/{id}/studies", GetChildResources<ResourceType_Patient, ResourceType_Study>);
    Register("/patients/{id}/series", GetChildResources<ResourceType_Patient, ResourceType_Series>);
    Register("/patients/{id}/instances", GetChildResources<ResourceType_Patient, ResourceType_Instance>);
    Register("/studies/{id}/series", GetChildResources<ResourceType_Study, ResourceType_Series>);
    Register("/studies/{id}/instances", GetChildResources<ResourceType_Study, ResourceType_Instance>);
    Register("/series/{id}/instances", GetChildResources<ResourceType_Series, ResourceType_Instance>);

    Register("/studies/{id}/patient", GetParentResource<ResourceType_Study, ResourceType_Patient>);
    Register("/series/{id}/patient", GetParentResource<ResourceType_Series, ResourceType_Patient>);
    Register("/series/{id}/study", GetParentResource<ResourceType_Series, ResourceType_Study>);
    Register("/instances/{id}/patient", GetParentResource<ResourceType_Instance, ResourceType_Patient>);
    Register("/instances/{id}/study", GetParentResource<ResourceType_Instance, ResourceType_Study>);
    Register("/instances/{id}/series", GetParentResource<ResourceType_Instance, ResourceType_Series>);

    Register("/patients/{id}/instances-tags", GetChildInstancesTags);
    Register("/studies/{id}/instances-tags", GetChildInstancesTags);
    Register("/series/{id}/instances-tags", GetChildInstancesTags);

    Register("/instances/{id}/content/*", GetRawContent);

    Register("/series/{id}/ordered-slices", OrderSlices);

    Register("/patients/{id}/reconstruct", ReconstructResource<ResourceType_Patient>);
    Register("/studies/{id}/reconstruct", ReconstructResource<ResourceType_Study>);
    Register("/series/{id}/reconstruct", ReconstructResource<ResourceType_Series>);
    Register("/instances/{id}/reconstruct", ReconstructResource<ResourceType_Instance>);
    Register("/tools/reconstruct", ReconstructAllResources);
  }
}
