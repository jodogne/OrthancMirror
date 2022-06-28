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


#include "../PrecompiledHeaders.h"
#include "DicomValue.h"

#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  DicomValue::DicomValue() :
    type_(Type_Null)
  {
  }


  DicomValue::DicomValue(const DicomValue& other) :
    type_(other.type_),
    content_(other.content_),
    sequenceJson_(other.sequenceJson_)
  {
  }


  DicomValue::DicomValue(const std::string& content,
                         bool isBinary) :
    type_(isBinary ? Type_Binary : Type_String),
    content_(content)
  {
  }
  
  
  DicomValue::DicomValue(const char* data,
                         size_t size,
                         bool isBinary) :
    type_(isBinary ? Type_Binary : Type_String)
  {
    content_.assign(data, size);
  }
    
  DicomValue::DicomValue(const Json::Value& value) :
    type_(Type_SequenceAsJson),
    sequenceJson_(value)
  {
  }
  
  const std::string& DicomValue::GetContent() const
  {
    if (type_ == Type_Null || type_ == Type_SequenceAsJson)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return content_;
    }
  }

  const Json::Value& DicomValue::GetSequenceContent() const
  {
    if (type_ != Type_SequenceAsJson)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return sequenceJson_;
    }
  }


  bool DicomValue::IsNull() const
  {
    return type_ == Type_Null;
  }

  bool DicomValue::IsBinary() const
  {
    return type_ == Type_Binary;
  }

  bool DicomValue::IsString() const
  {
    return type_ == Type_String;
  }

  bool DicomValue::IsSequence() const
  {
    return type_ == Type_SequenceAsJson;
  }

  DicomValue* DicomValue::Clone() const
  {
    return new DicomValue(*this);
  }

  
#if ORTHANC_ENABLE_BASE64 == 1
  void DicomValue::FormatDataUriScheme(std::string& target,
                                       const std::string& mime) const
  {
    Toolbox::EncodeBase64(target, GetContent());
    target.insert(0, "data:" + mime + ";base64,");
  }

  void DicomValue::FormatDataUriScheme(std::string& target) const
  {
    FormatDataUriScheme(target, MIME_BINARY);
  }
#endif

  bool DicomValue::ParseInteger32(int32_t& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseInteger32(result, GetContent());
    }
  }

  bool DicomValue::ParseInteger64(int64_t& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseInteger64(result, GetContent());
    }
  }

  bool DicomValue::ParseUnsignedInteger32(uint32_t& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseUnsignedInteger32(result, GetContent());
    }
  }

  bool DicomValue::ParseUnsignedInteger64(uint64_t& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseUnsignedInteger64(result, GetContent());
    }
  }

  bool DicomValue::ParseFloat(float& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseFloat(result, GetContent());
    }
  }

  bool DicomValue::ParseDouble(double& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseDouble(result, GetContent());
    }
  }

  bool DicomValue::ParseFirstFloat(float& result) const
  {
    if (!IsString())
    {
      return false;
    }
    else
    {
      return SerializationToolbox::ParseFirstFloat(result, GetContent());
    }
  }

  bool DicomValue::ParseFirstUnsignedInteger(unsigned int& result) const
  {
    uint64_t value;

    if (!IsString())
    {
      return false;
    }
    else if (SerializationToolbox::ParseFirstUnsignedInteger64(value, GetContent()))
    {
      result = static_cast<unsigned int>(value);
      return (static_cast<uint64_t>(result) == value);   // Check no overflow
    }
    else
    {
      return false;
    }
  }

  bool DicomValue::CopyToString(std::string& result,
                                bool allowBinary) const
  {
    if (IsNull())
    {
      return false;
    }
    else if (IsSequence())
    {
      return false;
    }
    else if (IsBinary() && !allowBinary)
    {
      return false;
    }
    else
    {
      result.assign(content_);
      return true;
    }
  }    


  static const char* KEY_TYPE = "Type";
  static const char* KEY_CONTENT = "Content";
  
  void DicomValue::Serialize(Json::Value& target) const
  {
    target = Json::objectValue;

    switch (type_)
    {
      case Type_Null:
        target[KEY_TYPE] = "Null";
        break;

      case Type_String:
        target[KEY_TYPE] = "String";
        target[KEY_CONTENT] = content_;
        break;

      case Type_Binary:
      {
        target[KEY_TYPE] = "Binary";

        std::string base64;
        Toolbox::EncodeBase64(base64, content_);
        target[KEY_CONTENT] = base64;
        break;
      }

      case Type_SequenceAsJson:
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  void DicomValue::Unserialize(const Json::Value& source)
  {
    std::string type = SerializationToolbox::ReadString(source, KEY_TYPE);

    if (type == "Null")
    {
      type_ = Type_Null;
      content_.clear();
    }
    else if (type == "String")
    {
      type_ = Type_String;
      content_ = SerializationToolbox::ReadString(source, KEY_CONTENT);
    }
    else if (type == "Binary")
    {
      type_ = Type_Binary;

      const std::string base64 =SerializationToolbox::ReadString(source, KEY_CONTENT);
      Toolbox::DecodeBase64(content_, base64);
    }
    else if (type == "Sequence")
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
}
