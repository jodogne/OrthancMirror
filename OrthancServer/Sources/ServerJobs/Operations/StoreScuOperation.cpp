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
#include "StoreScuOperation.h"

#include "DicomInstanceOperationValue.h"
#include "../../ServerContext.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"

namespace Orthanc
{
  void StoreScuOperation::Apply(JobOperationValues& outputs,
                                const IJobOperationValue& input)
  {
    TimeoutDicomConnectionManager::Lock lock(connectionManager_, localAet_, modality_);
    
    if (input.GetType() != IJobOperationValue::Type_DicomInstance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    const DicomInstanceOperationValue& instance =
      dynamic_cast<const DicomInstanceOperationValue&>(input);

    LOG(INFO) << "Lua: Sending instance " << instance.GetId() << " to modality \"" 
              << modality_.GetApplicationEntityTitle() << "\"";

    try
    {
      std::string dicom;
      instance.ReadDicom(dicom);

      std::string sopClassUid, sopInstanceUid;  // Unused
      context_.StoreWithTranscoding(sopClassUid, sopInstanceUid, lock.GetConnection(), dicom,
                                    false /* Not a C-MOVE */, "", 0);
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: Unable to send instance " << instance.GetId() << " to modality \"" 
                 << modality_.GetApplicationEntityTitle() << "\": " << e.What();
    }

    outputs.Append(input.Clone());
  }

  
  void StoreScuOperation::Serialize(Json::Value& result) const
  {
    result = Json::objectValue;
    result["Type"] = "StoreScu";
    result["LocalAET"] = localAet_;
    modality_.Serialize(result["Modality"], true /* force advanced format */);
  }


  StoreScuOperation::StoreScuOperation(ServerContext& context,
                                       TimeoutDicomConnectionManager& connectionManager,
                                       const Json::Value& serialized) :
    context_(context),
    connectionManager_(connectionManager)
  {
    if (SerializationToolbox::ReadString(serialized, "Type") != "StoreScu" ||
        !serialized.isMember("LocalAET"))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    localAet_ = SerializationToolbox::ReadString(serialized, "LocalAET");
    modality_ = RemoteModalityParameters(serialized["Modality"]);
  }
}
