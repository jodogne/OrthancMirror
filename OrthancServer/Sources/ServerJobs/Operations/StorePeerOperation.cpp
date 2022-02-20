/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../../PrecompiledHeadersServer.h"
#include "StorePeerOperation.h"

#include "DicomInstanceOperationValue.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/HttpClient.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"

namespace Orthanc
{
  void StorePeerOperation::Apply(JobOperationValues& outputs,
                                 const IJobOperationValue& input)
  {
    // Configure the HTTP client
    HttpClient client(peer_, "instances");
    client.SetMethod(HttpMethod_Post);

    if (input.GetType() != IJobOperationValue::Type_DicomInstance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    const DicomInstanceOperationValue& instance =
      dynamic_cast<const DicomInstanceOperationValue&>(input);

    LOG(INFO) << "Lua: Sending instance " << instance.GetId() << " to Orthanc peer \"" 
              << peer_.GetUrl() << "\"";

    try
    {
      // Lifetime of "body" must exceed the call to "client.Apply()" because of "SetExternalBody()"
      std::string body;
      instance.ReadDicom(body);

      client.SetExternalBody(body);  // Avoids a memcpy()
      
      std::string answer;
      if (!client.Apply(answer))
      {
        LOG(ERROR) << "Lua: Unable to send instance " << instance.GetId()
                   << " to Orthanc peer \"" << peer_.GetUrl();
      }
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: Unable to send instance " << instance.GetId()
                 << " to Orthanc peer \"" << peer_.GetUrl() << "\": " << e.What();
    }

    outputs.Append(input.Clone());
  }

  
  void StorePeerOperation::Serialize(Json::Value& result) const
  {
    result = Json::objectValue;
    result["Type"] = "StorePeer";
    peer_.Serialize(result["Peer"], 
                    true /* force advanced format */,
                    true /* include passwords */);
  }


  StorePeerOperation::StorePeerOperation(const Json::Value& serialized)
  {
    if (SerializationToolbox::ReadString(serialized, "Type") != "StorePeer" ||
        !serialized.isMember("Peer"))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    peer_ = WebServiceParameters(serialized["Peer"]);
  }
}
