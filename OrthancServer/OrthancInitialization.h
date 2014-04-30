/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#pragma once

#include <string>
#include <set>
#include <json/json.h>
#include <stdint.h>
#include "../Core/HttpServer/MongooseServer.h"
#include "DicomProtocol/RemoteModalityParameters.h"
#include "ServerEnumerations.h"

namespace Orthanc
{
  void OrthancInitialize(const char* configurationFile = NULL);

  void OrthancFinalize();

  std::string GetGlobalStringParameter(const std::string& parameter,
                                       const std::string& defaultValue);

  int GetGlobalIntegerParameter(const std::string& parameter,
                                int defaultValue);

  bool GetGlobalBoolParameter(const std::string& parameter,
                              bool defaultValue);

  void GetDicomModalityUsingSymbolicName(const std::string& name,
                                         std::string& aet,
                                         std::string& address,
                                         int& port,
                                         ModalityManufacturer& manufacturer);

  bool LookupDicomModalityUsingAETitle(const std::string& aet,
                                       std::string& symbolicName,
                                       std::string& address,
                                       int& port,
                                       ModalityManufacturer& manufacturer);

  void GetOrthancPeer(const std::string& name,
                      std::string& url,
                      std::string& username,
                      std::string& password);

  void GetListOfDicomModalities(std::set<std::string>& target);

  void GetListOfOrthancPeers(std::set<std::string>& target);

  void SetupRegisteredUsers(MongooseServer& httpServer);

  std::string InterpretRelativePath(const std::string& baseDirectory,
                                    const std::string& relativePath);

  std::string InterpretStringParameterAsPath(const std::string& parameter);

  void GetGlobalListOfStringsParameter(std::list<std::string>& target,
                                       const std::string& key);

  bool IsKnownAETitle(const std::string& aet);

  bool IsSameAETitle(const std::string& aet1,
                     const std::string& aet2);

  RemoteModalityParameters GetModalityUsingSymbolicName(const std::string& name);

  RemoteModalityParameters GetModalityUsingAet(const std::string& aet);
}
