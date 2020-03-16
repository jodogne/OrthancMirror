/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "DicomFormat/DicomTag.h"

#include <json/value.h>
#include <list>
#include <map>

namespace Orthanc
{
  namespace SerializationToolbox
  {
    std::string ReadString(const Json::Value& value,
                           const std::string& field);

    int ReadInteger(const Json::Value& value,
                    const std::string& field);

    unsigned int ReadUnsignedInteger(const Json::Value& value,
                                     const std::string& field);

    bool ReadBoolean(const Json::Value& value,
                     const std::string& field);

    void ReadArrayOfStrings(std::vector<std::string>& target,
                            const Json::Value& value,
                            const std::string& field);

    void ReadListOfStrings(std::list<std::string>& target,
                           const Json::Value& value,
                           const std::string& field);

    void ReadSetOfStrings(std::set<std::string>& target,
                          const Json::Value& value,
                          const std::string& field);

    void ReadSetOfTags(std::set<DicomTag>& target,
                       const Json::Value& value,
                       const std::string& field);

    void ReadMapOfStrings(std::map<std::string, std::string>& values,
                          const Json::Value& target,
                          const std::string& field);

    void ReadMapOfTags(std::map<DicomTag, std::string>& values,
                       const Json::Value& target,
                       const std::string& field);

    void WriteArrayOfStrings(Json::Value& target,
                             const std::vector<std::string>& values,
                             const std::string& field);

    void WriteListOfStrings(Json::Value& target,
                            const std::list<std::string>& values,
                            const std::string& field);

    void WriteSetOfStrings(Json::Value& target,
                           const std::set<std::string>& values,
                           const std::string& field);

    void WriteSetOfTags(Json::Value& target,
                        const std::set<DicomTag>& tags,
                        const std::string& field);

    void WriteMapOfStrings(Json::Value& target,
                           const std::map<std::string, std::string>& values,
                           const std::string& field);

    void WriteMapOfTags(Json::Value& target,
                        const std::map<DicomTag, std::string>& values,
                        const std::string& field);
  }
}
