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


#include "PrecompiledHeaders.h"
#include "SerializationToolbox.h"

#include "OrthancException.h"

#if ORTHANC_ENABLE_DCMTK == 1
#  include "DicomParsing/FromDcmtkBridge.h"
#endif

namespace Orthanc
{
  namespace SerializationToolbox
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

    
    std::string ReadString(const Json::Value& value,
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


    int ReadInteger(const Json::Value& value,
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


    unsigned int ReadUnsignedInteger(const Json::Value& value,
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


    bool ReadBoolean(const Json::Value& value,
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

  
    void ReadArrayOfStrings(std::vector<std::string>& target,
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


    void ReadListOfStrings(std::list<std::string>& target,
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
  

    void ReadSetOfStrings(std::set<std::string>& target,
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


    void ReadSetOfTags(std::set<DicomTag>& target,
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


    void ReadMapOfStrings(std::map<std::string, std::string>& target,
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


    void ReadMapOfTags(std::map<DicomTag, std::string>& target,
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


    void WriteArrayOfStrings(Json::Value& target,
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


    void WriteListOfStrings(Json::Value& target,
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


    void WriteSetOfStrings(Json::Value& target,
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


    void WriteSetOfTags(Json::Value& target,
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


    void WriteMapOfStrings(Json::Value& target,
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


    void WriteMapOfTags(Json::Value& target,
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
  }
}
