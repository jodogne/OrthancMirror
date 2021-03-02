/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
