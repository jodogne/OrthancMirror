/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <boost/regex.hpp>


static const char* const DICOM_MODALITIES = "DicomModalities";
static const char* const DICOM_MODALITIES_IN_DB = "DicomModalitiesInDatabase";
static const char* const ORTHANC_PEERS = "OrthancPeers";
static const char* const ORTHANC_PEERS_IN_DB = "OrthancPeersInDatabase";
static const char* const TEMPORARY_DIRECTORY = "TemporaryDirectory";
static const char* const DATABASE_SERVER_IDENTIFIER = "DatabaseServerIdentifier";
static const char* const WARNINGS = "Warnings";

namespace Orthanc
{
  static void AddFileToConfiguration(Json::Value& target,
                                     const boost::filesystem::path& path)
  {
    std::map<std::string, std::string> env;
    SystemToolbox::GetEnvironmentVariables(env);
    
    LOG(WARNING) << "Reading the configuration from: " << path;

    Json::Value config;

    {
      std::string content;
      SystemToolbox::ReadFile(content, path.string());

      content = Toolbox::SubstituteVariables(content, env);

      Json::Value tmp;
      if (!Toolbox::ReadJson(tmp, content) ||
          tmp.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadJson,
                               "The configuration file does not follow the JSON syntax: " + path.string());
      }

      Toolbox::CopyJsonWithoutComments(config, tmp);
    }

