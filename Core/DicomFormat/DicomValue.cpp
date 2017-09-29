/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017 Osimis, Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomValue.h"

#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  DicomValue::DicomValue(const DicomValue& other) : 
    type_(other.type_),
    content_(other.content_)
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
    
  
  const std::string& DicomValue::GetContent() const
  {
    if (type_ == Type_Null)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return content_;
    }
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
#endif


  template <typename T,
            bool allowSigned>
  static bool ParseValue(T& result,
                         const DicomValue& source)
  {
    if (source.IsBinary() ||
        source.IsNull())
    {
      return false;
    }
    
    try
    {
      std::string value = Toolbox::StripSpaces(source.GetContent());
      if (value.empty())
      {
        return false;
      }

      if (!allowSigned &&
          value[0] == '-')
      {
        return false;
      }
      
      result = boost::lexical_cast<T>(value);
      return true;
    }
    catch (boost::bad_lexical_cast&)
    {
      return false;
    }
  }

  bool DicomValue::ParseInteger32(int32_t& result) const
  {
    int64_t tmp;
    if (ParseValue<int64_t, true>(tmp, *this))
    {
      result = static_cast<int32_t>(tmp);
      return (tmp == static_cast<int64_t>(result));  // Check no overflow occurs
    }
    else
    {
      return false;
    }
  }

  bool DicomValue::ParseInteger64(int64_t& result) const
  {
    return ParseValue<int64_t, true>(result, *this);
  }

  bool DicomValue::ParseUnsignedInteger32(uint32_t& result) const
  {
    uint64_t tmp;
    if (ParseValue<uint64_t, false>(tmp, *this))
    {
      result = static_cast<uint32_t>(tmp);
      return (tmp == static_cast<uint64_t>(result));  // Check no overflow occurs
    }
    else
    {
      return false;
    }
  }

  bool DicomValue::ParseUnsignedInteger64(uint64_t& result) const
  {
    return ParseValue<uint64_t, false>(result, *this);
  }

  bool DicomValue::ParseFloat(float& result) const
  {
    return ParseValue<float, true>(result, *this);
  }

  bool DicomValue::ParseDouble(double& result) const
  {
    return ParseValue<double, true>(result, *this);
  }

  bool DicomValue::CopyToString(std::string& result,
                                bool allowBinary) const
  {
    if (IsNull())
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
}
