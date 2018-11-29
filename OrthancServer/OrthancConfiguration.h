/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#pragma once

#include "../Core/Images/FontRegistry.h"
#include "../Core/WebServiceParameters.h"
#include "../Core/DicomNetworking/RemoteModalityParameters.h"

#include <EmbeddedResources.h>

#include <boost/filesystem.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

namespace Orthanc
{
  class MongooseServer;
  
  class OrthancConfiguration : public boost::noncopyable
  {
  private:
    boost::shared_mutex      mutex_;
    Json::Value              json_;
    boost::filesystem::path  defaultDirectory_;
    std::string              configurationAbsolutePath_;
    FontRegistry             fontRegistry_;
    const char*              configurationFileArg_;

    OrthancConfiguration() :
      configurationFileArg_(NULL)
    {
    }

    void ValidateConfiguration() const;
    
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
    
    void RegisterFont(EmbeddedResources::FileResourceId resource)
    {
      fontRegistry_.AddFromResource(resource);
    }

    std::string GetStringParameter(const std::string& parameter,
                                   const std::string& defaultValue) const;
    
    int GetIntegerParameter(const std::string& parameter,
                            int defaultValue) const;
    
    unsigned int GetUnsignedIntegerParameter(const std::string& parameter,
                                             unsigned int defaultValue) const;

    bool GetBooleanParameter(const std::string& parameter,
                             bool defaultValue) const;

    void GetDicomModalityUsingSymbolicName(RemoteModalityParameters& modality,
                                           const std::string& name) const;

    bool GetOrthancPeer(WebServiceParameters& peer,
                        const std::string& name) const;

    bool ReadKeys(std::set<std::string>& target,
                  const char* parameter,
                  bool onlyAlphanumeric) const;

    void GetListOfDicomModalities(std::set<std::string>& target) const;

    void GetListOfOrthancPeers(std::set<std::string>& target) const;

    void SetupRegisteredUsers(MongooseServer& httpServer) const;

    std::string InterpretStringParameterAsPath(const std::string& parameter) const;
    
    void GetListOfStringsParameter(std::list<std::string>& target,
                                   const std::string& key) const;
    
    bool IsSameAETitle(const std::string& aet1,
                       const std::string& aet2) const;

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
  };
}
