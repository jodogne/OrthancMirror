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


#include "../PrecompiledHeadersServer.h"
#include "ResourceModificationJob.h"

#include "../../Core/Logging.h"

namespace Orthanc
{
  ResourceModificationJob::Output::Output(ResourceType  level) :
    level_(level),
    isFirst_(true)
  {
    if (level_ != ResourceType_Patient &&
        level_ != ResourceType_Study &&
        level_ != ResourceType_Series)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }            
  }


  void ResourceModificationJob::Output::Update(DicomInstanceHasher& hasher)
  {
    boost::mutex::scoped_lock lock(mutex_);
        
    if (isFirst_)
    {
      switch (level_)
      {
        case ResourceType_Series:
          id_ = hasher.HashSeries();
          break;

        case ResourceType_Study:
          id_ = hasher.HashStudy();
          break;

        case ResourceType_Patient:
          id_ = hasher.HashPatient();
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      patientId_ = hasher.HashPatient();
      isFirst_ = false;
    }
  }


  bool ResourceModificationJob::Output::Format(Json::Value& target)
  {
    boost::mutex::scoped_lock lock(mutex_);
        
    if (isFirst_)
    {
      return false;
    }
    else
    {
      target = Json::objectValue;
      target["Type"] = EnumerationToString(level_);
      target["ID"] = id_;
      target["Path"] = GetBasePath(level_, id_);
      target["PatientID"] = patientId_;
      return true;
    }
  }

  
  bool ResourceModificationJob::Output::GetIdentifier(std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);
        
    if (isFirst_)
    {
      return false;
    }
    else
    {
      id = id_;
      return true;
    }
  }


  bool ResourceModificationJob::HandleInstance(const std::string& instance)
  {
    if (modification_.get() == NULL)
    {
      LOG(ERROR) << "No modification was provided for this job";
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

      
    LOG(INFO) << "Modifying instance in a job: " << instance;

    std::auto_ptr<ServerContext::DicomCacheLocker> locker;

    try
    {
      locker.reset(new ServerContext::DicomCacheLocker(context_, instance));
    }
    catch (OrthancException&)
    {
      LOG(WARNING) << "An instance was removed after the job was issued: " << instance;
      return false;
    }


    ParsedDicomFile& original = locker->GetDicom();
    DicomInstanceHasher originalHasher = original.GetHasher();


    /**
     * Compute the resulting DICOM instance.
     **/

    std::auto_ptr<ParsedDicomFile> modified(original.Clone(true));
    modification_->Apply(*modified);

    DicomInstanceToStore toStore;
    toStore.SetOrigin(origin_);
    toStore.SetParsedDicomFile(*modified);


    /**
     * Prepare the metadata information to associate with the
     * resulting DICOM instance (AnonymizedFrom/ModifiedFrom).
     **/

    DicomInstanceHasher modifiedHasher = modified->GetHasher();
      
    MetadataType metadataType = (isAnonymization_ ?
                                 MetadataType_AnonymizedFrom :
                                 MetadataType_ModifiedFrom);

    if (originalHasher.HashSeries() != modifiedHasher.HashSeries())
    {
      toStore.AddMetadata(ResourceType_Series, metadataType, originalHasher.HashSeries());
    }

    if (originalHasher.HashStudy() != modifiedHasher.HashStudy())
    {
      toStore.AddMetadata(ResourceType_Study, metadataType, originalHasher.HashStudy());
    }

    if (originalHasher.HashPatient() != modifiedHasher.HashPatient())
    {
      toStore.AddMetadata(ResourceType_Patient, metadataType, originalHasher.HashPatient());
    }

    assert(instance == originalHasher.HashInstance());
    toStore.AddMetadata(ResourceType_Instance, metadataType, instance);


    /**
     * Store the resulting DICOM instance into the Orthanc store.
     **/

    std::string modifiedInstance;
    if (context_.Store(modifiedInstance, toStore) != StoreStatus_Success)
    {
      LOG(ERROR) << "Error while storing a modified instance " << instance;
      throw OrthancException(ErrorCode_CannotStoreInstance);
    }

    // Sanity checks in debug mode
    assert(modifiedInstance == modifiedHasher.HashInstance());


    if (output_.get() != NULL)
    {
      output_->Update(modifiedHasher);
    }

    return true;
  }
    

  
  void ResourceModificationJob::SetModification(DicomModification* modification,
                                                bool isAnonymization)
  {
    if (modification == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      modification_.reset(modification);
      isAnonymization_ = isAnonymization;
    }
  }


  void ResourceModificationJob::SetOutput(boost::shared_ptr<Output>& output)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      output_ = output;
    }
  }

  
  void ResourceModificationJob::SetOrigin(const DicomInstanceOrigin& origin)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      origin_ = origin;
    }
  }

  
  void ResourceModificationJob::SetOrigin(const RestApiCall& call)
  {
    DicomInstanceOrigin tmp;
    tmp.SetRestOrigin(call);      
    SetOrigin(tmp);
  }


  void ResourceModificationJob::GetPublicContent(Json::Value& value)
  {
    SetOfInstancesJob::GetPublicContent(value);

    value["IsAnonymization"] = isAnonymization_;
  }

  
  void ResourceModificationJob::Serialize(Json::Value& value)
  {
    SetOfInstancesJob::Serialize(value);

    Json::Value tmp;
    modification_->Serialize(tmp);
    value["Modification"] = tmp;
  }
}
