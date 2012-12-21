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


#include "OrthancRestApi.h"

#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/Uuid.h"
#include "../Core/Compression/HierarchicalZipWriter.h"
#include "DicomProtocol/DicomUserConnection.h"
#include "FromDcmtkBridge.h"
#include "OrthancInitialization.h"
#include "ServerToolbox.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>


#define RETRIEVE_CONTEXT(call)                          \
  OrthancRestApi& contextApi =                          \
    dynamic_cast<OrthancRestApi&>(call.GetContext());   \
  ServerContext& context = contextApi.GetContext()

#define RETRIEVE_MODALITIES(call)                                       \
  const OrthancRestApi::Modalities& modalities =                        \
    dynamic_cast<OrthancRestApi&>(call.GetContext()).GetModalities();



namespace Orthanc
{
  // DICOM SCU ----------------------------------------------------------------

  static void ConnectToModality(DicomUserConnection& connection,
                                const std::string& name)
  {
    std::string aet, address;
    int port;
    GetDicomModality(name, aet, address, port);
    connection.SetLocalApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));
    connection.SetDistantApplicationEntityTitle(aet);
    connection.SetDistantHost(address);
    connection.SetDistantPort(port);
    connection.Open();
  }

  static bool MergeQueryAndTemplate(DicomMap& result,
                                    const std::string& postData)
  {
    Json::Value query;
    Json::Reader reader;

    if (!reader.parse(postData, query) ||
        query.type() != Json::objectValue)
    {
      return false;
    }

    Json::Value::Members members = query.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      DicomTag t = FromDcmtkBridge::ParseTag(members[i]);
      result.SetValue(t, query[members[i]].asString());
    }

    return true;
  }

  static void DicomFindPatient(RestApi::PostCall& call)
  {
    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }

    DicomUserConnection connection;
    ConnectToModality(connection, call.GetUriComponent("id", ""));

    DicomFindAnswers answers;
    connection.FindPatient(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindStudy(RestApi::PostCall& call)
  {
    DicomMap m;
    DicomMap::SetupFindStudyTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }

    if (m.GetValue(DICOM_TAG_ACCESSION_NUMBER).AsString().size() <= 2 &&
        m.GetValue(DICOM_TAG_PATIENT_ID).AsString().size() <= 2)
    {
      return;
    }        
      
    DicomUserConnection connection;
    ConnectToModality(connection, call.GetUriComponent("id", ""));
  
    DicomFindAnswers answers;
    connection.FindStudy(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindSeries(RestApi::PostCall& call)
  {
    DicomMap m;
    DicomMap::SetupFindSeriesTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }

    if ((m.GetValue(DICOM_TAG_ACCESSION_NUMBER).AsString().size() <= 2 &&
         m.GetValue(DICOM_TAG_PATIENT_ID).AsString().size() <= 2) ||
        m.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).AsString().size() <= 2)
    {
      return;
    }        
         
    DicomUserConnection connection;
    ConnectToModality(connection, call.GetUriComponent("id", ""));
  
    DicomFindAnswers answers;
    connection.FindSeries(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFind(RestApi::PostCall& call)
  {
    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }
 
    DicomUserConnection connection;
    ConnectToModality(connection, call.GetUriComponent("id", ""));
  
    DicomFindAnswers patients;
    connection.FindPatient(patients, m);

    // Loop over the found patients
    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < patients.GetSize(); i++)
    {
      Json::Value patient(Json::objectValue);
      FromDcmtkBridge::ToJson(patient, patients.GetAnswer(i));

      DicomMap::SetupFindStudyTemplate(m);
      if (!MergeQueryAndTemplate(m, call.GetPostBody()))
      {
        return;
      }
      m.CopyTagIfExists(patients.GetAnswer(i), DICOM_TAG_PATIENT_ID);

      DicomFindAnswers studies;
      connection.FindStudy(studies, m);

      patient["Studies"] = Json::arrayValue;
      
      // Loop over the found studies
      for (size_t j = 0; j < studies.GetSize(); j++)
      {
        Json::Value study(Json::objectValue);
        FromDcmtkBridge::ToJson(study, studies.GetAnswer(j));

        DicomMap::SetupFindSeriesTemplate(m);
        if (!MergeQueryAndTemplate(m, call.GetPostBody()))
        {
          return;
        }
        m.CopyTagIfExists(studies.GetAnswer(j), DICOM_TAG_PATIENT_ID);
        m.CopyTagIfExists(studies.GetAnswer(j), DICOM_TAG_STUDY_INSTANCE_UID);

        DicomFindAnswers series;
        connection.FindSeries(series, m);

        // Loop over the found series
        study["Series"] = Json::arrayValue;
        for (size_t k = 0; k < series.GetSize(); k++)
        {
          Json::Value series2(Json::objectValue);
          FromDcmtkBridge::ToJson(series2, series.GetAnswer(k));
          study["Series"].append(series2);
        }

        patient["Studies"].append(study);
      }

      result.append(patient);
    }
    
    call.GetOutput().AnswerJson(result);
  }


  static void DicomStore(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);

    std::string remote = call.GetUriComponent("id", "");
    DicomUserConnection connection;
    ConnectToModality(connection, remote);

    const std::string& resourceId = call.GetPostBody();

    Json::Value found;
    if (context.GetIndex().LookupResource(found, resourceId, ResourceType_Series))
    {
      // The UUID corresponds to a series
      context.GetIndex().LogExportedResource(resourceId, remote);

      for (Json::Value::ArrayIndex i = 0; i < found["Instances"].size(); i++)
      {
        std::string instanceId = found["Instances"][i].asString();
        std::string dicom;
        context.ReadFile(dicom, instanceId, FileContentType_Dicom);
        connection.Store(dicom);
      }

      call.GetOutput().AnswerBuffer("{}", "application/json");
    }
    else if (context.GetIndex().LookupResource(found, resourceId, ResourceType_Instance))
    {
      // The UUID corresponds to an instance
      context.GetIndex().LogExportedResource(resourceId, remote);

      std::string dicom;
      context.ReadFile(dicom, resourceId, FileContentType_Dicom);
      connection.Store(dicom);

      call.GetOutput().AnswerBuffer("{}", "application/json");
    }
    else
    {
      // The POST body is not a known resource, assume that it
      // contains a raw DICOM instance
      connection.Store(resourceId);
      call.GetOutput().AnswerBuffer("{}", "application/json");
    }
  }



  // System information -------------------------------------------------------

  static void ServeRoot(RestApi::GetCall& call)
  {
    call.GetOutput().Redirect("app/explorer.html");
  }
 
  static void GetSystemInformation(RestApi::GetCall& call)
  {
    Json::Value result = Json::objectValue;

    result["Version"] = ORTHANC_VERSION;
    result["Name"] = GetGlobalStringParameter("Name", "");

    call.GetOutput().AnswerJson(result);
  }

  static void GetStatistics(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);
    Json::Value result = Json::objectValue;
    context.GetIndex().ComputeStatistics(result);
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


  // Download of ZIP files ----------------------------------------------------
 

  static std::string GetDirectoryNameInArchive(const Json::Value& resource,
                                               ResourceType resourceType)
  {
    switch (resourceType)
    {
      case ResourceType_Patient:
      {
        std::string p = resource["MainDicomTags"]["PatientID"].asString();
        std::string n = resource["MainDicomTags"]["PatientName"].asString();
        return p + " " + n;
      }

      case ResourceType_Study:
      {
        return resource["MainDicomTags"]["StudyDescription"].asString();
      }
        
      case ResourceType_Series:
      {
        std::string d = resource["MainDicomTags"]["SeriesDescription"].asString();
        std::string m = resource["MainDicomTags"]["Modality"].asString();
        return m + " " + d;
      }
        
      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  static bool CreateRootDirectoryInArchive(HierarchicalZipWriter& writer,
                                           ServerContext& context,
                                           const Json::Value& resource,
                                           ResourceType resourceType)
  {
    if (resourceType == ResourceType_Patient)
    {
      return true;
    }

    ResourceType parentType = GetParentResourceType(resourceType);
    Json::Value parent;

    switch (resourceType)
    {
      case ResourceType_Study:
      {
        if (!context.GetIndex().LookupResource(parent, resource["ParentPatient"].asString(), parentType))
        {
          return false;
        }

        break;
      }
        
      case ResourceType_Series:
        if (!context.GetIndex().LookupResource(parent, resource["ParentStudy"].asString(), parentType) ||
            !CreateRootDirectoryInArchive(writer, context, parent, parentType))
        {
          return false;
        }
        break;
        
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    writer.OpenDirectory(GetDirectoryNameInArchive(parent, parentType).c_str());
    return true;
  }

  static bool ArchiveInstance(HierarchicalZipWriter& writer,
                              ServerContext& context,
                              const std::string& instancePublicId)
  {
    Json::Value instance;
    if (!context.GetIndex().LookupResource(instance, instancePublicId, ResourceType_Instance))
    {
      return false;
    }

    std::string filename = instance["MainDicomTags"]["SOPInstanceUID"].asString() + ".dcm";
    writer.OpenFile(filename.c_str());

    std::string dicom;
    context.ReadFile(dicom, instancePublicId, FileContentType_Dicom);
    writer.Write(dicom);

    return true;
  }

  static bool ArchiveInternal(HierarchicalZipWriter& writer,
                              ServerContext& context,
                              const std::string& publicId,
                              ResourceType resourceType,
                              bool isFirstLevel)
  {
    Json::Value resource;
    if (!context.GetIndex().LookupResource(resource, publicId, resourceType))
    {
      return false;
    }

    if (isFirstLevel && 
        !CreateRootDirectoryInArchive(writer, context, resource, resourceType))
    {
      return false;
    }

    writer.OpenDirectory(GetDirectoryNameInArchive(resource, resourceType).c_str());

    switch (resourceType)
    {
      case ResourceType_Patient:
        for (Json::Value::ArrayIndex i = 0; i < resource["Studies"].size(); i++)
        {
          std::string studyId = resource["Studies"][i].asString();
          if (!ArchiveInternal(writer, context, studyId, ResourceType_Study, false))
          {
            return false;
          }
        }
        break;

      case ResourceType_Study:
        for (Json::Value::ArrayIndex i = 0; i < resource["Series"].size(); i++)
        {
          std::string seriesId = resource["Series"][i].asString();
          if (!ArchiveInternal(writer, context, seriesId, ResourceType_Series, false))
          {
            return false;
          }
        }
        break;

      case ResourceType_Series:
        for (Json::Value::ArrayIndex i = 0; i < resource["Instances"].size(); i++)
        {
          if (!ArchiveInstance(writer, context, resource["Instances"][i].asString()))
          {
            return false;
          }
        }
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    writer.CloseDirectory();
    return true;
  }                                 

  template <enum ResourceType resourceType>
  static void GetArchive(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    // Create a RAII for the temporary file to manage the ZIP file
    Toolbox::TemporaryFile tmp;
    std::string id = call.GetUriComponent("id", "");

    {
      // Create a ZIP writer
      HierarchicalZipWriter writer(tmp.GetPath().c_str());

      // Store the requested resource into the ZIP
      if (!ArchiveInternal(writer, context, id, resourceType, true))
      {
        return;
      }
    }

    // Prepare the sending of the ZIP file
    FilesystemHttpSender sender(tmp.GetPath().c_str());
    sender.SetContentType("application/zip");
    sender.SetDownloadFilename(id + ".zip");

    // Send the ZIP
    call.GetOutput().AnswerFile(sender);

    // The temporary file is automatically removed thanks to the RAII
  }


  // Changes API --------------------------------------------------------------
 
  static void GetSinceAndLimit(int64_t& since,
                               unsigned int& limit,
                               bool& last,
                               const RestApi::GetCall& call)
  {
    static const unsigned int MAX_RESULTS = 100;
    
    if (call.HasArgument("last"))
    {
      last = true;
      return;
    }

    last = false;

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
  }

  static void GetChanges(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    //std::string filter = GetArgument(getArguments, "filter", "");
    int64_t since;
    unsigned int limit;
    bool last;
    GetSinceAndLimit(since, limit, last, call);

    Json::Value result;
    if ((!last && context.GetIndex().GetChanges(result, since, limit)) ||
        ( last && context.GetIndex().GetLastChange(result)))
    {
      call.GetOutput().AnswerJson(result);
    }
  }


  static void GetExports(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    int64_t since;
    unsigned int limit;
    bool last;
    GetSinceAndLimit(since, limit, last, call);

    Json::Value result;
    if ((!last && context.GetIndex().GetExportedResources(result, since, limit)) ||
        ( last && context.GetIndex().GetLastExportedResource(result)))
    {
      call.GetOutput().AnswerJson(result);
    }
  }

  
  // Get information about a single patient -----------------------------------
 
  static void IsProtectedPatient(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);
    std::string publicId = call.GetUriComponent("id", "");
    bool isProtected = context.GetIndex().IsProtectedPatient(publicId);
    call.GetOutput().AnswerBuffer(isProtected ? "1" : "0", "text/plain");
  }


  static void SetPatientProtection(RestApi::PutCall& call)
  {
    RETRIEVE_CONTEXT(call);
    std::string publicId = call.GetUriComponent("id", "");
    std::string s = Toolbox::StripSpaces(call.GetPutBody());

    if (s == "0")
    {
      context.GetIndex().SetProtectedPatient(publicId, false);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
    else if (s == "1")
    {
      context.GetIndex().SetProtectedPatient(publicId, true);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
    else
    {
      // Bad request
    }
  }


  // Get information about a single instance ----------------------------------
 
  static void GetInstanceFile(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    std::string publicId = call.GetUriComponent("id", "");
    context.AnswerFile(call.GetOutput(), publicId, FileContentType_Dicom);
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

    std::string publicId = call.GetUriComponent("id", "");
    std::string dicomContent, png;
    context.ReadFile(dicomContent, publicId, FileContentType_Dicom);

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


  // Upload of DICOM files through HTTP ---------------------------------------

  static void UploadDicomFile(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);

    const std::string& postData = call.GetPostBody();
    if (postData.size() == 0)
    {
      return;
    }

    LOG(INFO) << "Receiving a DICOM file of " << postData.size() << " bytes through HTTP";

    std::string publicId;
    StoreStatus status = context.Store(publicId, postData);
    Json::Value result = Json::objectValue;

    if (status != StoreStatus_Failure)
    {
      result["ID"] = publicId;
      result["Path"] = GetBasePath(ResourceType_Instance, publicId);
    }

    result["Status"] = ToString(status);
    call.GetOutput().AnswerJson(result);
  }



  // DICOM bridge -------------------------------------------------------------

  static bool IsExistingModality(const OrthancRestApi::Modalities& modalities,
                                 const std::string& id)
  {
    return modalities.find(id) != modalities.end();
  }

  static void ListModalities(RestApi::GetCall& call)
  {
    RETRIEVE_MODALITIES(call);

    Json::Value result = Json::arrayValue;
    for (OrthancRestApi::Modalities::const_iterator 
           it = modalities.begin(); it != modalities.end(); it++)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }


  static void ListModalityOperations(RestApi::GetCall& call)
  {
    RETRIEVE_MODALITIES(call);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingModality(modalities, id))
    {
      Json::Value result = Json::arrayValue;
      result.append("find-patient");
      result.append("find-study");
      result.append("find-series");
      result.append("find");
      result.append("store");
      call.GetOutput().AnswerJson(result);
    }
  }



  // Raw access to the DICOM tags of an instance ------------------------------

  static void GetRawContent(RestApi::GetCall& call)
  {
    // TODO IMPROVE MULTITHREADING
    static boost::mutex mutex_;
    boost::mutex::scoped_lock lock(mutex_);

    RETRIEVE_CONTEXT(call);
    std::string id = call.GetUriComponent("id", "");
    ParsedDicomFile& dicom = context.GetDicomFile(id);
    dicom.SendPathValue(call.GetOutput(), call.GetTrailingUri());
  }



  // Modification of DICOM instances ------------------------------------------

  static void ReplaceInstanceInternal(ParsedDicomFile& toModify,
                                      const Json::Value& removals,
                                      const Json::Value& replacements,
                                      DicomReplaceMode mode)
  {
    if (!replacements.isObject() ||
        !removals.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    for (Json::Value::ArrayIndex i = 0; i < removals.size(); i++)
    {
      DicomTag tag = FromDcmtkBridge::ParseTag(removals[i].asString());
      toModify.Remove(tag);
    }

    Json::Value::Members members = replacements.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      std::string value = replacements[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);      
      toModify.Replace(tag, value, mode);
    }

    // A new SOP instance UID is automatically generated
    std::string instanceUid = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Instance);
    toModify.Replace(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, DicomReplaceMode_InsertIfAbsent);
  }

 
  static bool ParseModifyRequest(Json::Value& removals,
                                 Json::Value& replacements,
                                 const RestApi::PostCall& call)
  {
    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      removals = Json::arrayValue;
      replacements = Json::objectValue;

      if (request.isMember("Remove"))
      {
        removals = request["Remove"];
      }

      if (request.isMember("Replace"))
      {
        replacements = request["Replace"];
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static void ModifyInstance(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);
    
    std::string id = call.GetUriComponent("id", "");
    ParsedDicomFile& dicom = context.GetDicomFile(id);
    
    Json::Value removals, replacements;
    if (ParseModifyRequest(removals, replacements, call))
    {
      std::auto_ptr<ParsedDicomFile> modified(dicom.Clone());
      ReplaceInstanceInternal(*modified, removals, replacements, DicomReplaceMode_InsertIfAbsent);
      context.GetIndex().SetMetadata(id, MetadataType_ModifiedFrom, id);
      modified->Answer(call.GetOutput());
    }
  }


  static void ModifySeriesInplace(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);
    
    typedef std::list<std::string> Instances;
    Instances instances;
    std::string id = call.GetUriComponent("id", "");
    context.GetIndex().GetChildInstances(instances, id);

    if (instances.size() == 0)
    {
      return;
    }

    Json::Value removals, replacements;
    if (ParseModifyRequest(removals, replacements, call))
    {
      std::string newSeriesId;
      replacements["0020-000e"] = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Series);

      for (Instances::const_iterator it = instances.begin(); 
           it != instances.end(); it++)
      {
        LOG(INFO) << "Modifying instance " << *it;
        ParsedDicomFile& dicom = context.GetDicomFile(*it);
        std::auto_ptr<ParsedDicomFile> modified(dicom.Clone());
        ReplaceInstanceInternal(*modified, removals, replacements, DicomReplaceMode_InsertIfAbsent);

        std::string modifiedInstance;
        if (context.Store(modifiedInstance, modified->GetDicom()) != StoreStatus_Success)
        {
          LOG(ERROR) << "Error while storing a modified instance " << *it;
          return;
        }

        if (newSeriesId.size() == 0 &&
            !context.GetIndex().LookupParent(newSeriesId, modifiedInstance))
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }

      assert(newSeriesId.size() != 0);
      Json::Value result = Json::objectValue;
      result["ID"] = newSeriesId;
      result["Path"] = GetBasePath(ResourceType_Series, newSeriesId);
      call.GetOutput().AnswerJson(result);
    }
  }


  static void ModifyStudyInplace(RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);
    
    typedef std::list<std::string> Instances;
    typedef std::map<std::string, std::string> SeriesUidMap;

    Instances instances;
    std::string id = call.GetUriComponent("id", "");
    context.GetIndex().GetChildInstances(instances, id);

    if (instances.size() == 0)
    {
      return;
    }

    SeriesUidMap seriesUidMap;

    Json::Value removals, replacements;
    if (ParseModifyRequest(removals, replacements, call))
    {
      std::string newStudyId;
      replacements["0020-000d"] = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Study);

      for (Instances::const_iterator it = instances.begin(); 
           it != instances.end(); it++)
      {
        LOG(INFO) << "Modifying instance " << *it;
        ParsedDicomFile& dicom = context.GetDicomFile(*it);

        std::string seriesId;
        if (!dicom.GetTagValue(seriesId, DICOM_TAG_SERIES_INSTANCE_UID))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        SeriesUidMap::const_iterator it2 = seriesUidMap.find(seriesId);
        if (it2 == seriesUidMap.end())
        {
          std::string newSeriesUid = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Series);
          seriesUidMap[seriesId] = newSeriesUid;
          replacements["0020-000e"] = newSeriesUid;
        }
        else
        {
          replacements["0020-000e"] = it2->second;
        }

        std::auto_ptr<ParsedDicomFile> modified(dicom.Clone());
        ReplaceInstanceInternal(*modified, removals, replacements, DicomReplaceMode_InsertIfAbsent);

        std::string modifiedInstance;
        if (context.Store(modifiedInstance, modified->GetDicom()) != StoreStatus_Success)
        {
          LOG(ERROR) << "Error while storing a modified instance " << *it;
          return;
        }

        if (newStudyId.size() == 0)
        {
          std::string newSeriesId;
          if (!context.GetIndex().LookupParent(newSeriesId, modifiedInstance) ||
              !context.GetIndex().LookupParent(newStudyId, newSeriesId))
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }
      }

      assert(newStudyId.size() != 0);
      Json::Value result = Json::objectValue;
      result["ID"] = newStudyId;
      result["Path"] = GetBasePath(ResourceType_Study, newStudyId);
      call.GetOutput().AnswerJson(result);
    }
  }




  // Registration of the various REST handlers --------------------------------

  OrthancRestApi::OrthancRestApi(ServerContext& context) : 
    context_(context)
  {
    GetListOfDicomModalities(modalities_);

    Register("/", ServeRoot);
    Register("/system", GetSystemInformation);
    Register("/statistics", GetStatistics);
    Register("/changes", GetChanges);
    Register("/exports", GetExports);

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

    Register("/patients/{id}/archive", GetArchive<ResourceType_Patient>);
    Register("/studies/{id}/archive", GetArchive<ResourceType_Study>);
    Register("/series/{id}/archive", GetArchive<ResourceType_Series>);

    Register("/patients/{id}/protected", IsProtectedPatient);
    Register("/patients/{id}/protected", SetPatientProtection);
    Register("/instances/{id}/file", GetInstanceFile);
    Register("/instances/{id}/tags", GetInstanceTags<false>);
    Register("/instances/{id}/simplified-tags", GetInstanceTags<true>);
    Register("/instances/{id}/frames", ListFrames);
    Register("/instances/{id}/content/*", GetRawContent);

    Register("/instances/{id}/frames/{frame}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/frames/{frame}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/frames/{frame}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/image-uint16", GetImage<ImageExtractionMode_UInt16>);

    Register("/modalities", ListModalities);
    Register("/modalities/{id}", ListModalityOperations);
    Register("/modalities/{id}/find-patient", DicomFindPatient);
    Register("/modalities/{id}/find-study", DicomFindStudy);
    Register("/modalities/{id}/find-series", DicomFindSeries);
    Register("/modalities/{id}/find", DicomFind);
    Register("/modalities/{id}/store", DicomStore);

    Register("/instances/{id}/modify", ModifyInstance);
    Register("/series/{id}/modify", ModifySeriesInplace);
    Register("/studies/{id}/modify", ModifyStudyInplace);
  }
}
