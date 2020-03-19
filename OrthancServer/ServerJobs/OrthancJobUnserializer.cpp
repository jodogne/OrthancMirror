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


#include "../PrecompiledHeadersServer.h"
#include "OrthancJobUnserializer.h"

#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/SerializationToolbox.h"
#include "../../Plugins/Engine/OrthancPlugins.h"
#include "../ServerContext.h"

#include "Operations/DeleteResourceOperation.h"
#include "Operations/DicomInstanceOperationValue.h"
#include "Operations/ModifyInstanceOperation.h"
#include "Operations/StorePeerOperation.h"
#include "Operations/StoreScuOperation.h"
#include "Operations/SystemCallOperation.h"

#include "DicomModalityStoreJob.h"
#include "DicomMoveScuJob.h"
#include "MergeStudyJob.h"
#include "OrthancPeerStoreJob.h"
#include "ResourceModificationJob.h"
#include "SplitStudyJob.h"
#include "StorageCommitmentScpJob.h"


namespace Orthanc
{
  IJob* OrthancJobUnserializer::UnserializeJob(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

#if ORTHANC_ENABLE_PLUGINS == 1
    if (context_.HasPlugins())
    {
      std::unique_ptr<IJob> job(context_.GetPlugins().UnserializeJob(type, source));
      if (job.get() != NULL)
      {
        return job.release();
      }
    }
#endif

    if (type == "DicomModalityStore")
    {
      return new DicomModalityStoreJob(context_, source);
    }
    else if (type == "OrthancPeerStore")
    {
      return new OrthancPeerStoreJob(context_, source);
    }
    else if (type == "ResourceModification")
    {
      return new ResourceModificationJob(context_, source);
    }
    else if (type == "MergeStudy")
    {
      return new MergeStudyJob(context_, source);
    }
    else if (type == "SplitStudy")
    {
      return new SplitStudyJob(context_, source);
    }
    else if (type == "DicomMoveScu")
    {
      return new DicomMoveScuJob(context_, source);
    }
    else if (type == "StorageCommitmentScp")
    {
      return new StorageCommitmentScpJob(context_, source);
    }
    else
    {
      return GenericJobUnserializer::UnserializeJob(source);
    }
  }


  IJobOperation* OrthancJobUnserializer::UnserializeOperation(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

    if (type == "DeleteResource")
    {
      return new DeleteResourceOperation(context_);
    }
    else if (type == "ModifyInstance")
    {
      return new ModifyInstanceOperation(context_, source);
    }
    else if (type == "StorePeer")
    {
      return new StorePeerOperation(source);
    }
    else if (type == "StoreScu")
    {
      return new StoreScuOperation(source);
    }
    else if (type == "SystemCall")
    {
      return new SystemCallOperation(source);
    }
    else
    {
      return GenericJobUnserializer::UnserializeOperation(source);
    }
  }


  JobOperationValue* OrthancJobUnserializer::UnserializeValue(const Json::Value& source)
  {
    const std::string type = SerializationToolbox::ReadString(source, "Type");

    if (type == "DicomInstance")
    {
      return new DicomInstanceOperationValue(context_, SerializationToolbox::ReadString(source, "ID"));
    }
    else
    {
      return GenericJobUnserializer::UnserializeValue(source);
    }
  }
}
