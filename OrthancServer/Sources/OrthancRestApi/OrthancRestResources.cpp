/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../../OrthancFramework/Sources/DicomFormat/DicomImageInformation.h"
#include "../../../OrthancFramework/Sources/DicomParsing/DicomWebJsonVisitor.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../../OrthancFramework/Sources/HttpServer/HttpContentNegociation.h"
#include "../../../OrthancFramework/Sources/Images/Image.h"
#include "../../../OrthancFramework/Sources/Images/ImageProcessing.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/MultiThreading/Semaphore.h"

#include "../OrthancConfiguration.h"
#include "../Search/DatabaseLookup.h"
#include "../ServerContext.h"
#include "../ServerToolbox.h"
#include "../SliceOrdering.h"

#include "../../Plugins/Engine/OrthancPlugins.h"

// This "include" is mandatory for Release builds using Linux Standard Base
#include <boost/math/special_functions/round.hpp>


/**
 * This semaphore is used to limit the number of concurrent HTTP
 * requests on CPU-intensive routes of the REST API, in order to
 * prevent exhaustion of resources (new in Orthanc 1.7.0).
 **/
static Orthanc::Semaphore throttlingSemaphore_(4);  // TODO => PARAMETER?


namespace Orthanc
{
  static std::string GetDocumentationSampleResource(ResourceType type)
  {
    switch (type)
    {
      case Orthanc::ResourceType_Instance:
        return "https://demo.orthanc-server.com/instances/d94d9a03-3003b047-a4affc69-322313b2-680530a2";
        break;
        
      case Orthanc::ResourceType_Series:
        return "https://demo.orthanc-server.com/series/37836232-d13a2350-fa1dedc5-962b31aa-010f8e52";
        break;
        
      case Orthanc::ResourceType_Study:
        return "https://demo.orthanc-server.com/studies/27f7126f-4f66fb14-03f4081b-f9341db2-53925988";
        break;
        
      case Orthanc::ResourceType_Patient:
        return "https://demo.orthanc-server.com/patients/46e6332c-677825b6-202fcf7c-f787bc5f-7b07c382";
        break;
        
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
  

  static void AnswerDicomAsJson(RestApiCall& call,
                                const Json::Value& dicom,
                                DicomToJsonFormat mode)
  {
    if (mode != DicomToJsonFormat_Full)
    {
      Json::Value simplified;
      Toolbox::SimplifyDicomAsJson(simplified, dicom, mode);
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
    if (call.IsDocumentation())
    {
      const std::string resources = GetResourceTypeText(resourceType, true /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("List the available " + resources)
        .SetDescription("List the Orthanc identifiers of all the available DICOM " + resources)
        .SetHttpGetArgument("limit", RestApiCallDocumentation::Type_Number, "Limit the number of results", false)
        .SetHttpGetArgument("since", RestApiCallDocumentation::Type_Number, "Show only the resources since the provided index", false)
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, retrieve detailed information about the individual " + resources, false)
        .AddAnswerType(MimeType_Json, "JSON array containing either the Orthanc identifiers, or detailed information "
                       "about the reported " + resources + " (if `expand` argument is provided)")
        .SetHttpGetSample("https://demo.orthanc-server.com/" + resources + "?since=0&limit=2", true);
      return;
    }
    
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
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Get information about some " + resource)
        .SetDescription("Get detailed information about the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .AddAnswerType(MimeType_Json, "Information about the DICOM " + resource)
        .SetHttpGetSample(GetDocumentationSampleResource(resourceType), true);
      return;
    }
    
    Json::Value result;
    if (OrthancRestApi::GetIndex(call).LookupResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }

  template <enum ResourceType resourceType>
  static void DeleteSingleResource(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Delete some " + resource)
        .SetDescription("Delete the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest");
      return;
    }

    Json::Value result;
    if (OrthancRestApi::GetContext(call).DeleteResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  // Get information about a single patient -----------------------------------
 
  static void IsProtectedPatient(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Patients")
        .SetSummary("Is the patient protected against recycling?")
        .SetUriArgument("id", "Orthanc identifier of the patient of interest")
        .AddAnswerType(MimeType_PlainText, "`1` if protected, `0` if not protected");
      return;
    }
    
    std::string publicId = call.GetUriComponent("id", "");
    bool isProtected = OrthancRestApi::GetIndex(call).IsProtectedPatient(publicId);
    call.GetOutput().AnswerBuffer(isProtected ? "1" : "0", MimeType_PlainText);
  }


  static void SetPatientProtection(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Patients")
        .SetSummary("Protect one patient against recycling")
        .SetDescription("Check out configuration options `MaximumStorageSize` and `MaximumPatientCount`")
        .SetUriArgument("id", "Orthanc identifier of the patient of interest");
      return;
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    context.GetIndex().SetProtectedPatient(publicId, call.ParseBooleanBody());
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  // Get information about a single instance ----------------------------------
 
  static void GetInstanceFile(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Download DICOM")
        .SetDescription("Download one DICOM instance")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetHttpHeader("Accept", "This HTTP header can be set to retrieve the DICOM instance in DICOMweb format")
        .AddAnswerType(MimeType_Dicom, "The DICOM instance")
        .AddAnswerType(MimeType_DicomWebJson, "The DICOM instance, in DICOMweb JSON format")
        .AddAnswerType(MimeType_DicomWebXml, "The DICOM instance, in DICOMweb XML format");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    HttpToolbox::Arguments::const_iterator accept = call.GetHttpHeaders().find("accept");
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Write DICOM onto filesystem")
        .SetDescription("Write the DICOM file onto the filesystem where Orthanc is running")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddRequestType(MimeType_PlainText, "Target path on the filesystem");
      return;
    }

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
    if (call.IsDocumentation())
    {
      if (format == DicomToJsonFormat_Human)
      {
        call.GetDocumentation()
          .SetTag("Instances")
          .SetSummary("Get human-readable tags")
          .SetDescription("Get the DICOM tags in human-readable format")
          .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
          .SetHttpGetArgument("ignore-length", RestApiCallDocumentation::Type_JsonListOfStrings,
                              "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
          .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags and their associated value")
          .SetTruncatedJsonHttpGetSample("https://demo.orthanc-server.com/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/simplified-tags", 10);
        return;
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }

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
      // This path allows one to avoid the JSON decoding if no
      // simplification is asked, and if no "ignore-length" argument
      // is present
      Json::Value full;
      context.ReadDicomAsJson(full, publicId);
      call.GetOutput().AnswerJson(full);
    }
  }


  static void GetInstanceTagsBis(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get DICOM tags")
        .SetDescription("Get the DICOM tags in the specified format. By default, the `full` format is used, which "
                        "combines hexadecimal tags with human-readable description.")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetHttpGetArgument("simplify", RestApiCallDocumentation::Type_String,
                            "If present, report the DICOM tags in human-readable format "
                            "(same as the `/instances/{id}/simplified-tags` route)", false)
        .SetHttpGetArgument("short", RestApiCallDocumentation::Type_String,
                            "If present, report the DICOM tags indexed in hexadecimal format", false)
        .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags and their associated value")
        .SetTruncatedJsonHttpGetSample("https://demo.orthanc-server.com/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/tags", 10);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("List available frames")
        .SetDescription("List the frames that are available in the DICOM instance of interest")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddAnswerType(MimeType_Json, "The list of the indices of the available frames")
        .SetHttpGetSample("https://demo.orthanc-server.com/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/frames", true);      
      return;
    }

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
      std::unique_ptr<ImageAccessor>&  image_;
      ImageExtractionMode            mode_;
      bool                           invert_;
      MimeType                       format_;
      std::string                    answer_;

    public:
      ImageToEncode(std::unique_ptr<ImageAccessor>& image,
                    ImageExtractionMode mode,
                    bool invert) :
        image_(image),
        mode_(mode),
        invert_(invert),
        format_(MimeType_Binary)
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
      explicit EncodePng(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype) ORTHANC_OVERRIDE
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
      explicit EncodePam(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype) ORTHANC_OVERRIDE
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
                          const std::string& subtype) ORTHANC_OVERRIDE
      {
        assert(type == "image");
        assert(subtype == "jpeg");
        image_.EncodeUsingJpeg(quality_);
      }
    };
  }


  namespace
  {
    class IDecodedFrameHandler : public boost::noncopyable
    {
    public:
      virtual ~IDecodedFrameHandler()
      {
      }

      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const DicomMap& dicom) = 0;

      virtual bool RequiresDicomTags() const = 0;

      static void Apply(RestApiGetCall& call,
                        IDecodedFrameHandler& handler,
                        ImageExtractionMode mode /* for generation of documentation */,
                        bool isRendered /* for generation of documentation */)
      {
        if (call.IsDocumentation())
        {
          std::string m;
          if (!isRendered)
          {
            switch (mode)
            {
              case ImageExtractionMode_Preview:
                m = "preview";
                break;
              case ImageExtractionMode_UInt8:
                m = "uint8";
                break;
              case ImageExtractionMode_UInt16:
                m = "uint16";
                break;
              case ImageExtractionMode_Int16:
                m = "int16";
                break;
              default:
                throw OrthancException(ErrorCode_ParameterOutOfRange);
            }
          }
          
          std::string description;
          std::string verb = (isRendered ? "Render" : "Decode");
          
          if (call.HasUriComponent("frame"))
          {
            description = verb + " one frame of interest from the given DICOM instance.";
            call.GetDocumentation()
              .SetSummary(verb + " a frame" + (m.empty() ? "" : " (" + m + ")"))
              .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");
          }
          else
          {
            description = verb + " the first frame of the given DICOM instance.";
            call.GetDocumentation()
              .SetSummary(verb + " an image" + (m.empty() ? "" : " (" + m + ")"));
          }

          if (isRendered)
          {
            description += (" This function takes scaling into account (`RescaleSlope` and `RescaleIntercept` tags), "
                            "as well as the default windowing stored in the DICOM file (`WindowCenter` and `WindowWidth`tags), "
                            "and can be used to resize the resulting image. Color images are not affected by windowing.");
            call.GetDocumentation()
              .SetHttpGetArgument("window-center",RestApiCallDocumentation::Type_Number, "Windowing center", false)
              .SetHttpGetArgument("window-width",RestApiCallDocumentation::Type_Number, "Windowing width", false)
              .SetHttpGetArgument("width",RestApiCallDocumentation::Type_Number, "Width of the resized image", false)
              .SetHttpGetArgument("height",RestApiCallDocumentation::Type_Number, "Height of the resized image", false)
              .SetHttpGetArgument("smooth",RestApiCallDocumentation::Type_Boolean, "Whether to smooth image on resize", false);
          }
          else
          {
            switch (mode)
            {
              case ImageExtractionMode_Preview:
                description += " The full dynamic range of grayscale images is rescaled to the [0,255] range.";
                break;
              case ImageExtractionMode_UInt8:
                description += " Pixels of grayscale images are truncated to the [0,255] range.";
                break;
              case ImageExtractionMode_UInt16:
                description += " Pixels of grayscale images are truncated to the [0,65535] range.";
                break;
              case ImageExtractionMode_Int16:
                description += (" Pixels of grayscale images are truncated to the [-32768,32767] range. "
                                "Negative values must be interpreted according to two's complement.");
                break;
              default:
                throw OrthancException(ErrorCode_ParameterOutOfRange);
            }
          }
          
          call.GetDocumentation()
            .SetTag("Instances")
            .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
            .SetHttpGetArgument("quality", RestApiCallDocumentation::Type_Number, "Quality for JPEG images (between 1 and 100, defaults to 90)", false)
            .SetHttpHeader("Accept", "Format of the resulting image. Can be `image/png` (default), `image/jpeg` or `image/x-portable-arbitrarymap`")
            .AddAnswerType(MimeType_Png, "PNG image")
            .AddAnswerType(MimeType_Jpeg, "JPEG image")
            .AddAnswerType(MimeType_Pam, "PAM image (Portable Arbitrary Map)")
            .SetDescription(description);

          return;
        }
        
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

        DicomMap dicom;
        std::unique_ptr<ImageAccessor> decoded;

        try
        {
          std::string publicId = call.GetUriComponent("id", "");

          decoded.reset(context.DecodeDicomFrame(publicId, frame));

          if (decoded.get() == NULL)
          {
            throw OrthancException(ErrorCode_NotImplemented,
                                   "Cannot decode DICOM instance with ID: " + publicId);
          }
          
          if (handler.RequiresDicomTags())
          {
            /**
             * Retrieve a summary of the DICOM tags, which is
             * necessary to deal with MONOCHROME1 photometric
             * interpretation, and with windowing parameters.
             **/ 
            ServerContext::DicomCacheLocker locker(context, publicId);
            OrthancConfiguration::DefaultExtractDicomSummary(dicom, locker.GetDicom());
          }
        }
        catch (OrthancException& e)
        {
          if (e.GetErrorCode() == ErrorCode_ParameterOutOfRange ||
              e.GetErrorCode() == ErrorCode_UnknownResource)
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

        handler.Handle(call, decoded, dicom);
      }


      static void DefaultHandler(RestApiGetCall& call,
                                 std::unique_ptr<ImageAccessor>& decoded,
                                 ImageExtractionMode mode,
                                 bool invert)
      {
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
    };


    class GetImageHandler : public IDecodedFrameHandler
    {
    private:
      ImageExtractionMode mode_;

    public:
      explicit GetImageHandler(ImageExtractionMode mode) :
        mode_(mode)
      {
      }

      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const DicomMap& dicom) ORTHANC_OVERRIDE
      {
        bool invert = false;

        if (mode_ == ImageExtractionMode_Preview)
        {
          DicomImageInformation info(dicom);
          invert = (info.GetPhotometricInterpretation() == PhotometricInterpretation_Monochrome1);
        }

        DefaultHandler(call, decoded, mode_, invert);
      }

      virtual bool RequiresDicomTags() const ORTHANC_OVERRIDE
      {
        return mode_ == ImageExtractionMode_Preview;
      }
    };


    class RenderedFrameHandler : public IDecodedFrameHandler
    {
    private:
      static void GetDicomParameters(bool& invert,
                                     float& rescaleSlope,
                                     float& rescaleIntercept,
                                     float& windowWidth,
                                     float& windowCenter,
                                     const DicomMap& dicom)
      {
        DicomImageInformation info(dicom);

        invert = (info.GetPhotometricInterpretation() == PhotometricInterpretation_Monochrome1);

        rescaleSlope = 1.0f;
        rescaleIntercept = 0.0f;

        if (dicom.HasTag(Orthanc::DICOM_TAG_RESCALE_SLOPE) &&
            dicom.HasTag(Orthanc::DICOM_TAG_RESCALE_INTERCEPT))
        {
          dicom.ParseFloat(rescaleSlope, Orthanc::DICOM_TAG_RESCALE_SLOPE);
          dicom.ParseFloat(rescaleIntercept, Orthanc::DICOM_TAG_RESCALE_INTERCEPT);
        }

        windowWidth = static_cast<float>(1 << info.GetBitsStored()) * rescaleSlope;
        windowCenter = windowWidth / 2.0f + rescaleIntercept;

        if (dicom.HasTag(Orthanc::DICOM_TAG_WINDOW_CENTER) &&
            dicom.HasTag(Orthanc::DICOM_TAG_WINDOW_WIDTH))
        {
          dicom.ParseFirstFloat(windowCenter, Orthanc::DICOM_TAG_WINDOW_CENTER);
          dicom.ParseFirstFloat(windowWidth, Orthanc::DICOM_TAG_WINDOW_WIDTH);
        }
      }

      static void GetUserArguments(float& windowWidth /* inout */,
                                   float& windowCenter /* inout */,
                                   unsigned int& argWidth,
                                   unsigned int& argHeight,
                                   bool& smooth,
                                   const RestApiGetCall& call)
      {
        static const char* ARG_WINDOW_CENTER = "window-center";
        static const char* ARG_WINDOW_WIDTH = "window-width";
        static const char* ARG_WIDTH = "width";
        static const char* ARG_HEIGHT = "height";
        static const char* ARG_SMOOTH = "smooth";

        if (call.HasArgument(ARG_WINDOW_WIDTH))
        {
          try
          {
            windowWidth = boost::lexical_cast<float>(call.GetArgument(ARG_WINDOW_WIDTH, ""));
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_WINDOW_WIDTH));
          }
        }

        if (call.HasArgument(ARG_WINDOW_CENTER))
        {
          try
          {
            windowCenter = boost::lexical_cast<float>(call.GetArgument(ARG_WINDOW_CENTER, ""));
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_WINDOW_CENTER));
          }
        }

        argWidth = 0;
        argHeight = 0;

        if (call.HasArgument(ARG_WIDTH))
        {
          try
          {
            int tmp = boost::lexical_cast<int>(call.GetArgument(ARG_WIDTH, ""));
            if (tmp < 0)
            {
              throw OrthancException(ErrorCode_ParameterOutOfRange,
                                     "Argument cannot be negative: " + std::string(ARG_WIDTH));
            }
            else
            {
              argWidth = static_cast<unsigned int>(tmp);
            }
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_WIDTH));
          }
        }

