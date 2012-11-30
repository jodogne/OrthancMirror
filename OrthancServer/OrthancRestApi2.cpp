/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "OrthancRestApi2.h"

#include "OrthancInitialization.h"
#include "FromDcmtkBridge.h"
#include "../Core/Uuid.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "ServerToolbox.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>


#define RETRIEVE_CONTEXT(call)                                          \
  OrthancRestApi2& contextApi =                                         \
    dynamic_cast<OrthancRestApi2&>(call.GetContext());                  \
  ServerContext& context = contextApi.GetContext()


namespace Orthanc
{
  // System information -------------------------------------------------------

  static void ServeRoot(RestApi::GetCall& call)
  {
    call.GetOutput().Redirect("app/explorer.html");
  }
 
  static void GetSystemInformation(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value result = Json::objectValue;
    result["Version"] = ORTHANC_VERSION;
    result["Name"] = GetGlobalStringParameter("Name", "");
    result["TotalCompressedSize"] = boost::lexical_cast<std::string>(context.GetIndex().GetTotalCompressedSize());
    result["TotalUncompressedSize"] = boost::lexical_cast<std::string>(context.GetIndex().GetTotalUncompressedSize());
    call.GetOutput().AnswerJson(result);
  }


  // List all the patients, studies, series or instances ----------------------
 
  template <enum ResourceType resourceType>
  static void ListResources(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value result;
    context.GetIndex().GetAllUuids(result, resourceType);
    call.GetOutput().AnswerJson(result);
  }

  template <enum ResourceType resourceType>
  static void GetSingleResource(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value result;
    if (context.GetIndex().LookupResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }

  template <enum ResourceType resourceType>
  static void DeleteSingleResource(RestApi::DeleteCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value result;
    if (context.GetIndex().DeleteResource(result, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  // Changes API --------------------------------------------------------------
 
  static void GetChanges(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    static const unsigned int MAX_RESULTS = 100;
    ServerIndex& index = context.GetIndex();
        
    //std::string filter = GetArgument(getArguments, "filter", "");
    int64_t since;
    unsigned int limit;
    try
    {
      since = boost::lexical_cast<int64_t>(call.GetArgument("since", "0"));
      limit = boost::lexical_cast<unsigned int>(call.GetArgument("limit", "0"));
    }
    catch (boost::bad_lexical_cast)
    {
      return;
    }

    if (limit == 0 || limit > MAX_RESULTS)
    {
      limit = MAX_RESULTS;
    }

    Json::Value result;
    if (index.GetChanges(result, since, limit))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  
  // Get information about a single instance ----------------------------------
 
  static void GetInstanceFile(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    std::string publicId = call.GetUriComponent("id", "");
    context.AnswerFile(call.GetOutput(), publicId, AttachedFileType_Dicom);
  }


  template <bool simplify>
  static void GetInstanceTags(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    std::string publicId = call.GetUriComponent("id", "");
    
    Json::Value full;
    context.ReadJson(full, publicId);

    if (simplify)
    {
      Json::Value simplified;
      SimplifyTags(simplified, full);
      call.GetOutput().AnswerJson(simplified);
    }
    else
    {
      call.GetOutput().AnswerJson(full);
    }
  }

  
  static void ListFrames(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value instance;
    if (context.GetIndex().LookupResource(instance, call.GetUriComponent("id", ""), ResourceType_Instance))
    {
      unsigned int numberOfFrames = 1;

      try
      {
        Json::Value tmp = instance["MainDicomTags"]["NumberOfFrames"];
        numberOfFrames = boost::lexical_cast<unsigned int>(tmp.asString());
      }
      catch (...)
      {
      }

      Json::Value result = Json::arrayValue;
      for (unsigned int i = 0; i < numberOfFrames; i++)
      {
        result.append(i);
      }

      call.GetOutput().AnswerJson(result);
    }
  }


  template <enum ImageExtractionMode mode>
  static void GetImage(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    CompressionType compressionType;
    std::string fileUuid;
    std::string publicId = call.GetUriComponent("id", "");
    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast)
    {
      return;
    }

    if (context.GetIndex().GetFile(fileUuid, compressionType, publicId, AttachedFileType_Dicom))
    {
      assert(compressionType == CompressionType_None);

      std::string dicomContent, png;
      context.GetFileStorage().ReadFile(dicomContent, fileUuid);

      try
      {
        FromDcmtkBridge::ExtractPngImage(png, dicomContent, frame, mode);
        call.GetOutput().AnswerBuffer(png, "image/png");
      }
      catch (OrthancException& e)
      {
        if (e.GetErrorCode() == ErrorCode_ParameterOutOfRange)
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
      }
    }
  }


  // Upload of DICOM files through HTTP ---------------------------------------

  static void UploadDicomFile(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);

    const std::string& postData = call.GetPostBody();

    LOG(INFO) << "Receiving a DICOM file of " << postData.size() << " bytes through HTTP";

    // Prepare an input stream for the memory buffer
    DcmInputBufferStream is;
    if (postData.size() > 0)
    {
      is.setBuffer(&postData[0], postData.size());
    }
    is.setEos();

    DcmFileFormat dicomFile;
    if (!dicomFile.read(is).good())
    {
      call.GetOutput().SignalError(Orthanc_HttpStatus_415_UnsupportedMediaType);
      return;
    }

    DicomMap dicomSummary;
    FromDcmtkBridge::Convert(dicomSummary, *dicomFile.getDataset());

    DicomInstanceHasher hasher(dicomSummary);

    Json::Value dicomJson;
    FromDcmtkBridge::ToJson(dicomJson, *dicomFile.getDataset());
      
    StoreStatus status = StoreStatus_Failure;
    if (postData.size() > 0)
    {
      status = context.Store
        (reinterpret_cast<const char*>(&postData[0]),
         postData.size(), dicomSummary, dicomJson, "");
    }

    Json::Value result = Json::objectValue;

    if (status != StoreStatus_Failure)
    {
      result["ID"] = hasher.HashInstance();
      result["Path"] = GetBasePath(ResourceType_Instance, hasher.HashInstance());
    }

    result["Status"] = ToString(status);
    call.GetOutput().AnswerJson(result);
  }


  // DICOM bridge -------------------------------------------------------------

  static void ListModalities(RestApi::GetCall& call)
  {
    const OrthancRestApi2::Modalities& m = 
      dynamic_cast<OrthancRestApi2&>(call.GetContext()).GetModalities();

    Json::Value result = Json::arrayValue;

    for (OrthancRestApi2::Modalities::const_iterator 
           it = m.begin(); it != m.end(); it++)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }



  // Registration of the various REST handlers --------------------------------

  OrthancRestApi2::OrthancRestApi2(ServerContext& context) : 
    context_(context)
  {
    GetListOfDicomModalities(modalities_);

    Register("/", ServeRoot);
    Register("/system", GetSystemInformation);
    Register("/changes", GetChanges);
    Register("/modalities", ListModalities);

    Register("/instances", UploadDicomFile);
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

    Register("/instances/{id}/file", GetInstanceFile);
    Register("/instances/{id}/tags", GetInstanceTags<false>);
    Register("/instances/{id}/simplified-tags", GetInstanceTags<true>);
    Register("/instances/{id}/frames", ListFrames);

    Register("/instances/{id}/frames/{frame}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/frames/{frame}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/frames/{frame}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/image-uint16", GetImage<ImageExtractionMode_UInt16>);

    // TODO : "content"
  }
}
