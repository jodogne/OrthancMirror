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

#include <string>
#include <set>
#include <json/json.h>
#include <stdint.h>

#include "../Core/FileStorage/IStorageArea.h"
#include "../Core/HttpServer/MongooseServer.h"
#include "../Core/Images/FontRegistry.h"
#include "../Core/WebServiceParameters.h"
#include "../Core/DicomNetworking/RemoteModalityParameters.h"

#include "IDatabaseWrapper.h"
#include "ServerEnumerations.h"


namespace Orthanc
{
  void OrthancInitialize(const char* configurationFile = NULL);

  void OrthancFinalize();

  class Configuration
  {
  private:
    Configuration();  // Forbidden, this is a static class

  public:
    static std::string GetGlobalStringParameter(const std::string& parameter,
                                                const std::string& defaultValue);

    static int GetGlobalIntegerParameter(const std::string& parameter,
                                         int defaultValue);

    static unsigned int GetGlobalUnsignedIntegerParameter(const std::string& parameter,
                                                          unsigned int defaultValue);

    static bool GetGlobalBoolParameter(const std::string& parameter,
                                       bool defaultValue);

    static void GetDicomModalityUsingSymbolicName(RemoteModalityParameters& modality,
                                                  const std::string& name);

    static bool LookupDicomModalityUsingAETitle(RemoteModalityParameters& modality,
                                                const std::string& aet);

    static void GetOrthancPeer(WebServiceParameters& peer,
                               const std::string& name);

    static void GetListOfDicomModalities(std::set<std::string>& target);

    static void GetListOfOrthancPeers(std::set<std::string>& target);

    static void SetupRegisteredUsers(MongooseServer& httpServer);

    static std::string InterpretRelativePath(const std::string& baseDirectory,
                                             const std::string& relativePath);

    static std::string InterpretStringParameterAsPath(const std::string& parameter);

    static void GetGlobalListOfStringsParameter(std::list<std::string>& target,
                                                const std::string& key);

    static bool IsKnownAETitle(const std::string& aet,
                               const std::string& ip);

    static bool IsSameAETitle(const std::string& aet1,
                              const std::string& aet2);

    static RemoteModalityParameters GetModalityUsingSymbolicName(const std::string& name);

    static RemoteModalityParameters GetModalityUsingAet(const std::string& aet);

    static void UpdateModality(const std::string& symbolicName,
                               const RemoteModalityParameters& modality);

    static void RemoveModality(const std::string& symbolicName);

    static void UpdatePeer(const std::string& symbolicName,
                           const WebServiceParameters& peer);

    static void RemovePeer(const std::string& symbolicName);

    static const std::string& GetConfigurationAbsolutePath();

    static IDatabaseWrapper* CreateDatabaseWrapper();

    static IStorageArea* CreateStorageArea();

    static void GetConfiguration(Json::Value& result);

    static void FormatConfiguration(std::string& result);

    static const FontRegistry& GetFontRegistry();

    static void SetDefaultEncoding(Encoding encoding);

    static bool HasConfigurationChanged();
  };
}
