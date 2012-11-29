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

#include "OrthancInitialization.h"
#include "FromDcmtkBridge.h"
#include "ServerToolbox.h"
#include "../Core/Uuid.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  static void SendJson(HttpOutput& output,
                       const Json::Value& value)
  {
    Json::StyledWriter writer;
    std::string s = writer.write(value);
    output.AnswerBufferWithContentType(s, "application/json");
  }

  void OrthancRestApi::ConnectToModality(DicomUserConnection& c,
                                           const std::string& name)
  {
    std::string aet, address;
    int port;
    GetDicomModality(name, aet, address, port);
    c.SetLocalApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));
    c.SetDistantApplicationEntityTitle(aet);
    c.SetDistantHost(address);
    c.SetDistantPort(port);
    c.Open();
  }

  bool OrthancRestApi::MergeQueryAndTemplate(DicomMap& result,
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

  bool OrthancRestApi::DicomFindPatient(Json::Value& result,
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

  bool OrthancRestApi::DicomFindStudy(Json::Value& result,
                                        DicomUserConnection& c,
                                        const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindStudyTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    if (m.GetValue(DICOM_TAG_ACCESSION_NUMBER).AsString().size() <= 2 &&
        m.GetValue(DICOM_TAG_PATIENT_ID).AsString().size() <= 2)
    {
      return false;
    }        
        
    DicomFindAnswers answers;
    c.FindStudy(answers, m);
    answers.ToJson(result);
    return true;
  }

  bool OrthancRestApi::DicomFindSeries(Json::Value& result,
                                         DicomUserConnection& c,
                                         const std::string& postData)
  {
    DicomMap m;
    DicomMap::SetupFindSeriesTemplate(m);
    if (!MergeQueryAndTemplate(m, postData))
    {
      return false;
    }

    if ((m.GetValue(DICOM_TAG_ACCESSION_NUMBER).AsString().size() <= 2 &&
         m.GetValue(DICOM_TAG_PATIENT_ID).AsString().size() <= 2) ||
        m.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).AsString().size() <= 2)
    {
      return false;
    }        
        
    DicomFindAnswers answers;
    c.FindSeries(answers, m);
    answers.ToJson(result);
    return true;
  }

  bool OrthancRestApi::DicomFind(Json::Value& result,
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
      m.CopyTagIfExists(patients.GetAnswer(i), DICOM_TAG_PATIENT_ID);

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
        m.CopyTagIfExists(studies.GetAnswer(j), DICOM_TAG_PATIENT_ID);
        m.CopyTagIfExists(studies.GetAnswer(j), DICOM_TAG_STUDY_INSTANCE_UID);

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



  bool OrthancRestApi::DicomStore(Json::Value& result,
                                    DicomUserConnection& c,
                                    const std::string& postData)
  {
    Json::Value found(Json::objectValue);

    if (!Toolbox::IsUuid(postData))
    {
      // This is not a UUID, assume this is a DICOM instance
      c.Store(postData);
    }
    else if (index_.LookupResource(found, postData, ResourceType_Series))
    {
      // The UUID corresponds to a series
      for (Json::Value::ArrayIndex i = 0; i < found["Instances"].size(); i++)
      {
        std::string uuid = found["Instances"][i].asString();
        Json::Value instance(Json::objectValue);
        if (index_.LookupResource(instance, uuid, ResourceType_Instance))
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
    else if (index_.LookupResource(found, postData, ResourceType_Instance))
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


  OrthancRestApi::OrthancRestApi(ServerIndex& index,
                                     const std::string& path) :
    index_(index),
    storage_(path)
  {
    GetListOfDicomModalities(modalities_);
  }


  void OrthancRestApi::Handle(
    HttpOutput& output,
    const std::string& method,
    const UriComponents& uri,
    const Arguments& headers,
    const Arguments& getArguments,
    const std::string& postData)
  {
    bool existingResource = false;
    Json::Value result(Json::objectValue);


    // DICOM bridge -------------------------------------------------------------

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
            output.SendHeader(Orthanc_HttpStatus_400_BadRequest);
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
      output.SendHeader(Orthanc_HttpStatus_404_NotFound);
    }
  }
}
