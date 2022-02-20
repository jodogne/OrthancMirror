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
#include "GenericJobUnserializer.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"

#include "Operations/LogJobOperation.h"
#include "Operations/NullOperationValue.h"
#include "Operations/SequenceOfOperationsJob.h"
#include "Operations/StringOperationValue.h"

namespace Orthanc
{
  IJob* GenericJobUnserializer::UnserializeJob(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

    if (type == "SequenceOfOperations")
    {
      return new SequenceOfOperationsJob(*this, source);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot unserialize job of type: " + type);
    }
  }


  IJobOperation* GenericJobUnserializer::UnserializeOperation(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

    if (type == "Log")
    {
      return new LogJobOperation;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot unserialize operation of type: " + type);
    }
  }


  IJobOperationValue* GenericJobUnserializer::UnserializeValue(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

    if (type == "String")
    {
      return new StringOperationValue(SerializationToolbox::ReadString(source, "Content"));
    }
    else if (type == "Null")
    {
      return new NullOperationValue;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot unserialize value of type: " + type);
    }
  }
}

