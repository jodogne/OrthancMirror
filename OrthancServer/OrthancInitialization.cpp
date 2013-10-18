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

#include "../Core/HttpClient.h"
#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "ServerEnumerations.h"

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
  static boost::filesystem::path defaultDirectory_;


  static void ReadGlobalConfiguration(const char* configurationFile)
  {
    configuration_.reset(new Json::Value);

    std::string content;

    if (configurationFile)
    {
      Toolbox::ReadFile(content, configurationFile);
      defaultDirectory_ = boost::filesystem::path(configurationFile).parent_path();
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


  static void RegisterUserMetadata()
  {
    if (configuration_->isMember("UserMetadata"))
    {
      const Json::Value& parameter = (*configuration_) ["UserMetadata"];

      Json::Value::Members members = parameter.getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        std::string info = "\"" + members[i] + "\" = " + parameter[members[i]].toStyledString();
        LOG(INFO) << "Registering user-defined metadata: " << info;

        if (!parameter[members[i]].asBool())
        {
          LOG(ERROR) << "Not a number in this user-defined metadata: " << info;
          throw OrthancException(ErrorCode_BadParameterType);
        }

        int metadata = parameter[members[i]].asInt();

        try
        {
          RegisterUserMetadata(metadata, members[i]);
        }
        catch (OrthancException e)
        {
          LOG(ERROR) << "Cannot register this user-defined metadata: " << info;
          throw e;
        }
      }
    }
  }


  void OrthancInitialize(const char* configurationFile)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    InitializeServerEnumerations();
    defaultDirectory_ = boost::filesystem::current_path();
    ReadGlobalConfiguration(configurationFile);

    HttpClient::GlobalInitialize();

    RegisterUserMetadata();
  }



  void OrthancFinalize()
  {
    boost::mutex::scoped_lock lock(globalMutex_);
    HttpClient::GlobalFinalize();
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
                        int& port,
                        ModalityManufacturer& manufacturer)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (!configuration_->isMember("DicomModalities"))
    {
      throw OrthancException("");
    }

    const Json::Value& modalities = (*configuration_) ["DicomModalities"];
    if (modalities.type() != Json::objectValue ||
        !modalities.isMember(name) ||
        (modalities[name].size() != 3 && modalities[name].size() != 4))
    {
      throw OrthancException("");
    }

    try
    {
      aet = modalities[name].get(0u, "").asString();
      address = modalities[name].get(1u, "").asString();
      port = modalities[name].get(2u, "").asInt();

      if (modalities[name].size() == 4)
      {
        manufacturer = StringToModalityManufacturer(modalities[name].get(3u, "").asString());
      }
      else
      {
        manufacturer = ModalityManufacturer_Generic;
      }
    }
    catch (...)
    {
      throw OrthancException("Badly formatted DICOM modality");
    }
  }



  void GetOrthancPeer(const std::string& name,
                      std::string& url,
                      std::string& username,
                      std::string& password)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    if (!configuration_->isMember("OrthancPeers"))
    {
      throw OrthancException("");
    }

    const Json::Value& modalities = (*configuration_) ["OrthancPeers"];
    if (modalities.type() != Json::objectValue ||
        !modalities.isMember(name))
    {
      throw OrthancException("");
    }

    try
    {
      url = modalities[name].get(0u, "").asString();

      if (modalities[name].size() == 1)
      {
        username = "";
        password = "";
      }
      else if (modalities[name].size() == 3)
      {
        username = modalities[name].get(1u, "").asString();
        password = modalities[name].get(2u, "").asString();
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
    catch (...)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    if (url.size() != 0 && url[url.size() - 1] != '/')
    {
      url += '/';
    }
  }


  static bool ReadKeys(std::set<std::string>& target,
                       const char* parameter,
                       bool onlyAlphanumeric)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    target.clear();
  
    if (!configuration_->isMember(parameter))
    {
      return true;
    }

    const Json::Value& modalities = (*configuration_) [parameter];
    if (modalities.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value::Members members = modalities.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      if (onlyAlphanumeric)
      {
        for (size_t j = 0; j < members[i].size(); j++)
        {
          if (!isalnum(members[i][j]) && members[i][j] != '-')
          {
            return false;
          }
        }
      }

      target.insert(members[i]);
    }

    return true;
  }


  void GetListOfDicomModalities(std::set<std::string>& target)
  {
    if (!ReadKeys(target, "DicomModalities", true))
    {
      throw OrthancException("Only alphanumeric and dash characters are allowed in the names of the modalities");
    }
  }


  void GetListOfOrthancPeers(std::set<std::string>& target)
  {
    if (!ReadKeys(target, "OrthancPeers", true))
    {
      throw OrthancException("Only alphanumeric and dash characters are allowed in the names of Orthanc peers");
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


  std::string InterpretRelativePath(const std::string& baseDirectory,
                                    const std::string& relativePath)
  {
    boost::filesystem::path base(baseDirectory);
    boost::filesystem::path relative(relativePath);

    /**
       The following lines should be equivalent to this one: 

       return (base / relative).string();

       However, for some unknown reason, some versions of Boost do not
       make the proper path resolution when "baseDirectory" is an
       absolute path. So, a hack is used below.
     **/

    if (relative.is_absolute())
    {
      return relative.string();
    }
    else
    {
      return (base / relative).string();
    }
  }

  std::string InterpretStringParameterAsPath(const std::string& parameter)
  {
    boost::mutex::scoped_lock lock(globalMutex_);
    return InterpretRelativePath(defaultDirectory_.string(), parameter);
  }


  void GetGlobalListOfStringsParameter(std::list<std::string>& target,
                                       const std::string& key)
  {
    boost::mutex::scoped_lock lock(globalMutex_);

    target.clear();
  
    if (!configuration_->isMember(key))
    {
      return;
    }

    const Json::Value& lst = (*configuration_) [key];

    if (lst.type() != Json::arrayValue)
    {
      throw OrthancException("Badly formatted list of strings");
    }

    for (Json::Value::ArrayIndex i = 0; i < lst.size(); i++)
    {
      target.push_back(lst[i].asString());
    }    
  }


  void ConnectToModalityUsingSymbolicName(DicomUserConnection& connection,
                                          const std::string& name)
  {
    std::string aet, address;
    int port;
    ModalityManufacturer manufacturer;
    GetDicomModality(name, aet, address, port, manufacturer);

    LOG(WARNING) << "Connecting to remote DICOM modality: AET=" << aet << ", address=" << address << ", port=" << port;

    connection.SetLocalApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));
    connection.SetDistantApplicationEntityTitle(aet);
    connection.SetDistantHost(address);
    connection.SetDistantPort(port);
    connection.SetDistantManufacturer(manufacturer);
    connection.Open();
  }


  void ConnectToModalityUsingAETitle(DicomUserConnection& connection,
                                     const std::string& aet)
  {
    std::set<std::string> modalities;
    GetListOfDicomModalities(modalities);

    std::string address;
    int port;
    ModalityManufacturer manufacturer;
    bool found = false;

    for (std::set<std::string>::const_iterator 
           it = modalities.begin(); it != modalities.end(); it++)
    {
      try
      {
        std::string thisAet;
        GetDicomModality(*it, thisAet, address, port, manufacturer);
        
        if (aet == thisAet)
        {
          found = true;
          break;
        }
      }
      catch (OrthancException&)
      {
      }
    }

    if (!found)
    {
      throw OrthancException("Unknown modality: " + aet);
    }

    LOG(WARNING) << "Connecting to remote DICOM modality: AET=" << aet << ", address=" << address << ", port=" << port;

    connection.SetLocalApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));
    connection.SetDistantApplicationEntityTitle(aet);
    connection.SetDistantHost(address);
    connection.SetDistantPort(port);
    connection.SetDistantManufacturer(manufacturer);
    connection.Open();
  }
}