        if (call.HasArgument(ARG_HEIGHT))
        {
          try
          {
            int tmp = boost::lexical_cast<int>(call.GetArgument(ARG_HEIGHT, ""));
            if (tmp < 0)
            {
              throw OrthancException(ErrorCode_ParameterOutOfRange,
                                     "Argument cannot be negative: " + std::string(ARG_HEIGHT));
            }
            else
            {
              argHeight = static_cast<unsigned int>(tmp);
            }
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_HEIGHT));
          }
        }

        smooth = false;

        if (call.HasArgument(ARG_SMOOTH))
        {
          smooth = RestApiCall::ParseBoolean(call.GetArgument(ARG_SMOOTH, ""));
        }        
      }
                                
      
    public:
      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const DicomMap& dicom) ORTHANC_OVERRIDE
      {
        bool invert;
        float rescaleSlope, rescaleIntercept, windowWidth, windowCenter;
        GetDicomParameters(invert, rescaleSlope, rescaleIntercept, windowWidth, windowCenter, dicom);

        unsigned int argWidth, argHeight;
        bool smooth;
        GetUserArguments(windowWidth, windowCenter, argWidth, argHeight, smooth, call);

        unsigned int targetWidth = decoded->GetWidth();
        unsigned int targetHeight = decoded->GetHeight();

        if (decoded->GetWidth() != 0 &&
            decoded->GetHeight() != 0)
        {
          float ratio = 1;

          if (argWidth != 0 &&
              argHeight != 0)
          {
            float ratioX = static_cast<float>(argWidth) / static_cast<float>(decoded->GetWidth());
            float ratioY = static_cast<float>(argHeight) / static_cast<float>(decoded->GetHeight());
            ratio = std::min(ratioX, ratioY);
          }
          else if (argWidth != 0)
          {
            ratio = static_cast<float>(argWidth) / static_cast<float>(decoded->GetWidth());
          }
          else if (argHeight != 0)
          {
            ratio = static_cast<float>(argHeight) / static_cast<float>(decoded->GetHeight());
          }
          
          targetWidth = boost::math::iround(ratio * static_cast<float>(decoded->GetWidth()));
          targetHeight = boost::math::iround(ratio * static_cast<float>(decoded->GetHeight()));
        }
        
        if (decoded->GetFormat() == PixelFormat_RGB24)
        {
          if (targetWidth == decoded->GetWidth() &&
              targetHeight == decoded->GetHeight())
          {
            DefaultHandler(call, decoded, ImageExtractionMode_Preview, false);
          }
          else
          {
            std::unique_ptr<ImageAccessor> resized(
              new Image(decoded->GetFormat(), targetWidth, targetHeight, false));
            
            if (smooth &&
                (targetWidth < decoded->GetWidth() ||
                 targetHeight < decoded->GetHeight()))
            {
              ImageProcessing::SmoothGaussian5x5(*decoded, false /* be fast, don't round */);
            }
            
            ImageProcessing::Resize(*resized, *decoded);
            DefaultHandler(call, resized, ImageExtractionMode_Preview, false);
          }
        }
        else
        {
          // Grayscale image: (1) convert to Float32, (2) apply
          // windowing to get a Grayscale8, (3) possibly resize

          Image converted(PixelFormat_Float32, decoded->GetWidth(), decoded->GetHeight(), false);
          ImageProcessing::Convert(converted, *decoded);

          // Avoid divisions by zero
          if (windowWidth <= 1.0f)
          {
            windowWidth = 1;
          }

          if (std::abs(rescaleSlope) <= 0.1f)
          {
            rescaleSlope = 0.1f;
          }

          const float scaling = 255.0f * rescaleSlope / windowWidth;
          const float offset = (rescaleIntercept - windowCenter + windowWidth / 2.0f) / rescaleSlope;

          std::unique_ptr<ImageAccessor> rescaled(new Image(PixelFormat_Grayscale8, decoded->GetWidth(), decoded->GetHeight(), false));
          ImageProcessing::ShiftScale(*rescaled, converted, offset, scaling, false);

          if (targetWidth == decoded->GetWidth() &&
              targetHeight == decoded->GetHeight())
          {
            DefaultHandler(call, rescaled, ImageExtractionMode_UInt8, invert);
          }
          else
          {
            std::unique_ptr<ImageAccessor> resized(
              new Image(PixelFormat_Grayscale8, targetWidth, targetHeight, false));
            
            if (smooth &&
                (targetWidth < decoded->GetWidth() ||
                 targetHeight < decoded->GetHeight()))
            {
              ImageProcessing::SmoothGaussian5x5(*rescaled, false /* be fast, don't round */);
            }
            
            ImageProcessing::Resize(*resized, *rescaled);
            DefaultHandler(call, resized, ImageExtractionMode_UInt8, invert);
          }
        }
      }

      virtual bool RequiresDicomTags() const ORTHANC_OVERRIDE
      {
        return true;
      }
    };
  }


  template <enum ImageExtractionMode mode>
  static void GetImage(RestApiGetCall& call)
  {
    Semaphore::Locker locker(throttlingSemaphore_);
        
    GetImageHandler handler(mode);
    IDecodedFrameHandler::Apply(call, handler, mode, false /* not rendered */);
  }


  static void GetRenderedFrame(RestApiGetCall& call)
  {
    Semaphore::Locker locker(throttlingSemaphore_);
        
    RenderedFrameHandler handler;
    IDecodedFrameHandler::Apply(call, handler, ImageExtractionMode_Preview /* arbitrary value */, true);
  }


  static void GetMatlabImage(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      std::string description;
      
      if (call.HasUriComponent("frame"))
      {
        description = "Decode one frame of interest from the given DICOM instance";
        call.GetDocumentation()
          .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");
      }
      else
      {
        description = "Decode the first frame of the given DICOM instance.";
      }

      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Decode frame for Matlab")
        .SetDescription(description + ", and export this frame as a Octave/Matlab matrix to be imported with `eval()`: "
                        "https://book.orthanc-server.com/faq/matlab.html")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddAnswerType(MimeType_PlainText, "Octave/Matlab matrix");
      return;
    }

    Semaphore::Locker locker(throttlingSemaphore_);
        
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
    std::unique_ptr<ImageAccessor> decoded(context.DecodeDicomFrame(publicId, frame));

    if (decoded.get() == NULL)
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "Cannot decode DICOM instance with ID: " + publicId);
    }
    else
    {
      std::string result;
      decoded->ToMatlabString(result);
      call.GetOutput().AnswerBuffer(result, MimeType_PlainText);
    }
  }


  template <bool GzipCompression>
  static void GetRawFrame(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Access raw frame" + std::string(GzipCompression ? " (compressed)" : ""))
        .SetDescription("Access the raw content of one individual frame of the DICOM instance of interest, "
                        "bypassing image decoding. This is notably useful to access the source files "
                        "in compressed transfer syntaxes." +
                        std::string(GzipCompression ? " The image is compressed using gzip" : ""))
        .SetUriArgument("id", "Orthanc identifier of the instance of interest")
        .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");

      if (GzipCompression)
      {
        call.GetDocumentation().AddAnswerType(MimeType_Gzip, "The raw frame, compressed using gzip");
      }
      else
      {
        call.GetDocumentation().AddAnswerType(MimeType_Binary, "The raw frame");
      }
      return;
    }
    
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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get " + r + " statistics")
        .SetDescription("Get statistics about the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetAnswerField("DiskSize", RestApiCallDocumentation::Type_String,
                        "Size of the " + r + " on the disk in bytes, expressed as a string for 64bit compatibility with JSON")
        .SetAnswerField("DiskSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size of the " + r + " on the disk, expressed in megabytes (MB)")
        .SetAnswerField("UncompressedSize", RestApiCallDocumentation::Type_String,
                        "Size of the " + r + " after decompression in bytes, expressed as a string for 64bit compatibility with JSON")
        .SetAnswerField("UncompressedSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size of the " + r + " after decompression, expressed in megabytes (MB). "
                        "This is different from `DiskSizeMB` iff `StorageCompression` is `true`.")
        .SetAnswerField("DicomDiskSize", RestApiCallDocumentation::Type_String,
                        "Size on the disk of the DICOM instances associated with the " + r + ", expressed in bytes")
        .SetAnswerField("DicomDiskSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size on the disk of the DICOM instances associated with the " + r + ", expressed in megabytes (MB)")
        .SetAnswerField("DicomUncompressedSize", RestApiCallDocumentation::Type_String,
                        "Size on the disk of the uncompressed DICOM instances associated with the " + r + ", expressed in bytes")
        .SetAnswerField("DicomUncompressedSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size on the disk of the uncompressed DICOM instances associated with the " + r + ", expressed in megabytes (MB)")
        .SetHttpGetSample(GetDocumentationSampleResource(t) + "/statistics", true);

      switch (t)
      {
        // Do NOT add "break" below this point!
        case ResourceType_Patient:
          call.GetDocumentation().SetAnswerField("CountStudies", RestApiCallDocumentation::Type_Number,
                                                 "Number of child studies within this " + r);

        case ResourceType_Study:
          call.GetDocumentation().SetAnswerField("CountSeries", RestApiCallDocumentation::Type_Number,
                                                 "Number of child series within this " + r);

        case ResourceType_Series:
          call.GetDocumentation().SetAnswerField("CountInstances", RestApiCallDocumentation::Type_Number,
                                                 "Number of child instances within this " + r);

        case ResourceType_Instance:
        default:
          break;
      }

      return;
    }

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

  static void CheckValidResourceType(const RestApiCall& call)
  {
    assert(!call.GetFullUri().empty());
    const std::string resourceType = call.GetFullUri() [0];
    StringToResourceType(resourceType.c_str());
  }


  static void ListMetadata(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("List metadata")
        .SetDescription("Get the list of metadata that are associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, also retrieve the value of the individual metadata", false)
        .AddAnswerType(MimeType_Json, "JSON array containing the names of the available metadata, "
                       "or JSON associative array mapping metadata to their values (if `expand` argument is provided)")
        .SetHttpGetSample(GetDocumentationSampleResource(t) + "/metadata", true);
      return;
    }

    std::map<MetadataType, std::string> metadata;

    assert(!call.GetFullUri().empty());
    const std::string publicId = call.GetUriComponent("id", "");
    const ResourceType level = StringToResourceType(call.GetFullUri() [0].c_str());

    OrthancRestApi::GetIndex(call).GetAllMetadata(metadata, publicId, level);

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get metadata")
        .SetDescription("Get the value of a metadata that is associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)")
        .AddAnswerType(MimeType_PlainText, "Value of the metadata");
      return;
    }

    assert(!call.GetFullUri().empty());
    const std::string publicId = call.GetUriComponent("id", "");
    const ResourceType level = StringToResourceType(call.GetFullUri() [0].c_str());

    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    std::string value;
    if (OrthancRestApi::GetIndex(call).LookupMetadata(value, publicId, level, metadata))
    {
      call.GetOutput().AnswerBuffer(value, MimeType_PlainText);
    }
  }


  static void DeleteMetadata(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Delete metadata")
        .SetDescription("Delete some metadata associated with the given DICOM " + r +
                        ". This call will fail if trying to delete a system metadata (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Set metadata")
        .SetDescription("Set the value of some metadata in the given DICOM " + r +
                        ". This call will fail if trying to modify a system metadata (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)")
        .AddRequestType(MimeType_PlainText, "String value of the metadata");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("List attachments")
        .SetDescription("Get the list of attachments that are associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .AddAnswerType(MimeType_Json, "JSON array containing the names of the attachments")
        .SetHttpGetSample(GetDocumentationSampleResource(t) + "/attachments", true);
      return;
    }

    const std::string resourceType = call.GetFullUri() [0];
    const std::string publicId = call.GetUriComponent("id", "");
    std::set<FileContentType> attachments;
    OrthancRestApi::GetIndex(call).ListAvailableAttachments(attachments, publicId, StringToResourceType(resourceType.c_str()));

    Json::Value result = Json::arrayValue;

    for (std::set<FileContentType>::const_iterator 
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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag("Other")
        .SetSummary("List operations on attachments")
        .SetDescription("Get the list of the operations that are available for attachments associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_Json, "List of the available operations")
        .SetHttpGetSample("https://demo.orthanc-server.com/instances/d94d9a03-3003b047-a4affc69-322313b2-680530a2/attachments/dicom", true);
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get attachment" + std::string(uncompress ? "" : " (no decompression)"))
        .SetDescription("Get the (binary) content of one attachment associated with the given " + r +
                        std::string(uncompress ? "" : ". The attachment will not be decompressed if `StorageCompression` is `true`."))
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_Binary, "The attachment");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get size of attachment")
        .SetDescription("Get the size of one attachment associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_PlainText, "The size of the attachment");
      return;
    }

    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedSize()), MimeType_PlainText);
    }
  }


  static void GetAttachmentCompressedSize(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get size of attachment on disk")
        .SetDescription("Get the size of one attachment associated with the given " + r + ", as stored on the disk. "
                        "This is different from `.../size` iff `EnableStorage` is `true`.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_PlainText, "The size of the attachment, as stored on the disk");
      return;
    }

    FileInfo info;
    if (GetAttachmentInfo(info, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedSize()), MimeType_PlainText);
    }
  }


  static void GetAttachmentMD5(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get MD5 of attachment")
        .SetDescription("Get the MD5 hash of one attachment associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_PlainText, "The MD5 of the attachment");
      return;
    }

    FileInfo info;
    if (GetAttachmentInfo(info, call) &&
        info.GetUncompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedMD5()), MimeType_PlainText);
    }
  }


  static void GetAttachmentCompressedMD5(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get MD5 of attachment on disk")
        .SetDescription("Get the MD5 hash of one attachment associated with the given " + r + ", as stored on the disk. "
                        "This is different from `.../md5` iff `EnableStorage` is `true`.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_PlainText, "The MD5 of the attachment, as stored on the disk");
      return;
    }

    FileInfo info;
    if (GetAttachmentInfo(info, call) &&
        info.GetCompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedMD5()), MimeType_PlainText);
    }
  }


  static void VerifyAttachment(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Verify attachment")
        .SetDescription("Verify that the attachment is not corrupted, by validating its MD5 hash")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_Json, "On success, a valid JSON object is returned");
      return;
    }

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
      CLOG(INFO, HTTP) << "The attachment " << name << " of resource " << publicId << " has the right MD5";
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      CLOG(INFO, HTTP) << "The attachment " << name << " of resource " << publicId << " has bad MD5!";
    }
  }


  static void UploadAttachment(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Set attachment")
        .SetDescription("Attach a file to the given DICOM " + r +
                        ". This call will fail if trying to modify a system attachment (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddRequestType(MimeType_Binary, "Binary data containing the attachment")
        .AddAnswerType(MimeType_Json, "Empty JSON object in the case of a success");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Delete attachment")
        .SetDescription("Delete an attachment associated with the given DICOM " + r +
                        ". This call will fail if trying to delete a system attachment (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary(compression == CompressionType_None ? "Uncompress attachment" : "Compress attachment")
        .SetDescription("Change the compression scheme that is used to store an attachment.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)");
      return;
    }

    CheckValidResourceType(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    OrthancRestApi::GetContext(call).ChangeAttachmentCompression(publicId, contentType, compression);
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  static void IsAttachmentCompressed(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Is attachment compressed?")
        .SetDescription("Test whether the attachment has been stored as a compressed file on the disk.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_PlainText, "`0` if the attachment was stored uncompressed, `1` if it was compressed");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get raw tag")
        .SetDescription("Get the raw content of one DICOM tag in the hierarchy of DICOM dataset")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetUriArgument("...", "Path to the DICOM tag. This is the interleaving of one DICOM tag, possibly followed "
                        "by an index for sequences. Sequences are accessible as, for instance, `/0008-1140/1/0008-1150`")
        .AddAnswerType(MimeType_Binary, "The raw value of the tag of intereset "
                       "(binary data, whose memory layout depends on the underlying transfer syntax), "
                       "or JSON array containing the list of available tags if accessing a dataset");
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get shared tags")
        .SetDescription("Extract the DICOM tags whose value is constant across all the child instances of "
                        "the DICOM " + r + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .AddAnswerType(MimeType_Json, "JSON object containing the values of the DICOM tags")
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(t) + "/shared-tags", 5);
      return;
    }

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
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      std::string m;
      switch (module)
      {
        case DicomModule_Patient:
          m = "patient";
          break;
        case DicomModule_Study:
          m = "study";
          break;
        case DicomModule_Series:
          m = "series";
          break;
        case DicomModule_Instance:
          m = "instance";
          break;
        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Get " + m + " module" + std::string(resource == m ? "" : " of " + resource))
        .SetDescription("Get the " + m + " module of the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .SetHttpGetArgument("ignore-length", RestApiCallDocumentation::Type_JsonListOfStrings,
                            "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
        .AddAnswerType(MimeType_Json, "Information about the DICOM " + resource)
        .SetHttpGetSample(GetDocumentationSampleResource(resourceType) + "/" + (*call.GetFullUri().rbegin()), true);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Look for DICOM identifiers")
        .SetDescription("This URI can be used to convert one DICOM identifier to a list of matching Orthanc resources")
        .AddRequestType(MimeType_PlainText, "The DICOM identifier of interest (i.e. the value of `PatientID`, "
                        "`StudyInstanceUID`, `SeriesInstanceUID`, or `SOPInstanceUID`)")
        .AddAnswerType(MimeType_Json, "JSON array containing a list of matching Orthanc resources, each item in the "
                       "list corresponding to a JSON object with the fields `Type`, `ID` and `Path` identifying one "
                       "DICOM resource that is stored by Orthanc");
      return;
    }

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
      
      virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
      {
        return false;   // (*)
      }
      
      virtual void MarkAsComplete() ORTHANC_OVERRIDE
      {
        isComplete_ = true;  // Unused information as of Orthanc 1.5.0
      }

      virtual void Visit(const std::string& publicId,
                         const std::string& instanceId   /* unused     */,
                         const DicomMap& mainDicomTags   /* unused     */,
                         const Json::Value* dicomAsJson  /* unused (*) */)  ORTHANC_OVERRIDE
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

    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Look for local resources")
        .SetDescription("This URI can be used to perform a search on the content of the local Orthanc server, "
                        "in a way that is similar to querying remote DICOM modalities using C-FIND SCU: "
                        "https://book.orthanc-server.com/users/rest.html#performing-finds-within-orthanc")
        .SetRequestField(KEY_CASE_SENSITIVE, RestApiCallDocumentation::Type_Boolean,
                         "Enable case-sensitive search for PN value representations (defaults to configuration option `CaseSensitivePN`)", false)
        .SetRequestField(KEY_EXPAND, RestApiCallDocumentation::Type_Boolean,
                         "Also retrieve the content of the matching resources, not only their Orthanc identifiers", false)
        .SetRequestField(KEY_LEVEL, RestApiCallDocumentation::Type_String,
                         "Level of the query (`Patient`, `Study`, `Series` or `Instance`)", true)
        .SetRequestField(KEY_LIMIT, RestApiCallDocumentation::Type_Number,
                         "Limit the number of reported resources", false)
        .SetRequestField(KEY_SINCE, RestApiCallDocumentation::Type_Number,
                         "Show only the resources since the provided index (in conjunction with `Limit`)", false)
        .SetRequestField(KEY_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the DICOM tags", true)
        .AddAnswerType(MimeType_Json, "JSON array containing either the Orthanc identifiers, or detailed information "
                       "about the reported resources (if `Expand` argument is `true`)");
      return;
    }

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
    if (call.IsDocumentation())
    {
      const std::string children = GetResourceTypeText(end, true /* plural */, false /* lower case */);
      const std::string resource = GetResourceTypeText(start, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(start, true /* plural */, true /* upper case */))
        .SetSummary("Get child " + children)
        .SetDescription("Get detailed information about the child " + children + " of the DICOM " +
                        resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .AddAnswerType(MimeType_Json, "JSON array containing information about the child DICOM " + children)
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(start) + "/" + children, 5);
      return;
    }

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
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Get tags of instances")
        .SetDescription("Get the tags of all the child instances of the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument("ignore-length", RestApiCallDocumentation::Type_JsonListOfStrings,
                            "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
        .AddAnswerType(MimeType_Json, "JSON object associating the Orthanc identifiers of the instances, with the values of their DICOM tags")
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(t) + "/instances-tags", 5);
      return;
    }

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
        Toolbox::SimplifyDicomAsJson(simplified, full, format);
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

    if (call.IsDocumentation())
    {
      const std::string parent = GetResourceTypeText(end, false /* plural */, false /* lower case */);
      const std::string resource = GetResourceTypeText(start, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(start, true /* plural */, true /* upper case */))
        .SetSummary("Get parent " + parent)
        .SetDescription("Get detailed information about the parent " + parent + " of the DICOM " +
                        resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .AddAnswerType(MimeType_Json, "Information about the parent DICOM " + parent)
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(start) + "/" + parent, 10);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get embedded PDF")
        .SetDescription("Get the PDF file that is embedded in one DICOM instance. "
                        "If the DICOM instance doesn't contain the `EncapsulatedDocument` tag or if the "
                        "`MIMETypeOfEncapsulatedDocument` tag doesn't correspond to the PDF type, a `404` HTTP error is raised.")
        .SetUriArgument("id", "Orthanc identifier of the instance interest")
        .AddAnswerType(MimeType_Pdf, "PDF file");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Series")
        .SetSummary("Order the slices")
        .SetDescription("Sort the instances and frames (slices) of the DICOM series whose Orthanc identifier is provided in the URL. "
                        "This URI is essentially used by the Orthanc Web viewer and by the Osimis Web viewer.")
        .SetUriArgument("id", "Orthanc identifier of the series of interest")
        .SetAnswerField("Dicom", RestApiCallDocumentation::Type_JsonListOfStrings,
                        "Ordered list of paths to DICOM instances")
        .SetAnswerField("Slices", RestApiCallDocumentation::Type_JsonListOfStrings,
                        "Ordered list of paths to frames. It is recommended to use this field, as it is also valid "
                        "in the case of multiframe images.")
        .SetAnswerField("SlicesShort", RestApiCallDocumentation::Type_JsonListOfObjects,
                        "Same information as the `Slices` field, but in a compact form")
        .SetAnswerField("Type", RestApiCallDocumentation::Type_String,
                        "Can be `Volume` (for 3D volumes) or `Sequence` (notably for cine images)")
        .SetTruncatedJsonHttpGetSample("https://demo.orthanc-server.com/series/1e2c125c-411b8e86-3f4fe68e-a7584dd3-c6da78f0/ordered-slices", 10);
      return;
    }

    const std::string id = call.GetUriComponent("id", "");

    ServerIndex& index = OrthancRestApi::GetIndex(call);
    SliceOrdering ordering(index, id);

    Json::Value result;
    ordering.Format(result);
    call.GetOutput().AnswerJson(result);
  }


  static void GetInstanceHeader(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get DICOM meta-header")
        .SetDescription("Get the DICOM tags in the meta-header of the DICOM instance. By default, the `full` format is used, which "
                        "combines hexadecimal tags with human-readable description.")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetHttpGetArgument("simplify", RestApiCallDocumentation::Type_String,
                            "If present, report the DICOM tags in human-readable format", false)
        .SetHttpGetArgument("short", RestApiCallDocumentation::Type_String,
                            "If present, report the DICOM tags indexed in hexadecimal format", false)
        .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags and their associated value")
        .SetHttpGetSample("https://demo.orthanc-server.com/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/header", true);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::string dicomContent;
    context.ReadDicom(dicomContent, publicId);

    // TODO Consider using "DicomMap::ParseDicomMetaInformation()" to
    // speed up things here

    ParsedDicomFile dicom(dicomContent);

    Json::Value header;
    OrthancConfiguration::DefaultDicomHeaderToJson(header, dicom);

    AnswerDicomAsJson(call, header);
  }


  static void InvalidateTags(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Invalidate DICOM-as-JSON summaries")
        .SetDescription("Remove all the attachments of the type \"DICOM-as-JSON\" that are associated will all the "
                        "DICOM instances stored in Orthanc. These summaries will be automatically re-created on the next access. "
                        "This is notably useful after changes to the `Dictionary` configuration option. "
                        "https://book.orthanc-server.com/faq/orthanc-storage.html#storage-area");
      return;
    }

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
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(type, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(type, true /* plural */, true /* upper case */))
        .SetSummary("Reconstruct tags of " + resource)
        .SetDescription("Reconstruct the main DICOM tags of the " + resource + " whose Orthanc identifier is provided "
                        "in the URL. This is useful if child studies/series/instances have inconsistent values for "
                        "higher-level tags, in order to force Orthanc to use the value from the resource of interest. "
                        "Beware that this is a time-consuming operation, as all the children DICOM instances will be "
                        "parsed again, and the Orthanc index will be updated accordingly.")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    ServerToolbox::ReconstructResource(context, call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void ReconstructAllResources(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Reconstruct all the index")
        .SetDescription("Reconstruct the index of all the tags of all the DICOM instances that are stored in Orthanc. "
                        "This is notably useful after the deletion of resources whose children resources have inconsistent "
                        "values with their sibling resources. Beware that this is a highly time-consuming operation, "
                        "as all the DICOM instances will be parsed again, and as all the Orthanc index will be regenerated.");
      return;
    }

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

    Register("/instances/{id}/frames/{frame}", RestApi::AutoListChildren);
    Register("/instances/{id}/frames/{frame}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/frames/{frame}/rendered", GetRenderedFrame);
    Register("/instances/{id}/frames/{frame}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/frames/{frame}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/frames/{frame}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/frames/{frame}/matlab", GetMatlabImage);
    Register("/instances/{id}/frames/{frame}/raw", GetRawFrame<false>);
    Register("/instances/{id}/frames/{frame}/raw.gz", GetRawFrame<true>);
    Register("/instances/{id}/pdf", ExtractPdf);
    Register("/instances/{id}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/rendered", GetRenderedFrame);
    Register("/instances/{id}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/matlab", GetMatlabImage);
    Register("/instances/{id}/header", GetInstanceHeader);

    Register("/patients/{id}/protected", IsProtectedPatient);
    Register("/patients/{id}/protected", SetPatientProtection);

    std::vector<std::string> resourceTypes;
    resourceTypes.push_back("patients");
    resourceTypes.push_back("studies");
    resourceTypes.push_back("series");
    resourceTypes.push_back("instances");

    for (size_t i = 0; i < resourceTypes.size(); i++)
    {
      Register("/" + resourceTypes[i] + "/{id}/metadata", ListMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", DeleteMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", GetMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", SetMetadata);

      Register("/" + resourceTypes[i] + "/{id}/attachments", ListAttachments);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", DeleteAttachment);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", GetAttachmentOperations);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", UploadAttachment);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compress", ChangeAttachmentCompression<CompressionType_ZlibWithSize>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-data", GetAttachmentData<0>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-md5", GetAttachmentCompressedMD5);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-size", GetAttachmentCompressedSize);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/data", GetAttachmentData<1>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/is-compressed", IsAttachmentCompressed);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/md5", GetAttachmentMD5);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/size", GetAttachmentSize);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/uncompress", ChangeAttachmentCompression<CompressionType_None>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/verify-md5", VerifyAttachment);
    }

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
