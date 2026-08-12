/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "OrthancConfiguration.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpServer.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../OrthancFramework/Sources/TemporaryFile.h"
#include "../../OrthancFramework/Sources/Toolbox.h"

#include "ServerIndex.h"

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>


static const char* const DICOM_MODALITIES = "DicomModalities";
static const char* const DICOM_MODALITIES_IN_DB = "DicomModalitiesInDatabase";
static const char* const ORTHANC_PEERS = "OrthancPeers";
static const char* const ORTHANC_PEERS_IN_DB = "OrthancPeersInDatabase";
static const char* const TEMPORARY_DIRECTORY = "TemporaryDirectory";
static const char* const WARNINGS = "Warnings";
static const char* const JOBS_ENGINE_THREADS_COUNT = "JobsEngineThreadsCount";
static const char* const DICOM_LOSSY_TRANSCODING_QUALITY = "DicomLossyTranscodingQuality";
static const char* const CONFIG_LOADER_THREADS = "LoaderThreads";
static const char* const CONFIG_ZIP_LOADER_THREADS = "ZipLoaderThreads"; // for backward compatibility only

namespace Orthanc
{
  static void ReadConfigurationFromString(Json::Value& target,
                                          const std::string& content)
  {
    std::map<std::string, std::string> env;
    SystemToolbox::GetEnvironmentVariables(env);

    std::string substituted = Toolbox::SubstituteVariables(content, env);

    Json::Value tmp;
    if (!Toolbox::ReadJson(tmp, substituted) ||
        tmp.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadJson, "The configuration file does not follow the JSON syntax");
    }

