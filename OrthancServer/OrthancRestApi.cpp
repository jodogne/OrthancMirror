/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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
  // TODO IMPROVE MULTITHREADING
  // Every call to "ParsedDicomFile" must lock this mutex!!!
  static boost::mutex cacheMutex_;


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
    std::string stripped = Toolbox::StripSpaces(call.GetPostBody());

    Json::Value request;
    if (Toolbox::IsSHA1(stripped))
    {
      // This is for compatibility with Orthanc <= 0.5.1.
      request = stripped;
    }
    else if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      return;
    }

    std::list<std::string> instances;
    if (request.isString())
    {
      context.GetIndex().LogExportedResource(request.asString(), remote);
      context.GetIndex().GetChildInstances(instances, request.asString());
    }
    else if (request.isArray())
    {
      for (Json::Value::ArrayIndex i = 0; i < request.size(); i++)
      {
        if (!request[i].isString())
        {
          return;
        }

        std::string stripped = Toolbox::StripSpaces(request[i].asString());
        if (!Toolbox::IsSHA1(stripped))
        {
          return;
        }

        context.GetIndex().LogExportedResource(stripped, remote);
       
        std::list<std::string> tmp;
        context.GetIndex().GetChildInstances(tmp, stripped);
        instances.merge(tmp);
        assert(tmp.size() == 0);
      }
    }
    else
    {
      // Neither a string, nor a list of strings. Bad request.
      return;
    }

    DicomUserConnection connection;
    ConnectToModality(connection, remote);

    for (std::list<std::string>::const_iterator 
           it = instances.begin(); it != instances.end(); it++)
    {
      std::string dicom;
      context.ReadFile(dicom, *it, FileContentType_Dicom);
      connection.Store(dicom);
    }

    call.GetOutput().AnswerBuffer("{}", "application/json");
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

  static void GenerateUid(RestApi::GetCall& call)
  {
    std::string level = call.GetArgument("level", "");
    if (level == "patient")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Patient), "text/plain");
    }
    else if (level == "study")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Study), "text/plain");
    }
    else if (level == "series")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Series), "text/plain");
    }
    else if (level == "instance")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Instance), "text/plain");
    }
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
    boost::mutex::scoped_lock lock(cacheMutex_);

    RETRIEVE_CONTEXT(call);
    std::string id = call.GetUriComponent("id", "");
    ParsedDicomFile& dicom = context.GetDicomFile(id);
    dicom.SendPathValue(call.GetOutput(), call.GetTrailingUri());
  }



  // Modification of DICOM instances ------------------------------------------

  namespace
  {
    typedef std::set<DicomTag> Removals;
    typedef std::map<DicomTag, std::string> Replacements;
    typedef std::map< std::pair<DicomRootLevel, std::string>, std::string>  UidMap;
  }

  static void ReplaceInstanceInternal(ParsedDicomFile& toModify,
                                      const Removals& removals,
                                      const Replacements& replacements,
                                      DicomReplaceMode mode,
                                      bool removePrivateTags)
  {
    if (removePrivateTags)
    {
      toModify.RemovePrivateTags();
    }

    for (Removals::const_iterator it = removals.begin(); 
         it != removals.end(); it++)
    {
      toModify.Remove(*it);
    }

    for (Replacements::const_iterator it = replacements.begin(); 
         it != replacements.end(); it++)
    {
      toModify.Replace(it->first, it->second, mode);
    }

    // A new SOP instance UID is automatically generated
    std::string instanceUid = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Instance);
    toModify.Replace(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, DicomReplaceMode_InsertIfAbsent);
  }


  static void ParseRemovals(Removals& target,
                            const Json::Value& removals)
  {
    if (!removals.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    target.clear();

    for (Json::Value::ArrayIndex i = 0; i < removals.size(); i++)
    {
      std::string name = removals[i].asString();
      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      target.insert(tag);

      VLOG(1) << "Removal: " << name << " " << tag << std::endl;
    }
  }


  static void ParseReplacements(Replacements& target,
                                const Json::Value& replacements)
  {
    if (!replacements.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    target.clear();

    Json::Value::Members members = replacements.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      std::string value = replacements[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);      
      target[tag] = value;

      VLOG(1) << "Replacement: " << name << " " << tag << " == " << value << std::endl;
    }
  }


  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }


  static void SetupAnonymization(Removals& removals,
                                 Replacements& replacements)
  {
    // This is Table E.1-1 from PS 3.15-2008 - DICOM Part 15: Security and System Management Profiles
    removals.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID
    //removals.insert(DicomTag(0x0008, 0x0018)); // SOP Instance UID => set by ReplaceInstanceInternal()
    removals.insert(DicomTag(0x0008, 0x0050));  // Accession Number
    removals.insert(DicomTag(0x0008, 0x0080));  // Institution Name
    removals.insert(DicomTag(0x0008, 0x0081));  // Institution Address
    removals.insert(DicomTag(0x0008, 0x0090));  // Referring Physician's Name 
    removals.insert(DicomTag(0x0008, 0x0092));  // Referring Physician's Address 
    removals.insert(DicomTag(0x0008, 0x0094));  // Referring Physician's Telephone Numbers 
    removals.insert(DicomTag(0x0008, 0x1010));  // Station Name 
    removals.insert(DicomTag(0x0008, 0x1030));  // Study Description 
    removals.insert(DicomTag(0x0008, 0x103e));  // Series Description 
    removals.insert(DicomTag(0x0008, 0x1040));  // Institutional Department Name 
    removals.insert(DicomTag(0x0008, 0x1048));  // Physician(s) of Record 
    removals.insert(DicomTag(0x0008, 0x1050));  // Performing Physicians' Name 
    removals.insert(DicomTag(0x0008, 0x1060));  // Name of Physician(s) Reading Study 
    removals.insert(DicomTag(0x0008, 0x1070));  // Operators' Name 
    removals.insert(DicomTag(0x0008, 0x1080));  // Admitting Diagnoses Description 
    removals.insert(DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID 
    removals.insert(DicomTag(0x0008, 0x2111));  // Derivation Description 
    removals.insert(DicomTag(0x0010, 0x0010));  // Patient's Name 
    removals.insert(DicomTag(0x0010, 0x0020));  // Patient ID
    removals.insert(DicomTag(0x0010, 0x0030));  // Patient's Birth Date 
    removals.insert(DicomTag(0x0010, 0x0032));  // Patient's Birth Time 
    removals.insert(DicomTag(0x0010, 0x0040));  // Patient's Sex 
    removals.insert(DicomTag(0x0010, 0x1000));  // Other Patient Ids 
    removals.insert(DicomTag(0x0010, 0x1001));  // Other Patient Names 
    removals.insert(DicomTag(0x0010, 0x1010));  // Patient's Age 
    removals.insert(DicomTag(0x0010, 0x1020));  // Patient's Size 
    removals.insert(DicomTag(0x0010, 0x1030));  // Patient's Weight 
    removals.insert(DicomTag(0x0010, 0x1090));  // Medical Record Locator 
    removals.insert(DicomTag(0x0010, 0x2160));  // Ethnic Group 
    removals.insert(DicomTag(0x0010, 0x2180));  // Occupation 
    removals.insert(DicomTag(0x0010, 0x21b0));  // Additional Patient's History 
    removals.insert(DicomTag(0x0010, 0x4000));  // Patient Comments 
    removals.insert(DicomTag(0x0018, 0x1000));  // Device Serial Number 
    removals.insert(DicomTag(0x0018, 0x1030));  // Protocol Name 
    //removals.insert(DicomTag(0x0020, 0x000d));  // Study Instance UID => generated below
    //removals.insert(DicomTag(0x0020, 0x000e));  // Series Instance UID => generated below
    removals.insert(DicomTag(0x0020, 0x0010));  // Study ID 
    removals.insert(DicomTag(0x0020, 0x0052));  // Frame of Reference UID 
    removals.insert(DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID 
    removals.insert(DicomTag(0x0020, 0x4000));  // Image Comments 
    removals.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence 
    removals.insert(DicomTag(0x0040, 0xa124));  // UID
    removals.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    removals.insert(DicomTag(0x0088, 0x0140));  // Storage Media File-set UID 
    removals.insert(DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID 
    removals.insert(DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID 

    // Some more removals (from the experience of DICOM files at the CHU of Liege)
    removals.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician

    // Set the DeidentificationMethod tag
    replacements.insert(std::make_pair(DicomTag(0x0012, 0x0063), "Orthanc " ORTHANC_VERSION " - PS 3.15-2008 Table E.1-1"));

    // Set the PatientIdentityRemoved
    replacements.insert(std::make_pair(DicomTag(0x0012, 0x0062), "YES"));

    // Generate random study UID if not specified
    if (replacements.find(DICOM_TAG_STUDY_INSTANCE_UID) == replacements.end())
    {
      replacements.insert(std::make_pair(DICOM_TAG_STUDY_INSTANCE_UID, 
                                         FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Study)));
    }

    // Generate random series UID if not specified
    if (replacements.find(DICOM_TAG_SERIES_INSTANCE_UID) == replacements.end())
    {
      replacements.insert(std::make_pair(DICOM_TAG_SERIES_INSTANCE_UID, 
                                         FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Series)));
    }
  }


  static bool ParseModifyRequest(Removals& removals,
                                 Replacements& replacements,
                                 bool& removePrivateTags,
                                 const RestApi::PostCall& call)
  {
    removePrivateTags = false;
    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      Json::Value removalsPart = Json::arrayValue;
      Json::Value replacementsPart = Json::objectValue;

      if (request.isMember("Remove"))
      {
        removalsPart = request["Remove"];
      }

      if (request.isMember("Replace"))
      {
        replacementsPart = request["Replace"];
      }

      if (request.isMember("RemovePrivateTags"))
      {
        removePrivateTags = true;
      }
      
      ParseRemovals(removals, removalsPart);
      ParseReplacements(replacements, replacementsPart);

      return true;
    }
    else
    {
      return false;
    }
  }


  static bool ParseAnonymizationRequest(Removals& removals,
                                        Replacements& replacements,
                                        bool& removePrivateTags,
                                        RestApi::PostCall& call)
  {
    RETRIEVE_CONTEXT(call);

    removePrivateTags = true;

    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      Json::Value keepPart = Json::arrayValue;
      Json::Value removalsPart = Json::arrayValue;
      Json::Value replacementsPart = Json::objectValue;

      if (request.isMember("Keep"))
      {
        keepPart = request["Keep"];
      }

      if (request.isMember("KeepPrivateTags"))
      {
        removePrivateTags = false;
      }

      if (request.isMember("Replace"))
      {
        replacementsPart = request["Replace"];
      }

      Removals toKeep;
      ParseRemovals(toKeep, keepPart);

      SetupAnonymization(removals, replacements);

      for (Removals::iterator it = toKeep.begin(); it != toKeep.end(); it++)
      {
        removals.erase(*it);
      }

      Removals additionalRemovals;
      ParseRemovals(additionalRemovals, removalsPart);

      for (Removals::iterator it = additionalRemovals.begin(); 
           it != additionalRemovals.end(); it++)
      {
        removals.insert(*it);
      }     

      ParseReplacements(replacements, replacementsPart);

      // Generate random Patient's Name if none is specified
      if (replacements.find(DicomTag(0x0010, 0x0010)) == replacements.end())
      {
        replacements.insert(std::make_pair(DicomTag(0x0010, 0x0010), GeneratePatientName(context)));
      }

      // Generate random Patient's ID if none is specified
      if (replacements.find(DICOM_TAG_PATIENT_ID) == replacements.end())
      {
        replacements.insert(std::make_pair(DICOM_TAG_PATIENT_ID, 
                                           FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Patient)));
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static void AnonymizeOrModifyInstance(Removals& removals,
                                        Replacements& replacements,
                                        bool removePrivateTags,
                                        RestApi::PostCall& call)
  {
    boost::mutex::scoped_lock lock(cacheMutex_);
    RETRIEVE_CONTEXT(call);
    
    std::string id = call.GetUriComponent("id", "");
    ParsedDicomFile& dicom = context.GetDicomFile(id);
    
    std::auto_ptr<ParsedDicomFile> modified(dicom.Clone());
    ReplaceInstanceInternal(*modified, removals, replacements, DicomReplaceMode_InsertIfAbsent, removePrivateTags);
    modified->Answer(call.GetOutput());
  }


  static bool RetrieveMappedUid(ParsedDicomFile& dicom,
                                DicomRootLevel level,
                                Replacements& replacements,
                                UidMap& uidMap)
  {
    std::auto_ptr<DicomTag> tag;
    if (level == DicomRootLevel_Series)
    {
      tag.reset(new DicomTag(DICOM_TAG_SERIES_INSTANCE_UID));
    }
    else
    {
      assert(level == DicomRootLevel_Study);
      tag.reset(new DicomTag(DICOM_TAG_STUDY_INSTANCE_UID));
    }

    std::string original;
    if (!dicom.GetTagValue(original, *tag))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::string mapped;
    bool isNew;

    UidMap::const_iterator previous = uidMap.find(std::make_pair(level, original));
    if (previous == uidMap.end())
    {
      mapped = FromDcmtkBridge::GenerateUniqueIdentifier(level);
      uidMap.insert(std::make_pair(std::make_pair(level, original), mapped));
      isNew = true;
    }
    else
    {
      mapped = previous->second;
      isNew = false;
    }    

    replacements[*tag] = mapped;
    return isNew;
  }


  static void AnonymizeOrModifyResource(Removals& removals,
                                        Replacements& replacements,
                                        bool removePrivateTags,
                                        MetadataType metadataType,
                                        ChangeType changeType,
                                        ResourceType resourceType,
                                        RestApi::PostCall& call)
  {
    typedef std::list<std::string> Instances;

    bool isFirst = true;
    Json::Value result(Json::objectValue);

    boost::mutex::scoped_lock lock(cacheMutex_);
    RETRIEVE_CONTEXT(call);

    Instances instances;
    std::string id = call.GetUriComponent("id", "");
    context.GetIndex().GetChildInstances(instances, id);

    if (instances.size() == 0)
    {
      return;
    }


    /**
     * Loop over all the instances of the resource.
     **/

    UidMap uidMap;
    for (Instances::const_iterator it = instances.begin(); 
         it != instances.end(); it++)
    {
      LOG(INFO) << "Modifying instance " << *it;
      ParsedDicomFile& original = context.GetDicomFile(*it);

      bool isNewSeries = RetrieveMappedUid(original, DicomRootLevel_Series, replacements, uidMap);

      bool isNewStudy = false;
      if (resourceType == ResourceType_Study ||
          resourceType == ResourceType_Patient)
      {
        isNewStudy = RetrieveMappedUid(original, DicomRootLevel_Study, replacements, uidMap);
      }

      /**
       * Compute the resulting DICOM instance and store it into the Orthanc store.
       **/

      std::auto_ptr<ParsedDicomFile> modified(original.Clone());
      ReplaceInstanceInternal(*modified, removals, replacements, DicomReplaceMode_InsertIfAbsent, removePrivateTags);

      std::string modifiedInstance;
      if (context.Store(modifiedInstance, modified->GetDicom()) != StoreStatus_Success)
      {
        LOG(ERROR) << "Error while storing a modified instance " << *it;
        return;
      }


      /**
       * Record metadata information (AnonimizedFrom/ModifiedFrom).
       **/

      DicomInstanceHasher modifiedHasher = modified->GetHasher();
      DicomInstanceHasher originalHasher = original.GetHasher();

      if (isNewSeries)
      {
        context.GetIndex().SetMetadata(modifiedHasher.HashSeries(), 
                                       metadataType, originalHasher.HashSeries());
      }

      if (isNewStudy)
      {
        context.GetIndex().SetMetadata(modifiedHasher.HashStudy(), 
                                       metadataType, originalHasher.HashStudy());
      }

      assert(*it == originalHasher.HashInstance());
      assert(modifiedInstance == modifiedHasher.HashInstance());
      context.GetIndex().SetMetadata(modifiedInstance, metadataType, *it);


      /**
       * Compute the JSON object that is returned by the REST call.
       **/

      if (isFirst)
      {
        std::string newId;

        switch (resourceType)
        {
          case ResourceType_Series:
            newId = modifiedHasher.HashSeries();
            break;

          case ResourceType_Study:
            newId = modifiedHasher.HashStudy();
            break;

          case ResourceType_Patient:
            newId = modifiedHasher.HashPatient();
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        result["Type"] = ToString(resourceType);
        result["ID"] = newId;
        result["Path"] = GetBasePath(resourceType, newId);
        result["PatientID"] = modifiedHasher.HashPatient();
        isFirst = false;
      }
    }

    call.GetOutput().AnswerJson(result);
  }



  static void ModifyInstance(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyInstance(removals, replacements, removePrivateTags, call);
    }
  }


  static void AnonymizeInstance(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyInstance(removals, replacements, removePrivateTags, call);
    }
  }


  static void ModifySeriesInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_ModifiedFrom, ChangeType_ModifiedSeries, 
                                ResourceType_Series, call);
    }
  }


  static void AnonymizeSeriesInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedSeries, 
                                ResourceType_Series, call);
    }
  }


  static void ModifyStudyInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_ModifiedFrom, ChangeType_ModifiedStudy, 
                                ResourceType_Study, call);
    }
  }


  static void AnonymizeStudyInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedStudy, 
                                ResourceType_Study, call);
    }
  }


  static void ModifyPatientInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_ModifiedFrom, ChangeType_ModifiedPatient, 
                                ResourceType_Patient, call);
    }
  }


  static void AnonymizePatientInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, 
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedPatient, 
                                ResourceType_Patient, call);
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
    Register("/patients/{id}/modify", ModifyPatientInplace);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeSeriesInplace);
    Register("/studies/{id}/anonymize", AnonymizeStudyInplace);
    Register("/patients/{id}/anonymize", AnonymizePatientInplace);

    Register("/tools/generate-uid", GenerateUid);
  }
}
