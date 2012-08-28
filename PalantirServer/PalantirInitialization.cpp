/**
 * Palantir - A Lightweight, RESTful DICOM Store
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


#include "PalantirInitialization.h"

#include "../Core/PalantirException.h"
#include "../Core/Toolbox.h"

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <curl/curl.h>
#include <boost/thread.hpp>

namespace Palantir
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
    }
    else
    {
      try
      {
#if PALANTIR_STANDALONE == 0
        boost::filesystem::path p = PALANTIR_PATH;
        p /= "Resources";
        p /= CONFIGURATION_FILE;
        Toolbox::ReadFile(content, p.string());
#else
        Toolbox::ReadFile(content, CONFIGURATION_FILE);
#endif
      }
      catch (PalantirException&)
      {
        // No configuration file found, give up with empty configuration
        return;
      }
    }

    Json::Reader reader;
    if (!reader.parse(content, *configuration_))
    {
      throw PalantirException("Unable to read the configuration file");
    }
  }


  void PalantirInitialize(const char* configurationFile)
  {
    boost::mutex::scoped_lock lock(globalMutex_);
    ReadGlobalConfiguration(configurationFile);
    curl_global_init(CURL_GLOBAL_ALL);
  }



  void PalantirFinalize()
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
      throw PalantirException("");
    }

    const Json::Value& modalities = (*configuration_) ["DicomModalities"];
    if (modalities.type() != Json::objectValue ||
        !modalities.isMember(name))
    {
      throw PalantirException("");
    }

    try
    {
      aet = modalities[name].get(0u, "").asString();
      address = modalities[name].get(1u, "").asString();
      port = modalities[name].get(2u, "").asInt();
    }
    catch (...)
    {
      throw PalantirException("Badly formatted DICOM modality");
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
      throw PalantirException("Badly formatted list of DICOM modalities");
    }

    Json::Value::Members members = modalities.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      for (size_t j = 0; j < members[i].size(); j++)
      {
        if (!isalnum(members[i][j]) && members[i][j] != '-')
        {
          throw PalantirException("Only alphanumeric and dash characters are allowed in the names of the modalities");
        }
      }

      target.insert(members[i]);
    }
  }
}
