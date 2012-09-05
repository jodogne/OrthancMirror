/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "PalanthirRestApi.h"

#include "PalanthirInitialization.h"
#include "FromDcmtkBridge.h"
#include "../Core/Uuid.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/lexical_cast.hpp>

namespace Palanthir
{
  static void SendJson(HttpOutput& output,
                       const Json::Value& value)
  {
    Json::StyledWriter writer;
    std::string s = writer.write(value);
    output.AnswerBufferWithContentType(s, "application/json");
  }


  static void SimplifyTagsRecursion(Json::Value& target,
                                    const Json::Value& source)
  {
    assert(source.isObject());

    target = Json::objectValue;
    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& v = source[members[i]];
      const std::string& name = v["Name"].asString();
      const std::string& type = v["Type"].asString();

      if (type == "String")
      {
        target[name] = v["Value"].asString();
      }
      else if (type == "TooLong" ||
               type == "Null")
      {
        target[name] = Json::nullValue;
      }
      else if (type == "Sequence")
      {
        const Json::Value& array = v["Value"];
        assert(array.isArray());

        Json::Value children = Json::arrayValue;
        for (size_t i = 0; i < array.size(); i++)
        {
          Json::Value c;
          SimplifyTagsRecursion(c, array[i]);
          children.append(c);
        }

        target[name] = children;
      }
      else
      {
        assert(0);
      }
    }
  }


  static void SimplifyTags(Json::Value& target,
                           const FileStorage& storage,
                           const std::string& fileUuid)
  {
    std::string s;
    storage.ReadFile(s, fileUuid);

    Json::Value source;
    Json::Reader reader;
    if (!reader.parse(s, source))
    {
      throw PalanthirException("Corrupted JSON file");
    }

    SimplifyTagsRecursion(target, source);
  }


  bool PalanthirRestApi::Store(Json::Value& result,
                               const std::string& postData)
  {
    // Prepare an input stream for the memory buffer
    DcmInputBufferStream is;
    if (postData.size() > 0)
    {
      is.setBuffer(&postData[0], postData.size());
    }
    is.setEos();

    //printf("[%d]\n", postData.size());

    DcmFileFormat dicomFile;
    if (dicomFile.read(is).good())
    {
      DicomMap dicomSummary;
      FromDcmtkBridge::Convert(dicomSummary, *dicomFile.getDataset());
          
      Json::Value dicomJson;
      FromDcmtkBridge::ToJson(dicomJson, *dicomFile.getDataset());
      
      std::string instanceUuid;
      StoreStatus status = StoreStatus_Failure;
      if (postData.size() > 0)
      {
        status = index_.Store
          (instanceUuid, storage_, reinterpret_cast<const char*>(&postData[0]),
           postData.size(), dicomSummary, dicomJson, "");
      }

      switch (status)
      {
      case StoreStatus_Success:
        result["ID"] = instanceUuid;
        result["Path"] = "/instances/" + instanceUuid;
        result["Status"] = "Success";
        return true;
      
      case StoreStatus_AlreadyStored:
        result["ID"] = instanceUuid;
        result["Path"] = "/instances/" + instanceUuid;
        result["Status"] = "AlreadyStored";
        return true;

      default:
        return false;
      }
    }

    return false;
  }

  void PalanthirRestApi::ConnectToModality(DicomUserConnection& c,
                                           const std::string& name)
  {
    std::string aet, address;
    int port;
    GetDicomModality(name, aet, address, port);
    c.SetLocalApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "PALANTHIR"));
    c.SetDistantApplicationEntityTitle(aet);
    c.SetDistantHost(address);
    c.SetDistantPort(port);
    c.Open();
  }

  bool PalanthirRestApi::MergeQueryAndTemplate(DicomMap& result,
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
      DicomTag t = FromDcmtkBridge::FindTag(members[i]);
      result.SetValue(t, query[members[i]].asString());
    }

    return true;
  }

  bool PalanthirRestApi::DicomFindPatient(Json::Value& result,
                                          DicomUserConnection& c,
                                          const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    DicomFindAnswers answers;
    c.FindPatient(answers, m);
    answers.ToJson(result);
    return true;
  }

  bool PalanthirRestApi::DicomFindStudy(Json::Value& result,
                                        DicomUserConnection& c,
                                        const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindStudyTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    if (m.GetValue(DicomTag::ACCESSION_NUMBER).AsString().size() <= 2 &&
        m.GetValue(DicomTag::PATIENT_ID).AsString().size() <= 2)
    {
      return false;
    }        
        
    DicomFindAnswers answers;
    c.FindStudy(answers, m);
    answers.ToJson(result);
    return true;
  }

  bool PalanthirRestApi::DicomFindSeries(Json::Value& result,
                                         DicomUserConnection& c,
                                         const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindSeriesTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    if ((m.GetValue(DicomTag::ACCESSION_NUMBER).AsString().size() <= 2 &&
         m.GetValue(DicomTag::PATIENT_ID).AsString().size() <= 2) ||
        m.GetValue(DicomTag::STUDY_UID).AsString().size() <= 2)
    {
      return false;
    }        
        
    DicomFindAnswers answers;
    c.FindSeries(answers, m);
    answers.ToJson(result);
    return true;
  }

  bool PalanthirRestApi::DicomFind(Json::Value& result,
                                   DicomUserConnection& c,
                                   const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    DicomFindAnswers patients;
    c.FindPatient(patients, m);

    // Loop over the found patients
    result = Json::arrayValue;
    for (size_t i = 0; i < patients.GetSize(); i++)
    {
      Json::Value patient(Json::objectValue);
      FromDcmtkBridge::ToJson(patient, patients.GetAnswer(i));

      DicomMap::SetupFindStudyTemplate(m);
      if (!MergeQueryAndTemplate(m, postData))
      {
        return false;
      }
      m.CopyTagIfExists(patients.GetAnswer(i), DicomTag::PATIENT_ID);

      DicomFindAnswers studies;
      c.FindStudy(studies, m);

      patient["Studies"] = Json::arrayValue;
      
      // Loop over the found studies
      for (size_t j = 0; j < studies.GetSize(); j++)
      {
        Json::Value study(Json::objectValue);
        FromDcmtkBridge::ToJson(study, studies.GetAnswer(j));

        DicomMap::SetupFindSeriesTemplate(m);
        if (!MergeQueryAndTemplate(m, postData))
        {
          return false;
        }
        m.CopyTagIfExists(studies.GetAnswer(j), DicomTag::PATIENT_ID);
        m.CopyTagIfExists(studies.GetAnswer(j), DicomTag::STUDY_UID);

        DicomFindAnswers series;
        c.FindSeries(series, m);

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
    
    return true;
  }



  bool PalanthirRestApi::DicomStore(Json::Value& result,
                                    DicomUserConnection& c,
                                    const std::string& postData)
  {
    Json::Value found(Json::objectValue);

    if (!Toolbox::IsUuid(postData))
    {
      // This is not a UUID, assume this is a DICOM instance
      c.Store(postData);
    }
    else if (index_.GetSeries(found, postData))
    {
      // The UUID corresponds to a series
      for (size_t i = 0; i < found["Instances"].size(); i++)
      {
        std::string uuid = found["Instances"][i].asString();
        Json::Value instance(Json::objectValue);
        if (index_.GetInstance(instance, uuid))
        {
          std::string content;
          storage_.ReadFile(content, instance["FileUuid"].asString());
          c.Store(content);
        }
        else
        {
          return false;
        }
      }
    }
    else if (index_.GetInstance(found, postData))
    {
      // The UUID corresponds to an instance
      std::string content;
      storage_.ReadFile(content, found["FileUuid"].asString());
      c.Store(content);
    }
    else
    {
      return false;
    }

    return true;
  }


  PalanthirRestApi::PalanthirRestApi(ServerIndex& index,
                                     const std::string& path) :
    index_(index),
    storage_(path)
  {
    GetListOfDicomModalities(modalities_);
  }


  void PalanthirRestApi::Handle(
    HttpOutput& output,
    const std::string& method,
    const UriComponents& uri,
    const Arguments& headers,
    const Arguments& arguments,
    const std::string& postData)
  {
    if (uri.size() == 0)
    {
      if (method == "GET")
      {
        output.Redirect("/app/explorer.html");
      }
      else
      {
        output.SendMethodNotAllowedError("GET");
      }

      return;
    }

    bool existingResource = false;
    Json::Value result(Json::objectValue);


    // List all the instances ---------------------------------------------------
 
    if (uri.size() == 1 && uri[0] == "instances")
    {
      if (method == "GET")
      {
        result = Json::Value(Json::arrayValue);
        index_.GetAllUuids(result, "Instances");
        existingResource = true;
      }
      else if (method == "POST")
      {
        // Add a new instance to the storage
        if (Store(result, postData))
        {
          SendJson(output, result);
          return;
        }
        else
        {
          output.SendHeader(Palanthir_HttpStatus_415_UnsupportedMediaType);
          return;
        }
      }
      else
      {
        output.SendMethodNotAllowedError("GET,POST");
        return;
      }
    }


    // List all the patients, studies or series ---------------------------------
 
    if (uri.size() == 1 && 
        (uri[0] == "series" ||
         uri[0] == "studies" ||
         uri[0] == "patients"))
    {
      if (method == "GET")
      {
        result = Json::Value(Json::arrayValue);

        if (uri[0] == "instances")
          index_.GetAllUuids(result, "Instances");
        else if (uri[0] == "series")
          index_.GetAllUuids(result, "Series");
        else if (uri[0] == "studies")
          index_.GetAllUuids(result, "Studies");
        else if (uri[0] == "patients")
          index_.GetAllUuids(result, "Patients");

        existingResource = true;
      }
      else
      {
        output.SendMethodNotAllowedError("GET");
        return;
      }
    }


    // Information about a single object ----------------------------------------
 
    else if (uri.size() == 2 && 
             (uri[0] == "instances" ||
              uri[0] == "series" ||
              uri[0] == "studies" ||
              uri[0] == "patients"))
    {
      if (method == "GET")
      {
        if (uri[0] == "patients")
        {
          existingResource = index_.GetPatient(result, uri[1]);
        }
        else if (uri[0] == "studies")
        {
          existingResource = index_.GetStudy(result, uri[1]);
        }
        else if (uri[0] == "series")
        {
          existingResource = index_.GetSeries(result, uri[1]);
        }
        else if (uri[0] == "instances")
        {
          existingResource = index_.GetInstance(result, uri[1]);
        }
      }
      else if (method == "DELETE")
      {
        if (uri[0] == "patients")
        {
          existingResource = index_.DeletePatient(result, uri[1]);
        }
        else if (uri[0] == "studies")
        {
          existingResource = index_.DeleteStudy(result, uri[1]);
        }
        else if (uri[0] == "series")
        {
          existingResource = index_.DeleteSeries(result, uri[1]);
        }
        else if (uri[0] == "instances")
        {
          existingResource = index_.DeleteInstance(result, uri[1]);
        }

        if (existingResource)
        {
          result["Status"] = "Success";
        }
      }
      else
      {
        output.SendMethodNotAllowedError("GET,DELETE");
        return;
      }
    }


    // Get the DICOM or the JSON file of one instance ---------------------------
 
    else if (uri.size() == 3 &&
             uri[0] == "instances" &&
             (uri[2] == "file" || 
              uri[2] == "tags" || 
              uri[2] == "simplified-tags"))
    {
      std::string fileUuid, contentType;
      if (uri[2] == "file")
      {
        existingResource = index_.GetDicomFile(fileUuid, uri[1]);
        contentType = "application/dicom";
      }
      else if (uri[2] == "tags" ||
               uri[2] == "simplified-tags")
      {
        existingResource = index_.GetJsonFile(fileUuid, uri[1]);
        contentType = "application/json";
      }

      if (existingResource)
      {
        if (uri[2] == "simplified-tags")
        {
          Json::Value v;
          SimplifyTags(v, storage_, fileUuid);
          SendJson(output, v);
          return;
        }
        else
        {
          output.AnswerFile(storage_, fileUuid, contentType);
          return;
        }
      }
    }


    else if (uri.size() == 3 &&
             uri[0] == "instances" &&
             (uri[2] == "preview" ||
              uri[2] == "image-uint8" ||
              uri[2] == "image-uint16"))
    {
      std::string uuid;
      existingResource = index_.GetDicomFile(uuid, uri[1]);

      if (existingResource)
      {
        std::string dicomContent, png;
        storage_.ReadFile(dicomContent, uuid);
        try
        {
          if (uri[2] == "preview")
          {
            FromDcmtkBridge::ExtractPngImage(png, dicomContent, ImageExtractionMode_Preview);
          }
          else if (uri[2] == "image-uint8")
          {
            FromDcmtkBridge::ExtractPngImage(png, dicomContent, ImageExtractionMode_UInt8);
          }
          else if (uri[2] == "image-uint16")
          {
            FromDcmtkBridge::ExtractPngImage(png, dicomContent, ImageExtractionMode_UInt16);
          }
          else
          {
            throw PalanthirException(ErrorCode_InternalError);
          }

          output.AnswerBufferWithContentType(png, "image/png");
          return;
        }
        catch (PalanthirException&)
        {
          output.Redirect("/app/images/Unsupported.png");
          return;
        }
      }
    }



    // Changes API --------------------------------------------------------------
 
    if (uri.size() == 1 && uri[0] == "changes")
    {
      if (method == "GET")
      {
        const static unsigned int MAX_RESULTS = 100;

        std::string filter = GetArgument(arguments, "filter", "");
        int64_t since;
        unsigned int limit;
        try
        {
          since = boost::lexical_cast<int64_t>(GetArgument(arguments, "since", "0"));
          limit = boost::lexical_cast<unsigned int>(GetArgument(arguments, "limit", "0"));
        }
        catch (boost::bad_lexical_cast)
        {
          output.SendHeader(Palanthir_HttpStatus_400_BadRequest);
          return;
        }

        if (limit == 0 || limit > MAX_RESULTS)
        {
          limit = MAX_RESULTS;
        }

        if (!index_.GetChanges(result, since, filter, limit))
        {
          output.SendHeader(Palanthir_HttpStatus_400_BadRequest);
          return;
        }

        existingResource = true;
      }
      else
      {
        output.SendMethodNotAllowedError("GET");
        return;
      }
    }


    // DICOM bridge -------------------------------------------------------------

    if (uri.size() == 1 &&
        uri[0] == "modalities")
    {
      if (method == "GET")
      {
        result = Json::Value(Json::arrayValue);
        existingResource = true;

        for (Modalities::const_iterator it = modalities_.begin(); 
             it != modalities_.end(); it++)
        {
          result.append(*it);
        }
      }
      else
      {
        output.SendMethodNotAllowedError("GET");
        return;
      }
    }

    if ((uri.size() == 2 ||
         uri.size() == 3) && 
        uri[0] == "modalities")
    {
      if (modalities_.find(uri[1]) == modalities_.end())
      {
        // Unknown modality
      }
      else if (uri.size() == 2)
      {
        if (method != "GET")
        {
          output.SendMethodNotAllowedError("POST");
          return;
        }
        else
        {
          existingResource = true;
          result = Json::arrayValue;
          result.append("find-patient");
          result.append("find-study");
          result.append("find-series");
          result.append("find");
          result.append("store");
        }
      }
      else if (uri.size() == 3)
      {
        if (uri[2] != "find-patient" &&
            uri[2] != "find-study" &&
            uri[2] != "find-series" &&
            uri[2] != "find" &&
            uri[2] != "store")
        {
          // Unknown request
        }
        else if (method != "POST")
        {
          output.SendMethodNotAllowedError("POST");
          return;
        }
        else
        {
          DicomUserConnection connection;
          ConnectToModality(connection, uri[1]);
          existingResource = true;
          
          if ((uri[2] == "find-patient" && !DicomFindPatient(result, connection, postData)) ||
              (uri[2] == "find-study" && !DicomFindStudy(result, connection, postData)) ||
              (uri[2] == "find-series" && !DicomFindSeries(result, connection, postData)) ||
              (uri[2] == "find" && !DicomFind(result, connection, postData)) ||
              (uri[2] == "store" && !DicomStore(result, connection, postData)))
          {
            output.SendHeader(Palanthir_HttpStatus_400_BadRequest);
            return;
          }
        }
      }
    }

 
    if (existingResource)
    {
      SendJson(output, result);
    }
    else
    {
      output.SendHeader(Palanthir_HttpStatus_404_NotFound);
    }
  }
}
