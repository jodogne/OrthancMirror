/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "DicomFormat/DicomTag.h"
#include "OrthancFramework.h"

#include <json/value.h>
#include <list>
#include <map>

namespace Orthanc
{
  class ORTHANC_PUBLIC SerializationToolbox
  {
  public:
    static std::string ReadString(const Json::Value& value,
                                  const std::string& field);

    static std::string ReadString(const Json::Value& value,
                                  const std::string& field,
                                  const std::string& defaultValue);

    static int ReadInteger(const Json::Value& value,
                           const std::string& field);

    static int ReadInteger(const Json::Value& value,
                           const std::string& field,
                           int defaultValue);

    static unsigned int ReadUnsignedInteger(const Json::Value& value,
                                            const std::string& field);

    static unsigned int ReadUnsignedInteger(const Json::Value& value,
                                            const std::string& field,
                                            unsigned int defaultValue);

    static bool ReadBoolean(const Json::Value& value,
                            const std::string& field);

    static void ReadArrayOfStrings(std::vector<std::string>& target,
                                   const Json::Value& valueObject,
                                   const std::string& field);

    static void ReadArrayOfStrings(std::vector<std::string>& target,
                                   const Json::Value& valueArray);

    static void ReadListOfStrings(std::list<std::string>& target,
                                  const Json::Value& value,
                                  const std::string& field);

    static void ReadSetOfStrings(std::set<std::string>& target,
                                 const Json::Value& valueObject,
                                 const std::string& field);

    static void ReadSetOfStrings(std::set<std::string>& target,
                                 const Json::Value& valueArray);

    static void ReadSetOfTags(std::set<DicomTag>& target,
                              const Json::Value& value,
                              const std::string& field);

    static void ReadMapOfStrings(std::map<std::string, std::string>& target,
                                 const Json::Value& value,
                                 const std::string& field);

    static void ReadMapOfTags(std::map<DicomTag, std::string>& target,
                              const Json::Value& value,
                              const std::string& field);

    static void WriteArrayOfStrings(Json::Value& target,
                                    const std::vector<std::string>& values,
                                    const std::string& field);

    static void WriteListOfStrings(Json::Value& target,
                                   const std::list<std::string>& values,
                                   const std::string& field);

    static void WriteSetOfStrings(Json::Value& targetObject,
                                  const std::set<std::string>& values,
                                  const std::string& field);

    static void WriteSetOfStrings(Json::Value& targetArray,
                                  const std::set<std::string>& values);

    static void WriteSetOfTags(Json::Value& target,
                               const std::set<DicomTag>& tags,
                               const std::string& field);

    static void WriteMapOfStrings(Json::Value& target,
                                  const std::map<std::string, std::string>& values,
                                  const std::string& field);

    static void WriteMapOfTags(Json::Value& target,
                               const std::map<DicomTag, std::string>& values,
                               const std::string& field);

    static bool ParseInteger32(int32_t& result,
                               const std::string& value);

    static bool ParseInteger64(int64_t& result,
                               const std::string& value);

    static bool ParseUnsignedInteger32(uint32_t& result,
                                       const std::string& value);

    static bool ParseUnsignedInteger64(uint64_t& result,
                                       const std::string& value);

    static bool ParseFloat(float& result,
                           const std::string& value);

    static bool ParseDouble(double& result,
                            const std::string& value);

    static bool ParseFirstInteger32(int32_t& result,
                                    const std::string& value);

    static bool ParseFirstInteger64(int64_t& result,
                                    const std::string& value);

    static bool ParseFirstUnsignedInteger32(uint32_t& result,
                                            const std::string& value);

    static bool ParseFirstUnsignedInteger64(uint64_t& result,
                                            const std::string& value);

    static bool ParseFirstFloat(float& result,
                                const std::string& value);

    static bool ParseFirstDouble(double& result,
                                 const std::string& value);

    static bool ParseBoolean(bool& result,
                             const std::string& value);
  };
}
