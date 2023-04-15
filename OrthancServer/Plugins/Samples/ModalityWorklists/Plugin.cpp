/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/filesystem.hpp>
#include <json/value.h>
#include <string.h>
#include <iostream>
#include <algorithm>

static std::string folder_;
static bool filterIssuerAet_ = false;
static unsigned int limitAnswers_ = 0;

/**
 * This is the main function for matching a DICOM worklist against a query.
 **/
static bool MatchWorklist(OrthancPluginWorklistAnswers*      answers,
                           const OrthancPluginWorklistQuery*  query,
                           const OrthancPlugins::FindMatcher& matcher,
                           const std::string& path)
{
  OrthancPlugins::MemoryBuffer dicom;
  dicom.ReadFile(path);

  if (matcher.IsMatch(dicom))
  {
    // This DICOM file matches the worklist query, add it to the answers
    OrthancPluginErrorCode code = OrthancPluginWorklistAddAnswer
      (OrthancPlugins::GetGlobalContext(), answers, query, dicom.GetData(), dicom.GetSize());

    if (code != OrthancPluginErrorCode_Success)
    {
      OrthancPlugins::LogError("Error while adding an answer to a worklist request");
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(code);
    }

    return true;
  }

  return false;
}


static OrthancPlugins::FindMatcher* CreateMatcher(const OrthancPluginWorklistQuery* query,
                                                  const char*                       issuerAet)
{
  // Extract the DICOM instance underlying the C-Find query
  OrthancPlugins::MemoryBuffer dicom;
  dicom.GetDicomQuery(query);

  // Convert the DICOM as JSON, and dump it to the user in "--verbose" mode
  Json::Value json;
  dicom.DicomToJson(json, OrthancPluginDicomToJsonFormat_Short,
                    static_cast<OrthancPluginDicomToJsonFlags>(0), 0);

  OrthancPlugins::LogInfo("Received worklist query from remote modality " +
                          std::string(issuerAet) + ":\n" + json.toStyledString());

  if (!filterIssuerAet_)
  {
    return new OrthancPlugins::FindMatcher(query);
  }
  else
  {
    // Alternative sample showing how to fine-tune an incoming C-Find
    // request, before matching it against the worklist database. The
    // code below will restrict the original DICOM request by
    // requesting the ScheduledStationAETitle to correspond to the AET
    // of the C-Find issuer. This code will make the integration test
    // "test_filter_issuer_aet" succeed (cf. the orthanc-tests repository).

    static const char* SCHEDULED_PROCEDURE_STEP_SEQUENCE = "0040,0100";
    static const char* SCHEDULED_STATION_AETITLE = "0040,0001";
    static const char* PREGNANCY_STATUS = "0010,21c0";

    if (!json.isMember(SCHEDULED_PROCEDURE_STEP_SEQUENCE))
    {
      // Create a ScheduledProcedureStepSequence sequence, with one empty element
      json[SCHEDULED_PROCEDURE_STEP_SEQUENCE] = Json::arrayValue;
      json[SCHEDULED_PROCEDURE_STEP_SEQUENCE].append(Json::objectValue);
    }

    Json::Value& v = json[SCHEDULED_PROCEDURE_STEP_SEQUENCE];

    if (v.type() != Json::arrayValue ||
        v.size() != 1 ||
        v[0].type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    // Set the ScheduledStationAETitle if none was provided
    if (!v[0].isMember(SCHEDULED_STATION_AETITLE) ||
        v[0].type() != Json::stringValue ||
        v[0][SCHEDULED_STATION_AETITLE].asString().size() == 0 ||
        v[0][SCHEDULED_STATION_AETITLE].asString() == "*")
    {
      v[0][SCHEDULED_STATION_AETITLE] = issuerAet;
    }

    if (json.isMember(PREGNANCY_STATUS) &&
        json[PREGNANCY_STATUS].asString().size() == 0)
    {
      json.removeMember(PREGNANCY_STATUS);
    }

    // Encode the modified JSON as a DICOM instance, then convert it to a C-Find matcher
    OrthancPlugins::MemoryBuffer modified;
    modified.CreateDicom(json, OrthancPluginCreateDicomFlags_None);
    
    return new OrthancPlugins::FindMatcher(modified);
  }
}



OrthancPluginErrorCode Callback(OrthancPluginWorklistAnswers*     answers,
                                const OrthancPluginWorklistQuery* query,
                                const char*                       issuerAet,
                                const char*                       calledAet)
{
  try
  {
    // Construct an object to match the worklists in the database against the C-Find query
    std::unique_ptr<OrthancPlugins::FindMatcher> matcher(CreateMatcher(query, issuerAet));

    // Loop over the regular files in the database folder
    namespace fs = boost::filesystem;

    fs::path source(folder_);
    fs::directory_iterator end;

    try
    {
      unsigned int parsedFilesCount = 0;
      unsigned int matchedWorklistCount = 0;
      
      for (fs::directory_iterator it(source); it != end; ++it)
      {
        fs::file_type type(it->status().type());

        if (type == fs::regular_file ||
            type == fs::reparse_file)   // cf. BitBucket issue #11
        {
          std::string extension = fs::extension(it->path());
          std::transform(extension.begin(), extension.end(), extension.begin(), tolower);  // Convert to lowercase

          if (extension == ".wl")
          {
            parsedFilesCount++;
            // We found a worklist (i.e. a DICOM find with extension ".wl"), match it against the query
            if (MatchWorklist(answers, query, *matcher, it->path().string()))
            {
              if (limitAnswers_ != 0 &&
                  matchedWorklistCount >= limitAnswers_)
              {
                // Too many answers are to be returned wrt. the
                // "LimitAnswers" configuration parameter. Mark the
                // C-FIND result as incomplete.
                OrthancPluginWorklistMarkIncomplete(OrthancPlugins::GetGlobalContext(), answers);
                return OrthancPluginErrorCode_Success;
              }
              
              OrthancPlugins::LogInfo("Worklist matched: " + it->path().string());
              matchedWorklistCount++;
            }
          }
        }
      }

      std::ostringstream message;
      message << "Worklist C-Find: parsed " << parsedFilesCount
              << " files, found " << matchedWorklistCount << " match(es)";
      OrthancPlugins::LogInfo(message.str());
    }
    catch (fs::filesystem_error&)
    {
      OrthancPlugins::LogError("Inexistent folder while scanning for worklists: " + source.string());
      return OrthancPluginErrorCode_DirectoryExpected;
    }

    return OrthancPluginErrorCode_Success;
  }
  catch (OrthancPlugins::PluginException& e)
  {
    return e.GetErrorCode();
  }
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPlugins::LogWarning("Sample worklist plugin is initializing");
    OrthancPluginSetDescription(c, "Serve DICOM modality worklists from a folder with Orthanc.");

    OrthancPlugins::OrthancConfiguration configuration;

    OrthancPlugins::OrthancConfiguration worklists;
    configuration.GetSection(worklists, "Worklists");

    bool enabled = worklists.GetBooleanValue("Enable", false);
    if (enabled)
    {
      if (worklists.LookupStringValue(folder_, "Database"))
      {
        OrthancPlugins::LogWarning("The database of worklists will be read from folder: " + folder_);
        OrthancPluginRegisterWorklistCallback(OrthancPlugins::GetGlobalContext(), Callback);
      }
      else
      {
        OrthancPlugins::LogError("The configuration option \"Worklists.Database\" must contain a path");
        return -1;
      }

      filterIssuerAet_ = worklists.GetBooleanValue("FilterIssuerAet", false);
      limitAnswers_ = worklists.GetUnsignedIntegerValue("LimitAnswers", 0);
    }
    else
    {
      OrthancPlugins::LogWarning("Worklist server is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("Sample worklist plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "worklists";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return MODALITY_WORKLISTS_VERSION;
  }
}