    if (target.size() == 0)
    {
      target = config;
    }
    else
    {
      // Merge the newly-added file with the previous content of "target"
      Json::Value::Members members = config.getMemberNames();
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
          target[members[i]] = config[members[i]];
        }
      }
    }
  }

    
  static void ScanFolderForConfiguration(Json::Value& target,
                                         const char* folder)
  {
    using namespace boost::filesystem;

    LOG(WARNING) << "Scanning folder \"" << folder << "\" for configuration files";

    directory_iterator end_it; // default construction yields past-the-end
    for (directory_iterator it(folder);
         it != end_it;
         ++it)
    {
      if (!is_directory(it->status()))
      {
        std::string extension = boost::filesystem::extension(it->path());
        Toolbox::ToLowerCase(extension);

        if (extension == ".json")
        {
          AddFileToConfiguration(target, it->path().string());
        }
      }
    }
  }

    
  static void ReadConfiguration(Json::Value& target,
                                const char* configurationFile)
  {
    target = Json::objectValue;

    if (configurationFile != NULL)
    {
      if (!boost::filesystem::exists(configurationFile))
      {
        throw OrthancException(ErrorCode_InexistentFile,
                               "Inexistent path to configuration: " +
                               std::string(configurationFile));
      }
      
      if (boost::filesystem::is_directory(configurationFile))
      {
        ScanFolderForConfiguration(target, configurationFile);
      }
      else
      {
        AddFileToConfiguration(target, configurationFile);
      }
    }
    else
    {
#if ORTHANC_STANDALONE == 1
      // No default path for the standalone configuration
      LOG(WARNING) << "Using the default Orthanc configuration";
      return;

#else
      // In a non-standalone build, we use the
      // "Resources/Configuration.json" from the Orthanc source code

      boost::filesystem::path p = ORTHANC_PATH;
      p /= "Resources";
      p /= "Configuration.json";

      AddFileToConfiguration(target, p);
#endif
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
    if (GetBooleanParameter(DICOM_MODALITIES_IN_DB, false))
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
      if (json_.isMember(DICOM_MODALITIES))
      {
        LoadModalitiesFromJson(json_[DICOM_MODALITIES]);
      }
      else
      {
        modalities_.clear();
      }
    }
  }

  void OrthancConfiguration::LoadPeers()
  {
    if (GetBooleanParameter(ORTHANC_PEERS_IN_DB, false))
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
      if (json_.isMember(ORTHANC_PEERS))
      {
        LoadPeersFromJson(json_[ORTHANC_PEERS]);
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
    if (GetBooleanParameter(DICOM_MODALITIES_IN_DB, false))
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
          json_.isMember(DICOM_MODALITIES))
      {
        SaveModalitiesToJson(json_[DICOM_MODALITIES]);
      }
    }
  }


  void OrthancConfiguration::SavePeers()
  {
    if (GetBooleanParameter(ORTHANC_PEERS_IN_DB, false))
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
          json_.isMember(ORTHANC_PEERS))
      {
        SavePeersToJson(json_[ORTHANC_PEERS]);
      }
    }
  }


  OrthancConfiguration& OrthancConfiguration::GetInstance()
  {
    static OrthancConfiguration configuration;
    return configuration;
  }


  bool OrthancConfiguration::LookupStringParameter(std::string& target,
                                                   const std::string& parameter) const
  {
    if (json_.isMember(parameter))
    {
      if (json_[parameter].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter + "\" must be a string");
      }
      else
      {
        target = json_[parameter].asString();
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  std::string OrthancConfiguration::GetStringParameter(const std::string& parameter,
                                                       const std::string& defaultValue) const
  {
    std::string value;
    if (LookupStringParameter(value, parameter))
    {
      return value;
    }
    else
    {
      return defaultValue;
    }
  }

    
  int OrthancConfiguration::GetIntegerParameter(const std::string& parameter,
                                                int defaultValue) const
  {
    if (json_.isMember(parameter))
    {
      if (json_[parameter].type() != Json::intValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter + "\" must be an integer");
      }
      else
      {
        return json_[parameter].asInt();
      }
    }
    else
    {
      return defaultValue;
    }
  }

    
  unsigned int OrthancConfiguration::GetUnsignedIntegerParameter(
    const std::string& parameter,
    unsigned int defaultValue) const
  {
    int v = GetIntegerParameter(parameter, defaultValue);

    if (v < 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "The configuration option \"" + parameter + "\" must be a positive integer");
    }
    else
    {
      return static_cast<unsigned int>(v);
    }
  }


  bool OrthancConfiguration::LookupBooleanParameter(bool& target,
                                                    const std::string& parameter) const
  {
    if (json_.isMember(parameter))
    {
      if (json_[parameter].type() != Json::booleanValue)
      {
        throw OrthancException(ErrorCode_BadParameterType,
                               "The configuration option \"" + parameter +
                               "\" must be a Boolean (true or false)");
      }
      else
      {
        target = json_[parameter].asBool();
        return true;
      }
    }
    else
    {
      return false;
    }
  }


  bool OrthancConfiguration::GetBooleanParameter(const std::string& parameter,
                                                 bool defaultValue) const
  {
    bool value;
    if (LookupBooleanParameter(value, parameter))
    {
      return value;
    }
    else
    {
      return defaultValue;
    }
  }


  void OrthancConfiguration::Read(const char* configurationFile)
  {
    // Read the content of the configuration
    configurationFileArg_ = configurationFile;
    ReadConfiguration(json_, configurationFile);

    // Adapt the paths to the configurations
    defaultDirectory_ = boost::filesystem::current_path();
    configurationAbsolutePath_ = "";

    if (configurationFile)
    {
      if (boost::filesystem::is_directory(configurationFile))
      {
        defaultDirectory_ = boost::filesystem::path(configurationFile);
        configurationAbsolutePath_ = boost::filesystem::absolute(configurationFile).parent_path().string();
      }
      else
      {
        defaultDirectory_ = boost::filesystem::path(configurationFile).parent_path();
        configurationAbsolutePath_ = boost::filesystem::absolute(configurationFile).string();
      }
    }
    else
    {
#if ORTHANC_STANDALONE != 1
      // In a non-standalone build, we use the
      // "Resources/Configuration.json" from the Orthanc source code

      boost::filesystem::path p = ORTHANC_PATH;
      p /= "Resources";
      p /= "Configuration.json";
      configurationAbsolutePath_ = boost::filesystem::absolute(p).string();
#endif
    }
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


  bool OrthancConfiguration::SetupRegisteredUsers(HttpServer& httpServer) const
  {
    httpServer.ClearUsers();

    if (!json_.isMember("RegisteredUsers"))
    {
      return false;
    }

    const Json::Value& users = json_["RegisteredUsers"];
    if (users.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted list of users");
    }

    bool hasUser = false;
    Json::Value::Members usernames = users.getMemberNames();
    for (size_t i = 0; i < usernames.size(); i++)
    {
      const std::string& username = usernames[i];
      std::string password = users[username].asString();
      httpServer.RegisterUser(username.c_str(), password.c_str());
      hasUser = true;
    }

    return hasUser;
  }
    

  std::string OrthancConfiguration::InterpretStringParameterAsPath(
    const std::string& parameter) const
  {
    return SystemToolbox::InterpretRelativePath(defaultDirectory_.string(), parameter);
  }

    
  void OrthancConfiguration::GetListOfStringsParameter(std::list<std::string>& target,
                                                       const std::string& key) const
  {
    target.clear();
  
    if (!json_.isMember(key))
    {
      return;
    }

    const Json::Value& lst = json_[key];

    if (lst.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Badly formatted list of strings");
    }

    for (Json::Value::ArrayIndex i = 0; i < lst.size(); i++)
    {
      target.push_back(lst[i].asString());
    }    
  }

    
  bool OrthancConfiguration::IsSameAETitle(const std::string& aet1,
                                           const std::string& aet2) const
  {
    if (GetBooleanParameter("StrictAetComparison", false))
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
    else if (!GetBooleanParameter("DicomCheckModalityHost", false) ||
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
    Toolbox::WriteStyledJson(result, json_);
  }


  void OrthancConfiguration::SetDefaultEncoding(Encoding encoding)
  {
    SetDefaultDicomEncoding(encoding);

    // Propagate the encoding to the configuration file that is
    // stored in memory
    json_["DefaultEncoding"] = EnumerationToString(encoding);
  }


  bool OrthancConfiguration::HasConfigurationChanged() const
  {
    Json::Value current;
    ReadConfiguration(current, configurationFileArg_);

    std::string a, b;
    Toolbox::WriteFastJson(a, json_);
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
    if (json_.isMember(TEMPORARY_DIRECTORY))
    {
      return new TemporaryFile(InterpretStringParameterAsPath(GetStringParameter(TEMPORARY_DIRECTORY, ".")), "");
    }
    else
    {
      return new TemporaryFile;
    }
  }


  std::string OrthancConfiguration::GetDefaultPrivateCreator() const
  {
    // New configuration option in Orthanc 1.6.0
    return GetStringParameter("DefaultPrivateCreator", "");
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

    if (json_.isMember(ACCEPTED_TRANSFER_SYNTAXES))
    {
      ParseAcceptedTransferSyntaxes(target, json_[ACCEPTED_TRANSFER_SYNTAXES]);
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

    if (LookupStringParameter(id, DATABASE_SERVER_IDENTIFIER))
    {
      if (id.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Global configuration option \"" +
                               std::string(DATABASE_SERVER_IDENTIFIER) + "\" cannot be empty");
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

      items.insert("aet=" + GetStringParameter("DicomAet", "ORTHANC"));
      items.insert("dicom-port=" + boost::lexical_cast<std::string>(GetUnsignedIntegerParameter("DicomPort", 4242)));
      items.insert("http-port=" + boost::lexical_cast<std::string>(GetUnsignedIntegerParameter("HttpPort", 8042)));

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
    if (json_.isMember(WARNINGS))
    {
      const Json::Value& warnings = json_[WARNINGS];
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
}