    Toolbox::CopyJsonWithoutComments(target, tmp);
  }


  static void ReadConfigurationFromFile(Json::Value& target,
                                        const boost::filesystem::path& path)
  {
    LOG(WARNING) << "Reading the configuration file: " << SystemToolbox::PathToUtf8(path);

    std::string content;
    SystemToolbox::ReadFile(content, path);

    ReadConfigurationFromString(target, content);
  }


  static void MergeConfigurations(Json::Value& target,
                                  const Json::Value& source)
  {
    if (target.type() != Json::objectValue ||
        source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      Json::Value::Members members = source.getMemberNames();
      for (Json::Value::ArrayIndex i = 0; i < members.size(); i++)
      {
        if (target.isMember(members[i]))
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "The configuration section \"" + members[i] +
                                 "\" is defined in 2 different configuration files");
        }
        else
        {
          target[members[i]] = source[members[i]];
        }
      }
    }
  }


  static void ReadDefaultConfiguration(Json::Value& target)
  {
#if ORTHANC_STANDALONE == 1
    {
      std::string content;
      GetFileResource(content, ServerResources::CONFIGURATION_SAMPLE);
      ReadConfigurationFromString(target, content);
    }

    {
      // Merge with the content of the advanced configuration
      std::string content;
      GetFileResource(content, ServerResources::ADVANCED_CONFIGURATION_SAMPLE);

      Json::Value advanced;
      ReadConfigurationFromString(advanced, content);

      MergeConfigurations(target, advanced);
    }

#else
    // In a non-standalone build, we use the
    // "Resources/Configuration.json" from the Orthanc source code

    {
      boost::filesystem::path p = ORTHANC_PATH;
      p /= "Resources";
      p /= "Configuration.json";

      ReadConfigurationFromFile(target, p);
    }

    {
      // Merge with the content of the advanced configuration
      boost::filesystem::path p = ORTHANC_PATH;
      p /= "Resources";
      p /= "AdvancedConfiguration.json";

      Json::Value advanced;
      ReadConfigurationFromFile(advanced, p);

      MergeConfigurations(target, advanced);
    }
#endif
  }


  static void ReadConfigurationsFromFolder(Json::Value& target,
                                           const boost::filesystem::path& folder)
  {
    LOG(WARNING) << "Scanning folder for configuration files: " << Orthanc::SystemToolbox::PathToUtf8(folder);

    boost::filesystem::directory_iterator end_it; // default construction yields past-the-end

    for (boost::filesystem::directory_iterator it(folder); it != end_it; ++it)
    {
      if (!is_directory(it->status()))
      {
        std::string extension = it->path().extension().string();
        Toolbox::ToLowerCase(extension);

        if (extension == ".json")
        {
          Json::Value config;
          ReadConfigurationFromFile(config, it->path());
          MergeConfigurations(target, config);
        }
      }
    }
  }


  static void ReadConfiguration(Json::Value& target,
                                const std::list<boost::filesystem::path>& configurationPaths)
  {
    target = Json::objectValue;

    for (std::list<boost::filesystem::path>::const_iterator it = configurationPaths.begin();
         it != configurationPaths.end(); ++it)
    {
      if (boost::filesystem::is_directory(*it))
      {
        ReadConfigurationsFromFolder(target, *it);
      }
      else
      {
        Json::Value config;
        ReadConfigurationFromFile(config, *it);
        MergeConfigurations(target, config);
      }
    }
  }


  static void CheckAlphanumeric(const std::string& s)
  {
    for (size_t j = 0; j < s.size(); j++)
    {
      if (!isalnum(s[j]) && 
          s[j] != '-' && s[j] != '_')
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Only alphanumeric, dash characters and underscores are allowed "
                               "in the names of modalities/peers, but found: " + s);
      }
    }
  }


  OrthancConfiguration::OrthancConfiguration() :
    serverIndex_(NULL)
  {
    ReadDefaultConfiguration(defaultConfiguration_);
  }


  void OrthancConfiguration::LoadModalitiesFromJson(const Json::Value& source)
  {
    modalities_.clear();

    if (source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Bad format of the \"" + std::string(DICOM_MODALITIES) +
                             "\" configuration section");
    }

    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      CheckAlphanumeric(name);

      RemoteModalityParameters modality;
      modality.Unserialize(source[name]);
      modalities_[name] = modality;
    }
  }


  void OrthancConfiguration::LoadPeersFromJson(const Json::Value& source)
  {
    peers_.clear();

    if (source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Bad format of the \"" + std::string(ORTHANC_PEERS) +
                             "\" configuration section");
    }

    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      CheckAlphanumeric(name);

      WebServiceParameters peer;
      peer.Unserialize(source[name]);
      peers_[name] = peer;
    }
  }


  void OrthancConfiguration::LoadModalities()
  {
    if (GetBooleanParameter(DICOM_MODALITIES_IN_DB))
    {
      // Modalities are stored in the database
      if (serverIndex_ == NULL)
      {
        throw Orthanc::OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        std::string property = serverIndex_->GetGlobalProperty(GlobalProperty_Modalities, false /* not shared */, "{}");

        Json::Value modalities;
        if (Toolbox::ReadJson(modalities, property))
        {
          LoadModalitiesFromJson(modalities);
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot unserialize the list of modalities from the Orthanc database");
        }
      }
    }
    else
    {
      // Modalities are stored in the configuration files
      if (userConfiguration_.isMember(DICOM_MODALITIES))
      {
        LoadModalitiesFromJson(userConfiguration_[DICOM_MODALITIES]);
      }
      else
      {
        modalities_.clear();
      }
    }
  }

  void OrthancConfiguration::LoadJobsEngineThreadsCount()
  {
    // default values
    jobsEngineThreadsCount_["ResourceModification"] = 1;

    if (userConfiguration_.isMember(JOBS_ENGINE_THREADS_COUNT))
    {
      const Json::Value& source = userConfiguration_[JOBS_ENGINE_THREADS_COUNT];
      if (source.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Bad format of the \"" + std::string(JOBS_ENGINE_THREADS_COUNT) +
                               "\" configuration section");
      }

      Json::Value::Members members = source.getMemberNames();

      for (size_t i = 0; i < members.size(); i++)
      {
        const std::string& name = members[i];
        if (!source[name].isUInt())
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Bad format for \"" + std::string(JOBS_ENGINE_THREADS_COUNT) + "." + name + 
                                 "\".  It should be an unsigned integer");
        }
        jobsEngineThreadsCount_[name] = source[name].asUInt();
      }      
    }
  }

  unsigned int OrthancConfiguration::GetJobsEngineWorkersThread(const std::string& jobType) const
  {
    unsigned int workersThread = 1;
    
    const JobsEngineThreadsCount::const_iterator it = jobsEngineThreadsCount_.find(jobType);
    if (it != jobsEngineThreadsCount_.end())
    {
      workersThread = it->second;
    }

    if (workersThread == 0)
    {
      workersThread = SystemToolbox::GetHardwareConcurrency();
    }

    return workersThread;
  }

  void OrthancConfiguration::LoadOrthancAET()
  {
    std::string dicomAet = GetStringParameter(ORTHANC_CONFIG_DICOM_AET);
    
    if (!Toolbox::IsValidAet(dicomAet))
    {
      throw OrthancException(ErrorCode_BadFileFormat, std::string("The ") + ORTHANC_CONFIG_DICOM_AET + " contains characters that are not valid for an AET");
    }

    orthancDicomAet_ = Toolbox::NormalizeAet(dicomAet);
  }

  std::string OrthancConfiguration::GetOrthancAET() const
  {
    if (orthancDicomAet_.empty())
    {
      throw Orthanc::OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    return orthancDicomAet_;    
  }

  void OrthancConfiguration::LoadPeers()
  {
    if (GetBooleanParameter(ORTHANC_PEERS_IN_DB))
    {
      // Peers are stored in the database
      if (serverIndex_ == NULL)
      {
        throw Orthanc::OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        std::string property = serverIndex_->GetGlobalProperty(GlobalProperty_Peers, false /* not shared */, "{}");

        Json::Value peers;
        if (Toolbox::ReadJson(peers, property))
        {
          LoadPeersFromJson(peers);
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot unserialize the list of peers from the Orthanc database");
        }
      }
    }
    else
    {
      // Peers are stored in the configuration files
      if (userConfiguration_.isMember(ORTHANC_PEERS))
      {
        LoadPeersFromJson(userConfiguration_[ORTHANC_PEERS]);
      }
      else
      {
        peers_.clear();
      }
    }
  }


  void OrthancConfiguration::SaveModalitiesToJson(Json::Value& target)
  {
    target = Json::objectValue;

    for (Modalities::const_iterator it = modalities_.begin(); it != modalities_.end(); ++it)
    {
      Json::Value modality;
      it->second.Serialize(modality, true /* force advanced format */);

      target[it->first] = modality;
    }
  }

    
  void OrthancConfiguration::SavePeersToJson(Json::Value& target)
  {
    target = Json::objectValue;

    for (Peers::const_iterator it = peers_.begin(); it != peers_.end(); ++it)
    {
      Json::Value peer;
      it->second.Serialize(peer, 
                           false /* use simple format if possible */, 
                           true  /* include passwords */);

      target[it->first] = peer;
    }
  }  
    
    
  void OrthancConfiguration::SaveModalities()
  {
    if (GetBooleanParameter(DICOM_MODALITIES_IN_DB))
    {
      // Modalities are stored in the database
      if (serverIndex_ == NULL)
      {
        throw Orthanc::OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        Json::Value modalities;
        SaveModalitiesToJson(modalities);

        std::string s;
        Toolbox::WriteFastJson(s, modalities);
        
        serverIndex_->SetGlobalProperty(GlobalProperty_Modalities, false /* not shared */, s);
      }
    }
    else
    {
      // Modalities are stored in the configuration files
      if (!modalities_.empty() ||
          userConfiguration_.isMember(DICOM_MODALITIES))
      {
        SaveModalitiesToJson(userConfiguration_[DICOM_MODALITIES]);
      }
    }
  }


  void OrthancConfiguration::SavePeers()
  {
    if (GetBooleanParameter(ORTHANC_PEERS_IN_DB))
    {
      // Peers are stored in the database
      if (serverIndex_ == NULL)
      {
        throw Orthanc::OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        Json::Value peers;
        SavePeersToJson(peers);

        std::string s;
        Toolbox::WriteFastJson(s, peers);

        serverIndex_->SetGlobalProperty(GlobalProperty_Peers, false /* not shared */, s);
      }
    }
    else
    {
      // Peers are stored in the configuration files
      if (!peers_.empty() ||
          userConfiguration_.isMember(ORTHANC_PEERS))
      {
        SavePeersToJson(userConfiguration_[ORTHANC_PEERS]);
      }
    }
  }


  OrthancConfiguration& OrthancConfiguration::GetInstance()
  {
    static OrthancConfiguration configuration;
    return configuration;
  }


  static bool LookupStringParameterInternal(std::string& target,
                                            const Json::Value& config,
                                            const std::string& parameter)
  {
    if (config.isMember(parameter))
    {
      if (config[parameter].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter + "\" must be a string");
      }
      else
      {
        target = config[parameter].asString();
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancConfiguration::LookupStringParameter(std::string& target,
                                                   const std::string& parameter) const
  {
    return LookupStringParameterInternal(target, userConfiguration_, parameter);
  }


  std::string OrthancConfiguration::GetStringParameter(const std::string& parameter) const
  {
    std::string value;
    if (LookupStringParameter(value, parameter))
    {
      return value;
    }
    else if (LookupStringParameterInternal(value, defaultConfiguration_, parameter))
    {
      return value;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError,
                             "No or invalid default parameter found in the default configuration for '" + parameter + "'");
    }
  }


  static bool LookupIntegerParameterInternal(int& target,
                                             const Json::Value& config,
                                             const std::string& parameter)
  {
    if (config.isMember(parameter))
    {
      if (config[parameter].type() != Json::intValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter + "\" must be an integer");
      }
      else
      {
        target = config[parameter].asInt();
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancConfiguration::LookupIntegerParameter(int& target,
                                                    const std::string& parameter) const
  {
    return LookupIntegerParameterInternal(target, userConfiguration_, parameter);
  }


  int OrthancConfiguration::GetIntegerParameter(const std::string& parameter) const
  {
    int v;

    if (LookupIntegerParameter(v, parameter))
    {
      return v;
    }
    else if (LookupIntegerParameterInternal(v, defaultConfiguration_, parameter))
    {
      return v;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError,
                             "No or invalid default parameter found in the default configuration for '" + parameter + "'");
    }
  }

    
  static bool LookupUnsignedIntegerParameterInternal(unsigned int& target,
                                                     const Json::Value& config,
                                                     const std::string& parameter)
  {
    int v;
    if (LookupIntegerParameterInternal(v, config, parameter))
    {
      if (v < 0)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "The configuration option \"" + parameter + "\" must be a positive integer");
      }
      else
      {
        target = static_cast<unsigned int>(v);
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancConfiguration::LookupUnsignedIntegerParameter(unsigned int& target,
                                                            const std::string& parameter) const
  {
    return LookupUnsignedIntegerParameterInternal(target, userConfiguration_, parameter);
  }


  unsigned int OrthancConfiguration::GetUnsignedIntegerParameter(const std::string& parameter) const
  {
    unsigned int v;
    if (LookupUnsignedIntegerParameter(v, parameter))
    {
      return v;
    }
    else if (LookupUnsignedIntegerParameterInternal(v, defaultConfiguration_, parameter))
    {
      return v;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError,
                             "No or invalid default parameter found in the default configuration for '" + parameter + "'");
    }
  }


  static bool LookupBooleanParameterInternal(bool& target,
                                             const Json::Value& config,
                                             const std::string& parameter)
  {
    if (config.isMember(parameter))
    {
      if (config[parameter].type() != Json::booleanValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter +
                               "\" must be a Boolean (true or false)");
      }
      else
      {
        target = config[parameter].asBool();
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancConfiguration::LookupBooleanParameter(bool& target,
                                                    const std::string& parameter) const
  {
    return LookupBooleanParameterInternal(target, userConfiguration_, parameter);
  }


  bool OrthancConfiguration::GetBooleanParameter(const std::string& parameter) const
  {
    bool value;
    if (LookupBooleanParameter(value, parameter))
    {
      return value;
    }
    else if (LookupBooleanParameterInternal(value, defaultConfiguration_, parameter))
    {
      return value;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError,
                             "No or invalid default parameter found in the default configuration for '" + parameter + "'");
    }
  }


  std::string OrthancConfiguration::GetConfigurationAbsolutePath() const
  {
    if (configurationPaths_.empty())
    {
      return "";
    }
    else
    {
      if (configurationPaths_.size() >= 2)
      {
        LOG(WARNING) << "A plugin has called the deprecated OrthancPluginGetConfigurationPath() primitive, the results "
                     << "are unreliable because multiple configuration paths were provided";
      }

      const boost::filesystem::path& first = configurationPaths_.front();

      if (boost::filesystem::is_directory(first))
      {
        return boost::filesystem::absolute(first).parent_path().string();
      }
      else
      {
        return boost::filesystem::absolute(first).string();
      }
    }    
  }


  void OrthancConfiguration::Read(const std::list<boost::filesystem::path>& configurationPaths)
  {
    if (configurationPaths.empty())
    {
      LOG(WARNING) << "Using the default Orthanc configuration";
    }

    // Read the content of the configuration
    configurationPaths_ = configurationPaths;
    ReadConfiguration(userConfiguration_, configurationPaths);

    // Adapt the paths to the configurations
    defaultDirectory_ = boost::filesystem::current_path();

    if (!configurationPaths.empty())
    {
      const boost::filesystem::path& first = configurationPaths.front();

      if (boost::filesystem::is_directory(first))
      {
        defaultDirectory_ = first;
      }
      else
      {
        defaultDirectory_ = boost::filesystem::path(first).parent_path();
      }
    }

    LOG(WARNING) << "The paths in the Orthanc configuration will be interpreted relative to folder: "
                 << SystemToolbox::PathToUtf8(defaultDirectory_);
  }


  void OrthancConfiguration::LoadModalitiesAndPeers()
  {
    if (serverIndex_ == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      LoadModalities();
      LoadPeers();
    }
  }


  void OrthancConfiguration::RegisterFont(ServerResources::FileResourceId resource)
  {
    std::string content;
    ServerResources::GetFileResource(content, resource);
    fontRegistry_.AddFromMemory(content);
  }


  void OrthancConfiguration::GetDicomModalityUsingSymbolicName(
    RemoteModalityParameters& modality,
    const std::string& name) const
  {
    Modalities::const_iterator found = modalities_.find(name);

    if (found == modalities_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem,
                             "No modality with symbolic name: " + name);
    }
    else
    {
      modality = found->second;
    }
  }


  bool OrthancConfiguration::LookupOrthancPeer(WebServiceParameters& peer,
                                               const std::string& name) const
  {
    Peers::const_iterator found = peers_.find(name);

    if (found == peers_.end())
    {
      LOG(ERROR) << "No peer with symbolic name: " << name;
      return false;
    }
    else
    {
      peer = found->second;
      return true;
    }
  }


  void OrthancConfiguration::GetListOfDicomModalities(std::set<std::string>& target) const
  {
    target.clear();

    for (Modalities::const_iterator 
           it = modalities_.begin(); it != modalities_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  void OrthancConfiguration::GetListOfOrthancPeers(std::set<std::string>& target) const
  {
    target.clear();

    for (Peers::const_iterator it = peers_.begin(); it != peers_.end(); ++it)
    {
      target.insert(it->first);
    }
  }

  unsigned int OrthancConfiguration::GetDicomLossyTranscodingQuality() const
  {
    return GetUnsignedIntegerParameter(DICOM_LOSSY_TRANSCODING_QUALITY);
  }


  OrthancConfiguration::RegisteredUsersStatus OrthancConfiguration::SetupRegisteredUsers(HttpServer& httpServer) const
  {
    static const char* const REGISTERED_USERS = "RegisteredUsers";

    httpServer.ClearUsers();

    if (!userConfiguration_.isMember(REGISTERED_USERS))
    {
      return RegisteredUsersStatus_NoConfiguration;
    }
    else
    {
      const Json::Value& users = userConfiguration_[REGISTERED_USERS];
      if (users.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted list of users");
      }

      bool hasUser = false;
      Json::Value::Members usernames = users.getMemberNames();
      for (size_t i = 0; i < usernames.size(); i++)
      {
        const std::string& username = usernames[i];

        if (users[username].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted list of users");
        }
        else
        {
          std::string password = users[username].asString();
          httpServer.RegisterUser(username.c_str(), password.c_str());
          hasUser = true;
        }
      }

      return (hasUser ?
              RegisteredUsersStatus_HasUser :
              RegisteredUsersStatus_NoUser);
    }
  }
    

  boost::filesystem::path OrthancConfiguration::InterpretStringParameterAsPath(
    const std::string& parameter) const
  {
    return SystemToolbox::InterpretRelativePath(defaultDirectory_, parameter);
  }

    
  void OrthancConfiguration::GetListOfStringsParameter(std::list<std::string>& target,
                                                       const std::string& key) const
  {
    target.clear();
  
    if (!userConfiguration_.isMember(key))
    {
      return;
    }

    const Json::Value& lst = userConfiguration_[key];

    if (lst.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted list of strings: " + key);
    }

    for (Json::Value::ArrayIndex i = 0; i < lst.size(); i++)
    {
      target.push_back(lst[i].asString());
    }    
  }


  void OrthancConfiguration::GetSetOfStringsParameter(std::set<std::string>& target,
                                                      const std::string& key) const
  {
    target.clear();
  
    if (!userConfiguration_.isMember(key))
    {
      return;
    }

    const Json::Value& lst = userConfiguration_[key];

    if (lst.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted set of strings: " + key);
    }

    for (Json::Value::ArrayIndex i = 0; i < lst.size(); i++)
    {
      target.insert(lst[i].asString());
    }    
  }


  bool OrthancConfiguration::IsSameAETitle(const std::string& aet1,
                                           const std::string& aet2) const
  {
    if (GetBooleanParameter("StrictAetComparison"))
    {
      // Case-sensitive matching
      return aet1 == aet2;
    }
    else
    {
      // Case-insensitive matching (default)
      std::string tmp1, tmp2;
      Toolbox::ToLowerCase(tmp1, aet1);
      Toolbox::ToLowerCase(tmp2, aet2);
      return tmp1 == tmp2;
    }
  }


  bool OrthancConfiguration::LookupDicomModalityUsingAETitle(RemoteModalityParameters& modality,
                                                             const std::string& aet) const
  {
    for (Modalities::const_iterator it = modalities_.begin(); it != modalities_.end(); ++it)
    {
      if (IsSameAETitle(aet, it->second.GetApplicationEntityTitle()))
      {
        modality = it->second;
        return true;
      }
    }

    return false;
  }

  
  void OrthancConfiguration::LookupDicomModalitiesUsingAETitle(std::list<RemoteModalityParameters>& modalities,
                                                               const std::string& aet) const
  {
    modalities.clear();

    for (Modalities::const_iterator it = modalities_.begin(); it != modalities_.end(); ++it)
    {
      if (IsSameAETitle(aet, it->second.GetApplicationEntityTitle()))
      {
        modalities.push_back(it->second);
      }
    }
  }



  bool OrthancConfiguration::IsKnownAETitle(const std::string& aet,
                                            const std::string& ip) const
  {
    RemoteModalityParameters modality;
    
    if (!LookupDicomModalityUsingAETitle(modality, aet))
    {
      LOG(WARNING) << "Modality \"" << aet
                   << "\" is not listed in the \"DicomModalities\" configuration option";
      return false;
    }
    else if (!GetBooleanParameter("DicomCheckModalityHost") ||
             ip == modality.GetHost())
    {
      return true;
    }
    else
    {
      LOG(WARNING) << "Forbidding access from AET \"" << aet
                   << "\" given its hostname (" << ip << ") does not match "
                   << "the \"DicomModalities\" configuration option ("
                   << modality.GetHost() << " was expected)";
      return false;
    }
  }


  RemoteModalityParameters 
  OrthancConfiguration::GetModalityUsingSymbolicName(const std::string& name) const
  {
    RemoteModalityParameters modality;
    GetDicomModalityUsingSymbolicName(modality, name);

    return modality;
  }

    
  RemoteModalityParameters 
  OrthancConfiguration::GetModalityUsingAet(const std::string& aet) const
  {
    RemoteModalityParameters modality;
      
    if (LookupDicomModalityUsingAETitle(modality, aet))
    {
      return modality;
    }
    else
    {
      throw OrthancException(ErrorCode_InexistentItem,
                             "Unknown modality for AET: " + aet);
    }
  }

    
  void OrthancConfiguration::UpdateModality(const std::string& symbolicName,
                                            const RemoteModalityParameters& modality)
  {
    CheckAlphanumeric(symbolicName);
    
    modalities_[symbolicName] = modality;
    SaveModalities();
  }


  void OrthancConfiguration::RemoveModality(const std::string& symbolicName)
  {
    if (modalities_.find(symbolicName) == modalities_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem,
                             "Unknown DICOM modality with symbolic name: " + symbolicName);
    }
    else
    {
      modalities_.erase(symbolicName);
      SaveModalities();
    }
  }

    
  void OrthancConfiguration::UpdatePeer(const std::string& symbolicName,
                                        const WebServiceParameters& peer)
  {
    CheckAlphanumeric(symbolicName);
    
    peer.CheckClientCertificate();

    peers_[symbolicName] = peer;
    SavePeers();
  }


  void OrthancConfiguration::RemovePeer(const std::string& symbolicName)
  {
    if (peers_.find(symbolicName) == peers_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem,
                             "Unknown Orthanc peer: " + symbolicName);
    }
    else
    {
      peers_.erase(symbolicName);
      SavePeers();
    }
  }


  void OrthancConfiguration::Format(std::string& result) const
  {
    Toolbox::WriteStyledJson(result, userConfiguration_);
  }


  void OrthancConfiguration::SetDefaultEncoding(Encoding encoding)
  {
    SetDefaultDicomEncoding(encoding);

    // Propagate the encoding to the configuration file that is
    // stored in memory
    userConfiguration_["DefaultEncoding"] = EnumerationToString(encoding);
  }


  bool OrthancConfiguration::HasConfigurationChanged() const
  {
    Json::Value current;
    ReadConfiguration(current, configurationPaths_);

    std::string a, b;
    Toolbox::WriteFastJson(a, userConfiguration_);
    Toolbox::WriteFastJson(b, current);

    return a != b;
  }


  void OrthancConfiguration::SetServerIndex(ServerIndex& index)
  {
    serverIndex_ = &index;
  }


  void OrthancConfiguration::ResetServerIndex()
  {
    serverIndex_ = NULL;
  }

  
  TemporaryFile* OrthancConfiguration::CreateTemporaryFile() const
  {
    std::string temporaryDirectory = ".";

    if (LookupStringParameter(temporaryDirectory, TEMPORARY_DIRECTORY))
    {
      return new TemporaryFile(InterpretStringParameterAsPath(temporaryDirectory), "");
    }
    else
    {
      return new TemporaryFile;
    }
  }


  std::string OrthancConfiguration::GetDefaultPrivateCreator() const
  {
    // New configuration option in Orthanc 1.6.0
    return GetStringParameter("DefaultPrivateCreator");
  }


  static void GetAcceptOption(std::set<DicomTransferSyntax>& target,
                              const OrthancConfiguration& configuration,
                              const std::string& optionName,
                              TransferSyntaxGroup optionGroup)
  {
    bool accept;
    if (configuration.LookupBooleanParameter(accept, optionName))
    {
      std::set<DicomTransferSyntax> group;
      GetTransferSyntaxGroup(group, optionGroup);

      for (std::set<DicomTransferSyntax>::const_iterator
             syntax = group.begin(); syntax != group.end(); ++syntax)
      {
        if (accept)
        {
          target.insert(*syntax);
        }
        else
        {
          target.erase(*syntax);
        }
      }
    }
  }


  void OrthancConfiguration::GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& target) const
  {
    target.clear();

#if 1
    /**
     * This is the behavior in Orthanc >= 1.9.0. All the transfer
     * syntaxes are accepted by default, and the
     * "TransferSyntaxAccepted" options can be used to disable groups
     * of transfer syntaxes.
     **/

    static const char* const ACCEPTED_TRANSFER_SYNTAXES = "AcceptedTransferSyntaxes";

    if (userConfiguration_.isMember(ACCEPTED_TRANSFER_SYNTAXES))
    {
      ParseAcceptedTransferSyntaxes(target, userConfiguration_[ACCEPTED_TRANSFER_SYNTAXES]);
    }
    else
    {
      GetAllDicomTransferSyntaxes(target);
    }
#else
    /**
     * This was the behavior of Orthanc <= 1.8.2. The uncompressed
     * transfer syntaxes were always accepted, and additional transfer
     * syntaxes were added using the configuration options
     * "XXXTransferSyntaxAccepted".
     **/

    // The 3 transfer syntaxes below were the only ones to be supported in Orthanc <= 0.7.1
    target.insert(DicomTransferSyntax_LittleEndianExplicit);
    target.insert(DicomTransferSyntax_BigEndianExplicit);
    target.insert(DicomTransferSyntax_LittleEndianImplicit);
#endif

    // Groups of transfer syntaxes, supported since Orthanc 0.7.2
    GetAcceptOption(target, *this, "DeflatedTransferSyntaxAccepted", TransferSyntaxGroup_Deflated);
    GetAcceptOption(target, *this, "JpegTransferSyntaxAccepted", TransferSyntaxGroup_Jpeg);
    GetAcceptOption(target, *this, "Jpeg2000TransferSyntaxAccepted", TransferSyntaxGroup_Jpeg2000);
    GetAcceptOption(target, *this, "JpegLosslessTransferSyntaxAccepted", TransferSyntaxGroup_JpegLossless);
    GetAcceptOption(target, *this, "JpipTransferSyntaxAccepted", TransferSyntaxGroup_Jpip);
    GetAcceptOption(target, *this, "Mpeg2TransferSyntaxAccepted", TransferSyntaxGroup_Mpeg2);
    GetAcceptOption(target, *this, "Mpeg4TransferSyntaxAccepted", TransferSyntaxGroup_Mpeg4);
    GetAcceptOption(target, *this, "RleTransferSyntaxAccepted", TransferSyntaxGroup_Rle);
    GetAcceptOption(target, *this, "H265TransferSyntaxAccepted", TransferSyntaxGroup_H265);
  }


  std::string OrthancConfiguration::GetDatabaseServerIdentifier() const
  {
    std::string id;

    if (LookupStringParameter(id, ORTHANC_CONFIG_DATABASE_SERVER_IDENTIFIER))
    {
      if (id.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Global configuration option \"" +
                               std::string(ORTHANC_CONFIG_DATABASE_SERVER_IDENTIFIER) + "\" cannot be empty");
      }
      else
      {
        return id;
      }
    }
    else
    {
      std::set<std::string> items;

      {
        std::set<std::string> mac;
        SystemToolbox::GetMacAddresses(mac);

        for (std::set<std::string>::const_iterator it = mac.begin(); it != mac.end(); ++it)
        {
          items.insert("mac=" + *it);
        }
      }

      items.insert("aet=" + GetOrthancAET());
      items.insert("dicom-port=" + boost::lexical_cast<std::string>(GetDicomPort()));
      items.insert("http-port=" + boost::lexical_cast<std::string>(GetHttpPort()));

      for (std::set<std::string>::const_iterator it = items.begin(); it != items.end(); ++it)
      {
        if (id.empty())
        {
          id = *it;
        }
        else
        {
          id += ("|" + *it);
        }
      }

      std::string hash;
      Toolbox::ComputeSHA1(hash, id);
      return hash;
    }
  }

  void OrthancConfiguration::LoadWarnings()
  {
    if (userConfiguration_.isMember(WARNINGS))
    {
      const Json::Value& warnings = userConfiguration_[WARNINGS];
      if (!warnings.isObject())
      {
        throw OrthancException(ErrorCode_BadFileFormat, std::string(WARNINGS) + " configuration entry is not a Json object");
      }

      Json::Value::Members members = warnings.getMemberNames();

      for (size_t i = 0; i < members.size(); i++)
      {
        const std::string& name = members[i];
        bool enabled = warnings[name].asBool();

        Warnings warning = Warnings_None;
        if (name == "W001_TagsBeingReadFromStorage")
        {
          warning = Warnings_001_TagsBeingReadFromStorage;
        }
        else if (name == "W002_InconsistentDicomTagsInDb")
        {
          warning = Warnings_002_InconsistentDicomTagsInDb;
        }
        else if (name == "W003_DecoderFailure")
        {
          warning = Warnings_003_DecoderFailure;
        }
        else if (name == "W004_NoMainDicomTagsSignature")
        {
          warning = Warnings_004_NoMainDicomTagsSignature;
        }
        else if (name == "W005_RequestingTagFromLowerResourceLevel")
        {
          warning = Warnings_005_RequestingTagFromLowerResourceLevel;
        }
        else if (name == "W006_RequestingTagFromMetaHeader")
        {
          warning = Warnings_006_RequestingTagFromMetaHeader;
        }
        else if (name == "W007_MissingRequestedTagsNotReadFromDisk")
        {
          warning = Warnings_007_MissingRequestedTagsNotReadFromDisk;
        }
        else
        {
          throw OrthancException(ErrorCode_BadFileFormat, name + " is not recognized as a valid warning name");
        }

        if (!enabled)
        {
          disabledWarnings_.insert(warning);
        }
      }
    }
    else
    {
      disabledWarnings_.clear();
    }

  }


  void OrthancConfiguration::DefaultExtractDicomSummary(DicomMap& target,
                                                        const ParsedDicomFile& dicom)
  {
    std::set<DicomTag> ignoreTagLength;
    dicom.ExtractDicomSummary(target, ORTHANC_MAXIMUM_TAG_LENGTH, ignoreTagLength);
  }
  
    
  void OrthancConfiguration::DefaultExtractDicomSummary(DicomMap& target,
                                                        DcmDataset& dicom)
  {
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomSummary(target, dicom, ORTHANC_MAXIMUM_TAG_LENGTH, ignoreTagLength);
  }    
    

  void OrthancConfiguration::DefaultDicomDatasetToJson(Json::Value& target,
                                                       const ParsedDicomFile& dicom)
  {
    std::set<DicomTag> ignoreTagLength;
    DefaultDicomDatasetToJson(target, dicom, ignoreTagLength);
  }


  void OrthancConfiguration::DefaultDicomDatasetToJson(Json::Value& target,
                                                       DcmDataset& dicom,
                                                       const std::set<DicomTag>& ignoreTagLength)
  {
    FromDcmtkBridge::ExtractDicomAsJson(target, dicom, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 
                                        ORTHANC_MAXIMUM_TAG_LENGTH, ignoreTagLength);    
  }    
  
    
  void OrthancConfiguration::DefaultDicomDatasetToJson(Json::Value& target,
                                                       const ParsedDicomFile& dicom,
                                                       const std::set<DicomTag>& ignoreTagLength)
  {
    dicom.DatasetToJson(target, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 
                        ORTHANC_MAXIMUM_TAG_LENGTH, ignoreTagLength);
  }
  

  void OrthancConfiguration::DefaultDicomHeaderToJson(Json::Value& target,
                                                      const ParsedDicomFile& dicom)
  {
    dicom.HeaderToJson(target, DicomToJsonFormat_Full);
  }


  static void AddTransferSyntaxes(std::set<DicomTransferSyntax>& target,
                                  const std::string& source)
  {
    boost::regex pattern(Toolbox::WildcardToRegularExpression(source));

    std::set<DicomTransferSyntax> allSyntaxes;
    GetAllDicomTransferSyntaxes(allSyntaxes);

    for (std::set<DicomTransferSyntax>::const_iterator
           syntax = allSyntaxes.begin(); syntax != allSyntaxes.end(); ++syntax)
    {
      if (regex_match(GetTransferSyntaxUid(*syntax), pattern))
      {
        target.insert(*syntax);
      }
    }
  }


  void OrthancConfiguration::ParseAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& target,
                                                           const Json::Value& source)
  {
    if (source.type() == Json::stringValue)
    {
      AddTransferSyntaxes(target, source.asString());
    }
    else if (source.type() == Json::arrayValue)
    {
      for (Json::Value::ArrayIndex i = 0; i < source.size(); i++)
      {
        if (source[i].type() == Json::stringValue)
        {
          AddTransferSyntaxes(target, source[i].asString());
        }
        else
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  unsigned int OrthancConfiguration::GetLoaderThreads() const
  {
    // from 1.10.0 to 1.12.10, only CONFIG_ZIP_LOADER_THREADS was available -> read from it if CONFIG_LOADER_THREADS is not specified.
    unsigned int loaderThreads = 1; // old ZipLoaderThreads default value

    if (!LookupUnsignedIntegerParameter(loaderThreads, CONFIG_LOADER_THREADS))
    {
      LookupUnsignedIntegerParameter(loaderThreads, CONFIG_ZIP_LOADER_THREADS); // we cannot use GetUnsignedIntegerParameter() because there is no default value for this old configuration
    }

    if (loaderThreads <= 1)
    {
      return 1; // 0 is not a valid internal value anymore
    }
    else
    {
      return loaderThreads;
    }
  }
}
