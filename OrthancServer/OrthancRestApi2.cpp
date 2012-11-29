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


#define RETRIEVE_CONTEXT(call)                                          \
  OrthancRestApi2& context = dynamic_cast<OrthancRestApi2&>(call.GetContext())


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

    CompressionType compressionType;
    std::string fileUuid;
    std::string publicId = call.GetUriComponent("id", "");

    if (context.GetIndex().GetFile(fileUuid, compressionType, publicId, AttachedFileType_Dicom))
    {
      assert(compressionType == CompressionType_None);

      FilesystemHttpSender sender(context.GetFileStorage(), fileUuid);
      sender.SetFilename(fileUuid + ".dcm");
      sender.SetContentType("application/dicom");
      call.GetOutput().AnswerFile(sender);
    }
  }


  template <bool simplify>
  static void GetInstanceTags(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    CompressionType compressionType;
    std::string fileUuid;
    std::string publicId = call.GetUriComponent("id", "");

    if (context.GetIndex().GetFile(fileUuid, compressionType, publicId, AttachedFileType_Json))
    {
      assert(compressionType == CompressionType_None);

      Json::Value full;
      ReadJson(full, context.GetFileStorage(), fileUuid);

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
  }



  // DICOM bridge -------------------------------------------------------------

  static void ListModalities(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value result = Json::arrayValue;

    for (OrthancRestApi2::Modalities::const_iterator 
           it = context.GetModalities().begin();
         it != context.GetModalities().end(); it++)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }



  // Registration of the various REST handlers --------------------------------

  OrthancRestApi2::OrthancRestApi2(ServerIndex& index,
                                   const std::string& path) :
    index_(index),
    storage_(path)
  {
    GetListOfDicomModalities(modalities_);

    Register("/", ServeRoot);
    Register("/system", GetSystemInformation);
    Register("/changes", GetChanges);
    Register("/modalities", ListModalities);

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

    // TODO : "content", "frames"
  }
}
