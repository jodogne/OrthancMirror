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


#pragma once

#include "../../OrthancFramework/Sources/Images/FontRegistry.h"
#include "../../OrthancFramework/Sources/WebServiceParameters.h"
#include "../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"

#include <OrthancServerResources.h>
#include "ServerEnumerations.h"

#include <boost/filesystem.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <set>

class DcmDataset;

namespace Orthanc
{
  class DicomMap;
  class DicomTag;
  class HttpServer;
  class ParsedDicomFile;
  class ServerIndex;
  class TemporaryFile;


  class OrthancConfiguration : public boost::noncopyable
  {
  private:
    typedef std::map<std::string, RemoteModalityParameters>   Modalities;
    typedef std::map<std::string, WebServiceParameters>       Peers;

    boost::shared_mutex      mutex_;
    Json::Value              json_;
    boost::filesystem::path  defaultDirectory_;
    std::string              configurationAbsolutePath_;
    FontRegistry             fontRegistry_;
    const char*              configurationFileArg_;
    Modalities               modalities_;
    Peers                    peers_;
    ServerIndex*             serverIndex_;
    std::set<Warnings>       disabledWarnings_;

    OrthancConfiguration() :
      configurationFileArg_(NULL),
      serverIndex_(NULL)
    {
    }

    void LoadModalitiesFromJson(const Json::Value& source);
    
    void LoadPeersFromJson(const Json::Value& source);
    
    void LoadModalities();
    
    void LoadPeers();
    
    void SaveModalitiesToJson(Json::Value& target);
    
    void SavePeersToJson(Json::Value& target);
    
    void SaveModalities();
    
    void SavePeers();

    static OrthancConfiguration& GetInstance();

  public:
    class ReaderLock : public boost::noncopyable
    {
    private:
      OrthancConfiguration&                    configuration_;
      boost::shared_lock<boost::shared_mutex>  lock_;

    public:
      ReaderLock() :
        configuration_(GetInstance()),
        lock_(configuration_.mutex_)
      {
      }

      const OrthancConfiguration& GetConfiguration() const
      {
        return configuration_;
      }

      const Json::Value& GetJson() const
      {
        return configuration_.json_;
      }
    };


    class WriterLock : public boost::noncopyable
    {
    private:
      OrthancConfiguration&                    configuration_;
      boost::unique_lock<boost::shared_mutex>  lock_;

    public:
      WriterLock() :
        configuration_(GetInstance()),
        lock_(configuration_.mutex_)
      {
      }

      OrthancConfiguration& GetConfiguration()
      {
        return configuration_;
      }

      const OrthancConfiguration& GetConfiguration() const
      {
        return configuration_;
      }

      const Json::Value& GetJson() const
      {
        return configuration_.json_;
      }
    };


    const std::string& GetConfigurationAbsolutePath() const
    {
      return configurationAbsolutePath_;
    }

    const FontRegistry& GetFontRegistry() const
    {
      return fontRegistry_;
    }

    void Read(const char* configurationFile);

    // "SetServerIndex()" must have been called
    void LoadModalitiesAndPeers();

    void LoadWarnings();

    void RegisterFont(ServerResources::FileResourceId resource);

    bool LookupStringParameter(std::string& target,
                               const std::string& parameter) const;

    std::string GetStringParameter(const std::string& parameter,
                                   const std::string& defaultValue) const;
    
    int GetIntegerParameter(const std::string& parameter,
                            int defaultValue) const;
    
    unsigned int GetUnsignedIntegerParameter(const std::string& parameter,
                                             unsigned int defaultValue) const;

    bool LookupBooleanParameter(bool& target,
                                const std::string& parameter) const;

    bool GetBooleanParameter(const std::string& parameter,
                             bool defaultValue) const;

    void GetDicomModalityUsingSymbolicName(RemoteModalityParameters& modality,
                                           const std::string& name) const;

    bool LookupOrthancPeer(WebServiceParameters& peer,
                           const std::string& name) const;

    void GetListOfDicomModalities(std::set<std::string>& target) const;

    void GetListOfOrthancPeers(std::set<std::string>& target) const;

    // Returns "true" iff. at least one user is registered
    bool SetupRegisteredUsers(HttpServer& httpServer) const;

    std::string InterpretStringParameterAsPath(const std::string& parameter) const;
    
    void GetListOfStringsParameter(std::list<std::string>& target,
                                   const std::string& key) const;
    
    bool IsSameAETitle(const std::string& aet1,
                       const std::string& aet2) const;

    void LookupDicomModalitiesUsingAETitle(std::list<RemoteModalityParameters>& modalities,
                                           const std::string& aet) const;

    bool LookupDicomModalityUsingAETitle(RemoteModalityParameters& modality,
                                         const std::string& aet) const;

    bool IsKnownAETitle(const std::string& aet,
                        const std::string& ip) const;

    RemoteModalityParameters GetModalityUsingSymbolicName(const std::string& name) const;
    
    RemoteModalityParameters GetModalityUsingAet(const std::string& aet) const;
    
    void UpdateModality(const std::string& symbolicName,
                        const RemoteModalityParameters& modality);

    void RemoveModality(const std::string& symbolicName);
    
    void UpdatePeer(const std::string& symbolicName,
                    const WebServiceParameters& peer);

    void RemovePeer(const std::string& symbolicName);


    void Format(std::string& result) const;
    
    void SetDefaultEncoding(Encoding encoding);

    bool HasConfigurationChanged() const;

    void SetServerIndex(ServerIndex& index);

    void ResetServerIndex();

    TemporaryFile* CreateTemporaryFile() const;

    std::string GetDefaultPrivateCreator() const;

    std::string GetOrthancAET() const
    {
      return GetStringParameter("DicomAet", "ORTHANC");
    }

    void GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& target) const;

    std::string GetDatabaseServerIdentifier() const;

    bool IsWarningEnabled(Warnings warning) const
    {
      return disabledWarnings_.count(warning) == 0;
    }

    static void DefaultExtractDicomSummary(DicomMap& target,
                                           const ParsedDicomFile& dicom);

    static void DefaultExtractDicomSummary(DicomMap& target,
                                           DcmDataset& dicom);
    
    static void DefaultDicomDatasetToJson(Json::Value& target,
                                          const ParsedDicomFile& dicom);
    
    static void DefaultDicomDatasetToJson(Json::Value& target,
                                          DcmDataset& dicom,
                                          const std::set<DicomTag>& ignoreTagLength);
    
    static void DefaultDicomDatasetToJson(Json::Value& target,
                                          const ParsedDicomFile& dicom,
                                          const std::set<DicomTag>& ignoreTagLength);
    
    static void DefaultDicomHeaderToJson(Json::Value& target,
                                         const ParsedDicomFile& dicom);

    static void ParseAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& target,
                                              const Json::Value& source);
  };
}
