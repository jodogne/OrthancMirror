/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/filesystem.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <string.h>
#include <iostream>
#include <algorithm>

static OrthancPluginContext* context_ = NULL;
static std::string folder_;


/**
 * This is the main function for matching a DICOM worklist against a query.
 **/
static void  MatchWorklist(OrthancPluginWorklistAnswers*      answers,
                           const OrthancPluginWorklistQuery*  query,
                           const OrthancPlugins::FindMatcher& matcher,
                           const std::string& path)
{
  OrthancPlugins::MemoryBuffer dicom(context_);
  dicom.ReadFile(path);

  if (matcher.IsMatch(dicom))
  {
    // This DICOM file matches the worklist query, add it to the answers
    OrthancPluginErrorCode code = OrthancPluginWorklistAddAnswer
      (context_, answers, query, dicom.GetData(), dicom.GetSize());

    if (code != OrthancPluginErrorCode_Success)
    {
      OrthancPlugins::LogError(context_, "Error while adding an answer to a worklist request");
      ORTHANC_PLUGINS_THROW_EXCEPTION(code);
    }
  }
}


OrthancPlugins::FindMatcher* CreateMatcher(const OrthancPluginWorklistQuery* query,
                                           const char*                       remoteAet)
{
  OrthancPlugins::MemoryBuffer dicom(context_);
  dicom.GetDicomQuery(query);

  {
    Json::Value json;
    dicom.DicomToJson(json, OrthancPluginDicomToJsonFormat_Short, 
                      static_cast<OrthancPluginDicomToJsonFlags>(0), 0);

    OrthancPlugins::LogInfo(context_, "Received worklist query from remote modality " + 
                            std::string(remoteAet) + ":\n" + json.toStyledString());
  }

  return new OrthancPlugins::FindMatcher(context_, query);
  //return new OrthancPlugins::FindMatcher(context_, dicom);
}



OrthancPluginErrorCode Callback(OrthancPluginWorklistAnswers*     answers,
                                const OrthancPluginWorklistQuery* query,
                                const char*                       remoteAet,
                                const char*                       calledAet)
{
  try
  {
    // Construct an object to match the worklists in the database against the C-Find query
    std::auto_ptr<OrthancPlugins::FindMatcher> matcher(CreateMatcher(query, remoteAet));

    // Loop over the regular files in the database folder
    namespace fs = boost::filesystem;  

    fs::path source(folder_);
    fs::directory_iterator end;

    try
    {
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
            // We found a worklist (i.e. a DICOM find with extension ".wl"), match it against the query
            MatchWorklist(answers, query, *matcher, it->path().string());
          }
        }
      }
    }
    catch (fs::filesystem_error&)
    {
      OrthancPlugins::LogError(context_, "Inexistent folder while scanning for worklists: " + source.string());
      return OrthancPluginErrorCode_DirectoryExpected;
    }

    // Uncomment the following line if too many answers are to be returned
    // OrthancPluginMarkWorklistAnswersIncomplete(context_, answers);

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
    context_ = c;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    OrthancPlugins::LogWarning(context_, "Sample worklist plugin is initializing");
    OrthancPluginSetDescription(context_, "Serve DICOM modality worklists from a folder with Orthanc.");

    OrthancPlugins::OrthancConfiguration configuration(context_);

    OrthancPlugins::OrthancConfiguration worklists;
    configuration.GetSection(worklists, "Worklists");

    bool enabled = worklists.GetBooleanValue("Enable", false);
    if (enabled)
    {
      if (worklists.LookupStringValue(folder_, "Database"))
      {
        OrthancPlugins::LogWarning(context_, "The database of worklists will be read from folder: " + folder_);
        OrthancPluginRegisterWorklistCallback(context_, Callback);
      }
      else
      {
        OrthancPlugins::LogError(context_, "The configuration option \"Worklists.Database\" must contain a path");
        return -1;
      }
    }
    else
    {
      OrthancPlugins::LogWarning(context_, "Worklists server is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPluginLogWarning(context_, "Sample worklist plugin is finalizing");
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
