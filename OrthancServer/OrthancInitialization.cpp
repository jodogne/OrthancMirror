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


#include "OrthancInitialization.h"

#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <curl/curl.h>
#include <boost/thread.hpp>
#include <glog/logging.h>

namespace Orthanc
{
  static const char* CONFIGURATION_FILE = "Configuration.json";

  static boost::mutex globalMutex_;
  static std::auto_ptr<Json::Value> configuration_;


  static void ReadGlobalConfiguration(const char* configurationFile)
  {
    configuration_.reset(new Json::Value);

    std::string content;

    if (configurationFile)
    {
      Toolbox::ReadFile(content, configurationFile);
      LOG(INFO) << "Using the configuration from: " << configurationFile;
    }
    else
    {
#if 0 && ORTHANC_STANDALONE == 1 && defined(__linux)
      // Unused anymore
      // Under Linux, try and open "../../etc/orthanc/Configuration.json"
      try
      {
        boost::filesystem::path p = Toolbox::GetDirectoryOfExecutable();
        p = p.parent_path().parent_path();
        p /= "etc";
        p /= "orthanc";
        p /= CONFIGURATION_FILE;
          
        Toolbox::ReadFile(content, p.string());
        LOG(INFO) << "Using the configuration from: " << p.string();
      }
      catch (OrthancException&)
      {
        // No configuration file found, give up with empty configuration
        LOG(INFO) << "Using the default Orthanc configuration";
        return;
      }

#elif ORTHANC_STANDALONE == 1
      // No default path for the standalone configuration
      LOG(INFO) << "Using the default Orthanc configuration";
      return;

#else
      // In a non-standalone build, we use the
      // "Resources/Configuration.json" from the Orthanc distribution
      try
      {
        boost::filesystem::path p = ORTHANC_PATH;
        p /= "Resources";
        p /= CONFIGURATION_FILE;
        Toolbox::ReadFile(content, p.string());
        LOG(INFO) << "Using the configuration from: " << p.string();
      }
      catch (OrthancException&)
      {
        // No configuration file found, give up with empty configuration
        LOG(INFO) << "Using the default Orthanc configuration";
        return;
      }
#endif
    }

    Json::Reader reader;
    if (!reader.parse(content, *configuration_))
    {
      throw OrthancException("Unable to read the configuration file");
    }
  }


  void OrthancInitialize(const char* configurationFile)
  {
    boost::mutex::scoped_lock lock(globalMutex_);
    ReadGlobalConfiguration(configurationFile);
    curl_global_init(CURL_GLOBAL_ALL);
  }



  void OrthancFinalize()
  {
    boost::mutex::scoped_lock lock(globalMutex_);
    curl_global_cleanup();
    configuration_.reset(NULL);
  }



  std::string GetGlobalStringParameter(const std::string& parameter,
                                       const std::string& defaultValue)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (configuration_->isMember(parameter))
    {
      return (*configuration_) [parameter].asString();
    }
    else
    {
      return defaultValue;
    }
  }


  int GetGlobalIntegerParameter(const std::string& parameter,
                                int defaultValue)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (configuration_->isMember(parameter))
    {
      return (*configuration_) [parameter].asInt();
    }
    else
    {
      return defaultValue;
    }
  }

  bool GetGlobalBoolParameter(const std::string& parameter,
                              bool defaultValue)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (configuration_->isMember(parameter))
    {
      return (*configuration_) [parameter].asBool();
    }
    else
    {
      return defaultValue;
    }
  }




  void GetDicomModality(const std::string& name,
                        std::string& aet,
                        std::string& address,
                        int& port)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (!configuration_->isMember("DicomModalities"))
    {
      throw OrthancException("");
    }

    const Json::Value& modalities = (*configuration_) ["DicomModalities"];
    if (modalities.type() != Json::objectValue ||
        !modalities.isMember(name))
    {
      throw OrthancException("");
    }

    try
    {
      aet = modalities[name].get(0u, "").asString();
      address = modalities[name].get(1u, "").asString();
      port = modalities[name].get(2u, "").asInt();
    }
    catch (...)
    {
      throw OrthancException("Badly formatted DICOM modality");
    }
  }



  void GetListOfDicomModalities(std::set<std::string>& target)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    target.clear();
  
    if (!configuration_->isMember("DicomModalities"))
    {
      return;
    }

    const Json::Value& modalities = (*configuration_) ["DicomModalities"];
    if (modalities.type() != Json::objectValue)
    {
      throw OrthancException("Badly formatted list of DICOM modalities");
    }

    Json::Value::Members members = modalities.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      for (size_t j = 0; j < members[i].size(); j++)
      {
        if (!isalnum(members[i][j]) && members[i][j] != '-')
        {
          throw OrthancException("Only alphanumeric and dash characters are allowed in the names of the modalities");
        }
      }

      target.insert(members[i]);
    }
  }



  void SetupRegisteredUsers(MongooseServer& httpServer)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    httpServer.ClearUsers();

    if (!configuration_->isMember("RegisteredUsers"))
    {
      return;
    }

    const Json::Value& users = (*configuration_) ["RegisteredUsers"];
    if (users.type() != Json::objectValue)
    {
      throw OrthancException("Badly formatted list of users");
    }

    Json::Value::Members usernames = users.getMemberNames();
    for (size_t i = 0; i < usernames.size(); i++)
    {
      const std::string& username = usernames[i];
      std::string password = users[username].asString();
      httpServer.RegisterUser(username.c_str(), password.c_str());
    }
  }
}
