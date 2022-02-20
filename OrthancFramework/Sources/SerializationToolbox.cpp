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


#include "PrecompiledHeaders.h"
#include "SerializationToolbox.h"

#include "OrthancException.h"
#include "Toolbox.h"

#if ORTHANC_ENABLE_DCMTK == 1
#  include "DicomParsing/FromDcmtkBridge.h"
#endif

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  static bool ParseTagInternal(DicomTag& tag,
                               const char* name)
  {
#if ORTHANC_ENABLE_DCMTK == 1
    try
    {
      tag = FromDcmtkBridge::ParseTag(name);
      return true;
    }
    catch (OrthancException&)
    {
      return false;
    }
#else
    return DicomTag::ParseHexadecimal(tag, name);
#endif   
  }

    
  std::string SerializationToolbox::ReadString(const Json::Value& value,
                                               const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "String value expected in field: " + field);
    }
    else
    {
      return value[field.c_str()].asString();
    }
  }


  int SerializationToolbox::ReadInteger(const Json::Value& value,
                                        const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        (value[field.c_str()].type() != Json::intValue &&
         value[field.c_str()].type() != Json::uintValue))
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Integer value expected in field: " + field);
    }
    else
    {
      return value[field.c_str()].asInt();
    }    
  }


  int SerializationToolbox::ReadInteger(const Json::Value& value,
                                        const std::string& field,
                                        int defaultValue)
  {
    if (value.isMember(field.c_str()))
    {
      return ReadInteger(value, field);
    }
    else
    {
      return defaultValue;
    }
  }


  unsigned int SerializationToolbox::ReadUnsignedInteger(const Json::Value& value,
                                                         const std::string& field)
  {
    int tmp = ReadInteger(value, field);

    if (tmp < 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Unsigned integer value expected in field: " + field);
    }
    else
    {
      return static_cast<unsigned int>(tmp);
    }
  }


  unsigned int SerializationToolbox::ReadUnsignedInteger(const Json::Value& value,
                                                         const std::string& field,
                                                         unsigned int defaultValue)
  {
    if (value.isMember(field.c_str()))
    {
      return ReadUnsignedInteger(value, field);
    }
    else
    {
      return defaultValue;
    }
  }


  bool SerializationToolbox::ReadBoolean(const Json::Value& value,
                                         const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::booleanValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Boolean value expected in field: " + field);
    }
    else
    {
      return value[field.c_str()].asBool();
    }   
  }

  
  void SerializationToolbox::ReadArrayOfStrings(std::vector<std::string>& target,
                                                const Json::Value& value,
                                                const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "List of strings expected in field: " + field);
    }

    const Json::Value& arr = value[field.c_str()];

    target.resize(arr.size());

    for (Json::Value::ArrayIndex i = 0; i < arr.size(); i++)
    {
      if (arr[i].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "List of strings expected in field: " + field);
      }
      else
      {
        target[i] = arr[i].asString();
      }
    }
  }


  void SerializationToolbox::ReadListOfStrings(std::list<std::string>& target,
                                               const Json::Value& value,
                                               const std::string& field)
  {
    std::vector<std::string> tmp;
    ReadArrayOfStrings(tmp, value, field);

    target.clear();
    for (size_t i = 0; i < tmp.size(); i++)
    {
      target.push_back(tmp[i]);
    }
  }
  

  void SerializationToolbox::ReadSetOfStrings(std::set<std::string>& target,
                                              const Json::Value& value,
                                              const std::string& field)
  {
    std::vector<std::string> tmp;
    ReadArrayOfStrings(tmp, value, field);

    target.clear();
    for (size_t i = 0; i < tmp.size(); i++)
    {
      target.insert(tmp[i]);
    }
  }


  void SerializationToolbox::ReadSetOfTags(std::set<DicomTag>& target,
                                           const Json::Value& value,
                                           const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Set of DICOM tags expected in field: " + field);
    }

    const Json::Value& arr = value[field.c_str()];

    target.clear();

    for (Json::Value::ArrayIndex i = 0; i < arr.size(); i++)
    {
      DicomTag tag(0, 0);

      if (arr[i].type() != Json::stringValue ||
          !ParseTagInternal(tag, arr[i].asCString()))
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Set of DICOM tags expected in field: " + field);
      }
      else
      {
        target.insert(tag);
      }
    }
  }


  void SerializationToolbox::ReadMapOfStrings(std::map<std::string, std::string>& target,
                                              const Json::Value& value,
                                              const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Associative array of strings to strings expected in field: " + field);
    }

    const Json::Value& source = value[field.c_str()];

    target.clear();

    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& tmp = source[members[i]];

      if (tmp.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Associative array of string to strings expected in field: " + field);
      }
      else
      {
        target[members[i]] = tmp.asString();
      }
    }
  }


  void SerializationToolbox::ReadMapOfTags(std::map<DicomTag, std::string>& target,
                                           const Json::Value& value,
                                           const std::string& field)
  {
    if (value.type() != Json::objectValue ||
        !value.isMember(field.c_str()) ||
        value[field.c_str()].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Associative array of DICOM tags to strings expected in field: " + field);
    }

    const Json::Value& source = value[field.c_str()];

    target.clear();

    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& tmp = source[members[i]];

      DicomTag tag(0, 0);

      if (!ParseTagInternal(tag, members[i].c_str()) ||
          tmp.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Associative array of DICOM tags to strings expected in field: " + field);
      }
      else
      {
        target[tag] = tmp.asString();
      }
    }
  }


  void SerializationToolbox::WriteArrayOfStrings(Json::Value& target,
                                                 const std::vector<std::string>& values,
                                                 const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::arrayValue;
    for (size_t i = 0; i < values.size(); i++)
    {
      value.append(values[i]);
    }
  }


  void SerializationToolbox::WriteListOfStrings(Json::Value& target,
                                                const std::list<std::string>& values,
                                                const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::arrayValue;

    for (std::list<std::string>::const_iterator it = values.begin();
         it != values.end(); ++it)
    {
      value.append(*it);
    }
  }


  void SerializationToolbox::WriteSetOfStrings(Json::Value& target,
                                               const std::set<std::string>& values,
                                               const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::arrayValue;

    for (std::set<std::string>::const_iterator it = values.begin();
         it != values.end(); ++it)
    {
      value.append(*it);
    }
  }


  void SerializationToolbox::WriteSetOfTags(Json::Value& target,
                                            const std::set<DicomTag>& tags,
                                            const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::arrayValue;

    for (std::set<DicomTag>::const_iterator it = tags.begin();
         it != tags.end(); ++it)
    {
      value.append(it->Format());
    }
  }


  void SerializationToolbox::WriteMapOfStrings(Json::Value& target,
                                               const std::map<std::string, std::string>& values,
                                               const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::objectValue;

    for (std::map<std::string, std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      value[it->first] = it->second;
    }
  }


  void SerializationToolbox::WriteMapOfTags(Json::Value& target,
                                            const std::map<DicomTag, std::string>& values,
                                            const std::string& field)
  {
    if (target.type() != Json::objectValue ||
        target.isMember(field.c_str()))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value& value = target[field];

    value = Json::objectValue;

    for (std::map<DicomTag, std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      value[it->first.Format()] = it->second;
    }
  }


  template <typename T,
            bool allowSigned>
  static bool ParseValue(T& target,
                         const std::string& source)
  {
    try
    {
      std::string value = Toolbox::StripSpaces(source);
      if (value.empty())
      {
        return false;
      }
      else if (!allowSigned &&
               value[0] == '-')
      {
        return false;
      }
      else
      {
        target = boost::lexical_cast<T>(value);
        return true;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      return false;
    }
  }


  bool SerializationToolbox::ParseInteger32(int32_t& target,
                                            const std::string& source)
  {
    int64_t tmp;
    if (ParseValue<int64_t, true>(tmp, source))
    {
      target = static_cast<int32_t>(tmp);
      return (tmp == static_cast<int64_t>(target));  // Check no overflow occurs
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseInteger64(int64_t& target,
                                            const std::string& source)
  {
    return ParseValue<int64_t, true>(target, source);
  }
  

  bool SerializationToolbox::ParseUnsignedInteger32(uint32_t& target,
                                                    const std::string& source)
  {
    uint64_t tmp;
    if (ParseValue<uint64_t, false>(tmp, source))
    {
      target = static_cast<uint32_t>(tmp);
      return (tmp == static_cast<uint64_t>(target));  // Check no overflow occurs
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseUnsignedInteger64(uint64_t& target,
                                                    const std::string& source)
  {
    return ParseValue<uint64_t, false>(target, source);
  }
  

  bool SerializationToolbox::ParseFloat(float& target,
                                        const std::string& source)
  {
    return ParseValue<float, true>(target, source);
  }
         

  bool SerializationToolbox::ParseDouble(double& target,
                                         const std::string& source)
  {
    return ParseValue<double, true>(target, source);
  }


  static bool GetFirstItem(std::string& target,
                           const std::string& source)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, source, '\\');

    if (tokens.empty())
    {
      return false;
    }
    else
    {
      target = tokens[0];
      return true;
    }
  }
  

  bool SerializationToolbox::ParseFirstInteger32(int32_t& target,
                                                 const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseInteger32(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseFirstInteger64(int64_t& target,
                                                 const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseInteger64(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseFirstUnsignedInteger32(uint32_t& target,
                                                         const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseUnsignedInteger32(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseFirstUnsignedInteger64(uint64_t& target,
                                                         const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseUnsignedInteger64(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseFirstFloat(float& target,
                                             const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseFloat(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseFirstDouble(double& target,
                                              const std::string& source)
  {
    std::string first;
    if (GetFirstItem(first, source))
    {
      return ParseDouble(target, first);
    }
    else
    {
      return false;
    }
  }
  

  bool SerializationToolbox::ParseBoolean(bool& result,
                                          const std::string& value)
  {
    if (value == "0" ||
        value == "false")
    {
      result = false;
      return true;
    }
    else if (value == "1" ||
             value == "true")
    {
      result = true;
      return true;
    }
    else
    {
      return false;
    }
  }
}
