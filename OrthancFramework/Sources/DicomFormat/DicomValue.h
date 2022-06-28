/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../Enumerations.h"

#include <stdint.h>
#include <boost/noncopyable.hpp>
#include <json/value.h>

#if !defined(ORTHANC_ENABLE_BASE64)
#  error The macro ORTHANC_ENABLE_BASE64 must be defined
#endif


namespace Orthanc
{
  class ORTHANC_PUBLIC DicomValue : public boost::noncopyable
  {
  private:
    enum Type
    {
      Type_Null,
      Type_String,
      Type_Binary,
      Type_SequenceAsJson
    };

    Type         type_;
    std::string  content_;
    Json::Value  sequenceJson_;

    DicomValue(const DicomValue& other);

  public:
    DicomValue();
    
    DicomValue(const std::string& content,
               bool isBinary);
    
    DicomValue(const char* data,
               size_t size,
               bool isBinary);
    
    DicomValue(const Json::Value& value);
    
    const std::string& GetContent() const;

    const Json::Value& GetSequenceContent() const;

    bool IsNull() const;

    bool IsBinary() const;

    bool IsString() const;

    bool IsSequence() const;
    
    DicomValue* Clone() const;

#if ORTHANC_ENABLE_BASE64 == 1
    void FormatDataUriScheme(std::string& target,
                             const std::string& mime) const;

    void FormatDataUriScheme(std::string& target) const;
#endif

    bool CopyToString(std::string& result,
                      bool allowBinary) const;
    
    bool ParseInteger32(int32_t& result) const;

    bool ParseInteger64(int64_t& result) const;                                

    bool ParseUnsignedInteger32(uint32_t& result) const;

    bool ParseUnsignedInteger64(uint64_t& result) const;                                

    bool ParseFloat(float& result) const;                                

    bool ParseDouble(double& result) const;

    bool ParseFirstFloat(float& result) const;

    bool ParseFirstUnsignedInteger(unsigned int& result) const;

    void Serialize(Json::Value& target) const;

    void Unserialize(const Json::Value& source);
  };
}
