/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../OrthancInitialization.h"
#include "../../Core/HttpClient.h"
#include "../FromDcmtkBridge.h"
#include "../Scheduler/ServerJob.h"
#include "../Scheduler/StoreScuCommand.h"
#include "../Scheduler/StorePeerCommand.h"

#include <glog/logging.h>

namespace Orthanc
{
  // DICOM SCU ----------------------------------------------------------------

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


  static void DicomEcho(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    try
    {
      if (locker.GetConnection().Echo())
      {
        call.GetOutput().AnswerBuffer("{}", "application/json");
      }
    }
    catch (OrthancException&)
    {
    }
  }


  static void DicomFindPatient(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }

    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    DicomFindAnswers answers;
    locker.GetConnection().FindPatient(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindStudy(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

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
      
    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    DicomFindAnswers answers;
    locker.GetConnection().FindStudy(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindSeries(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

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
         
    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    DicomFindAnswers answers;
    locker.GetConnection().FindSeries(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindInstance(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap m;
    DicomMap::SetupFindInstanceTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }

    if ((m.GetValue(DICOM_TAG_ACCESSION_NUMBER).AsString().size() <= 2 &&
         m.GetValue(DICOM_TAG_PATIENT_ID).AsString().size() <= 2) ||
        m.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).AsString().size() <= 2 ||
        m.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).AsString().size() <= 2)
    {
      return;
    }        
         
    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    DicomFindAnswers answers;
    locker.GetConnection().FindInstance(answers, m);

    Json::Value result;
    answers.ToJson(result);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFind(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetPostBody()))
    {
      return;
    }
 
    RemoteModalityParameters remote = Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    ReusableDicomUserConnection::Locker locker(context.GetReusableDicomUserConnection(), remote);

    DicomFindAnswers patients;
    locker.GetConnection().FindPatient(patients, m);

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
      locker.GetConnection().FindStudy(studies, m);

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
        locker.GetConnection().FindSeries(series, m);

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


  static bool GetInstancesToExport(std::list<std::string>& instances,
                                   const std::string& remote,
                                   RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

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
      return false;
    }

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
          return false;
        }

        std::string stripped = Toolbox::StripSpaces(request[i].asString());
        if (!Toolbox::IsSHA1(stripped))
        {
          return false;
        }

        context.GetIndex().LogExportedResource(stripped, remote);
       
        std::list<std::string> tmp;
        context.GetIndex().GetChildInstances(tmp, stripped);

        for (std::list<std::string>::const_iterator
               it = tmp.begin(); it != tmp.end(); ++it)
        {
          instances.push_back(*it);
        }
      }
    }
    else
    {
      // Neither a string, nor a list of strings. Bad request.
      return false;
    }

    return true;
  }


  static void DicomStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    std::list<std::string> instances;
    if (!GetInstancesToExport(instances, remote, call))
    {
      return;
    }

    RemoteModalityParameters p = Configuration::GetModalityUsingSymbolicName(remote);

    ServerJob job;
    for (std::list<std::string>::const_iterator 
           it = instances.begin(); it != instances.end(); ++it)
    {
      job.AddCommand(new StoreScuCommand(context, p)).AddInput(*it);
    }

    job.SetDescription("HTTP request: Store-SCU to peer \"" + remote + "\"");
    context.GetScheduler().SubmitAndWait(job);

    call.GetOutput().AnswerBuffer("{}", "application/json");
  }


  // Orthanc Peers ------------------------------------------------------------

  static bool IsExistingPeer(const OrthancRestApi::SetOfStrings& peers,
                             const std::string& id)
  {
    return peers.find(id) != peers.end();
  }

  static void ListPeers(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings peers;
    Configuration::GetListOfOrthancPeers(peers);

    Json::Value result = Json::arrayValue;
    for (OrthancRestApi::SetOfStrings::const_iterator 
           it = peers.begin(); it != peers.end(); ++it)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }

  static void ListPeerOperations(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings peers;
    Configuration::GetListOfOrthancPeers(peers);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingPeer(peers, id))
    {
      RestApi::AutoListChildren(call);
    }
  }

  static void PeerStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    std::list<std::string> instances;
    if (!GetInstancesToExport(instances, remote, call))
    {
      return;
    }

    OrthancPeerParameters peer;
    Configuration::GetOrthancPeer(peer, remote);

    ServerJob job;
    for (std::list<std::string>::const_iterator 
           it = instances.begin(); it != instances.end(); ++it)
    {
      job.AddCommand(new StorePeerCommand(context, peer)).AddInput(*it);
    }

    job.SetDescription("HTTP request: POST to peer \"" + remote + "\"");
    context.GetScheduler().SubmitAndWait(job);

    call.GetOutput().AnswerBuffer("{}", "application/json");
  }


  // DICOM bridge -------------------------------------------------------------

  static bool IsExistingModality(const OrthancRestApi::SetOfStrings& modalities,
                                 const std::string& id)
  {
    return modalities.find(id) != modalities.end();
  }

  static void ListModalities(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings modalities;
    Configuration::GetListOfDicomModalities(modalities);

    Json::Value result = Json::arrayValue;
    for (OrthancRestApi::SetOfStrings::const_iterator 
           it = modalities.begin(); it != modalities.end(); ++it)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }


  static void ListModalityOperations(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings modalities;
    Configuration::GetListOfDicomModalities(modalities);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingModality(modalities, id))
    {
      RestApi::AutoListChildren(call);
    }
  }


  static void UpdateModality(RestApiPutCall& call)
  {
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(call.GetPutBody(), json))
    {
      RemoteModalityParameters modality;
      modality.FromJson(json);
      Configuration::UpdateModality(call.GetUriComponent("id", ""), modality);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
  }


  static void DeleteModality(RestApiDeleteCall& call)
  {
    Configuration::RemoveModality(call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", "text/plain");
  }


  static void UpdatePeer(RestApiPutCall& call)
  {
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(call.GetPutBody(), json))
    {
      OrthancPeerParameters peer;
      peer.FromJson(json);
      Configuration::UpdatePeer(call.GetUriComponent("id", ""), peer);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
  }


  static void DeletePeer(RestApiDeleteCall& call)
  {
    Configuration::RemovePeer(call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", "text/plain");
  }


  void OrthancRestApi::RegisterModalities()
  {
    Register("/modalities", ListModalities);
    Register("/modalities/{id}", ListModalityOperations);
    Register("/modalities/{id}", UpdateModality);
    Register("/modalities/{id}", DeleteModality);
    Register("/modalities/{id}/echo", DicomEcho);
    Register("/modalities/{id}/find-patient", DicomFindPatient);
    Register("/modalities/{id}/find-study", DicomFindStudy);
    Register("/modalities/{id}/find-series", DicomFindSeries);
    Register("/modalities/{id}/find-instance", DicomFindInstance);
    Register("/modalities/{id}/find", DicomFind);
    Register("/modalities/{id}/store", DicomStore);

    Register("/peers", ListPeers);
    Register("/peers/{id}", ListPeerOperations);
    Register("/peers/{id}", UpdatePeer);
    Register("/peers/{id}", DeletePeer);
    Register("/peers/{id}/store", PeerStore);
  }
}
