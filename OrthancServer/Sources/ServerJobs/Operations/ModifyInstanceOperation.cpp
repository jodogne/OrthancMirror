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
#include "ModifyInstanceOperation.h"

#include "DicomInstanceOperationValue.h"
#include "../../ServerContext.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"

namespace Orthanc
{
  ModifyInstanceOperation::ModifyInstanceOperation(ServerContext& context,
                                                   RequestOrigin origin,
                                                   DicomModification* modification) :
    context_(context),
    origin_(origin),
    modification_(modification)
  {
    if (modification == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    modification_->SetAllowManualIdentifiers(true);

    if (modification_->IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      modification_->SetLevel(ResourceType_Patient);
    }
    else if (modification_->IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      modification_->SetLevel(ResourceType_Study);
    }
    else if (modification_->IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      modification_->SetLevel(ResourceType_Series);
    }
    else
    {
      modification_->SetLevel(ResourceType_Instance);
    }

    if (origin_ != RequestOrigin_Lua)
    {
      // TODO If issued from HTTP, "remoteIp" and "username" must be provided
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  void ModifyInstanceOperation::Apply(JobOperationValues& outputs,
                                      const IJobOperationValue& input)
  {
    if (input.GetType() != IJobOperationValue::Type_DicomInstance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    const DicomInstanceOperationValue& instance =
      dynamic_cast<const DicomInstanceOperationValue&>(input);

    LOG(INFO) << "Lua: Modifying instance " << instance.GetId();

    std::unique_ptr<ParsedDicomFile> modified;
    
    {
      ServerContext::DicomCacheLocker lock(context_, instance.GetId());
      modified.reset(lock.GetDicom().Clone(true));
    }

    try
    {
      modification_->Apply(*modified);

      std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(*modified));
      assert(origin_ == RequestOrigin_Lua);
      toStore->SetOrigin(DicomInstanceOrigin::FromLua());

      // TODO other metadata
      toStore->AddMetadata(ResourceType_Instance, MetadataType_ModifiedFrom, instance.GetId());

      std::string modifiedId;
      context_.Store(modifiedId, *toStore, StoreInstanceMode_Default);

      // Only chain with other commands if this command succeeds
      outputs.Append(new DicomInstanceOperationValue(instance.GetServerContext(), modifiedId));
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: Unable to modify instance " << instance.GetId()
                 << ": " << e.What();
    }
  }


  void ModifyInstanceOperation::Serialize(Json::Value& target) const
  {
    target = Json::objectValue;
    target["Type"] = "ModifyInstance";
    target["Origin"] = EnumerationToString(origin_);
    modification_->Serialize(target["Modification"]);
  }


  ModifyInstanceOperation::ModifyInstanceOperation(ServerContext& context,
                                                   const Json::Value& serialized) :
    context_(context)
  {
    if (SerializationToolbox::ReadString(serialized, "Type") != "ModifyInstance" ||
        !serialized.isMember("Modification"))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    origin_ = StringToRequestOrigin(SerializationToolbox::ReadString(serialized, "Origin"));

    modification_.reset(new DicomModification(serialized["Modification"]));
  }
}

