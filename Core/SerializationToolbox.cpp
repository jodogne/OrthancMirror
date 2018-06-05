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


#include "PrecompiledHeaders.h"
#include "SerializationToolbox.h"

#include "OrthancException.h"

namespace Orthanc
{
  namespace SerializationToolbox
  {
    std::string ReadString(const Json::Value& value,
                           const std::string& field)
    {
      if (value.type() != Json::objectValue ||
          !value.isMember(field.c_str()) ||
          value[field.c_str()].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
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
        throw OrthancException(ErrorCode_BadFileFormat);
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
        throw OrthancException(ErrorCode_BadFileFormat);
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
        throw OrthancException(ErrorCode_BadFileFormat);
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
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      target.clear();
      target.resize(value.size());

      const Json::Value arr = value[field.c_str()];
    
      for (Json::Value::ArrayIndex i = 0; i < arr.size(); i++)
      {
        if (arr[i].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);        
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


    void WriteArrayOfStrings(Json::Value& target,
                             const std::vector<std::string>& values,
                             const std::string& field)
    {
      if (target.type() != Json::objectValue ||
          target.isMember(field.c_str()))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      Json::Value tmp;

      tmp = Json::arrayValue;
      for (size_t i = 0; i < values.size(); i++)
      {
        tmp.append(values[i]);
      }

      target[field] = tmp;
    }


    void WriteSetOfStrings(Json::Value& target,
                           const std::set<std::string>& values,
                           const std::string& field)
    {
      Json::Value v = Json::arrayValue;

      for (std::set<std::string>::const_iterator it = values.begin();
           it != values.end(); ++it)
      {
        v.append(*it);
      }
      
      target[field] = v;
    }
  }
}
