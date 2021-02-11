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


#include "../PrecompiledHeadersServer.h"
#include "ResourceModificationJob.h"

#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <cassert>

namespace Orthanc
{
  class ResourceModificationJob::Output : public boost::noncopyable
  {
  private:
    ResourceType  level_;
    bool          isFirst_;
    std::string   id_;
    std::string   patientId_;

  public:
    explicit Output(ResourceType level) :
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

    ResourceType GetLevel() const
    {
      return level_;
    }
    

    void Update(DicomInstanceHasher& hasher)
    {
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


    bool Format(Json::Value& target)
    {
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

  
    bool GetIdentifier(std::string& id)
    {
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
  };
    



  bool ResourceModificationJob::HandleInstance(const std::string& instance)
  {
    if (modification_.get() == NULL ||
        output_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "No modification was provided for this job");
    }

      
    LOG(INFO) << "Modifying instance in a job: " << instance;


    /**
     * Retrieve the original instance from the DICOM cache.
     **/
    
    std::unique_ptr<DicomInstanceHasher> originalHasher;
    std::unique_ptr<ParsedDicomFile> modified;

    try
    {
      ServerContext::DicomCacheLocker locker(GetContext(), instance);
      ParsedDicomFile& original = locker.GetDicom();

      originalHasher.reset(new DicomInstanceHasher(original.GetHasher()));
      modified.reset(original.Clone(true));
    }
    catch (OrthancException& e)
    {
      LOG(WARNING) << "An error occurred while executing a Modification job on instance " << instance << ": " << e.GetDetails();
      return false;
    }


    /**
     * Compute the resulting DICOM instance.
     **/

    modification_->Apply(*modified);

    const std::string modifiedUid = IDicomTranscoder::GetSopInstanceUid(modified->GetDcmtkObject());
    
    if (transcode_)
    {
      std::set<DicomTransferSyntax> syntaxes;
      syntaxes.insert(transferSyntax_);

      IDicomTranscoder::DicomImage source;
      source.AcquireParsed(*modified);  // "modified" is invalid below this point
      
      IDicomTranscoder::DicomImage transcoded;
      if (GetContext().Transcode(transcoded, source, syntaxes, true))
      {
        modified.reset(transcoded.ReleaseAsParsedDicomFile());

        // Fix the SOP instance UID in order the preserve the
        // references between instance UIDs in the DICOM hierarchy
        // (the UID might have changed in the case of lossy transcoding)
        if (modified.get() == NULL ||
            modified->GetDcmtkObject().getDataset() == NULL ||
            !modified->GetDcmtkObject().getDataset()->putAndInsertString(
              DCM_SOPInstanceUID, modifiedUid.c_str(), OFTrue /* replace */).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        LOG(WARNING) << "Cannot transcode instance, keeping original transfer syntax: " << instance;
        modified.reset(source.ReleaseAsParsedDicomFile());
      }
    }

    assert(modifiedUid == IDicomTranscoder::GetSopInstanceUid(modified->GetDcmtkObject()));

    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(*modified));
    toStore->SetOrigin(origin_);


    /**
     * Prepare the metadata information to associate with the
     * resulting DICOM instance (AnonymizedFrom/ModifiedFrom).
     **/

    DicomInstanceHasher modifiedHasher = modified->GetHasher();
      
    MetadataType metadataType = (isAnonymization_ ?
                                 MetadataType_AnonymizedFrom :
                                 MetadataType_ModifiedFrom);

    if (originalHasher->HashSeries() != modifiedHasher.HashSeries())
    {
      toStore->AddMetadata(ResourceType_Series, metadataType, originalHasher->HashSeries());
    }

    if (originalHasher->HashStudy() != modifiedHasher.HashStudy())
    {
      toStore->AddMetadata(ResourceType_Study, metadataType, originalHasher->HashStudy());
    }

    if (originalHasher->HashPatient() != modifiedHasher.HashPatient())
    {
      toStore->AddMetadata(ResourceType_Patient, metadataType, originalHasher->HashPatient());
    }

    assert(instance == originalHasher->HashInstance());
    toStore->AddMetadata(ResourceType_Instance, metadataType, instance);


    /**
     * Store the resulting DICOM instance into the Orthanc store.
     **/

    std::string modifiedInstance;
    if (GetContext().Store(modifiedInstance, *toStore,
                           StoreInstanceMode_Default) != StoreStatus_Success)
    {
      throw OrthancException(ErrorCode_CannotStoreInstance,
                             "Error while storing a modified instance " + instance);
    }

    /**
     * The assertion below will fail if automated transcoding to a
     * lossy transfer syntax is enabled in the Orthanc core, and if
     * the source instance is not in this transfer syntax.
     **/
    // assert(modifiedInstance == modifiedHasher.HashInstance());

    output_->Update(modifiedHasher);

    return true;
  }


  ResourceModificationJob::ResourceModificationJob(ServerContext& context) :
    CleaningInstancesJob(context, true /* by default, keep source */),
    modification_(new DicomModification),
    isAnonymization_(false),
    transcode_(false),
    transferSyntax_(DicomTransferSyntax_LittleEndianExplicit)  // dummy initialization
  {
  }


  void ResourceModificationJob::SetModification(DicomModification* modification,
                                                ResourceType level,
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
      output_.reset(new Output(level));
      isAnonymization_ = isAnonymization;
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
    SetOrigin(DicomInstanceOrigin::FromRest(call));
  }


  const DicomModification& ResourceModificationJob::GetModification() const
  {
    if (modification_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *modification_;
    }
  }


  DicomTransferSyntax ResourceModificationJob::GetTransferSyntax() const
  {
    if (transcode_)
    {
      return transferSyntax_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  

  void ResourceModificationJob::SetTranscode(DicomTransferSyntax syntax)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      transcode_ = true;
      transferSyntax_ = syntax;
    }    
  }


  void ResourceModificationJob::SetTranscode(const std::string& transferSyntaxUid)
  {
    DicomTransferSyntax s;
    if (LookupTransferSyntax(s, transferSyntaxUid))
    {
      SetTranscode(s);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Unknown transfer syntax UID: " + transferSyntaxUid);
    }
  }


  void ResourceModificationJob::ClearTranscode()
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      transcode_ = false;
    }
  }


  void ResourceModificationJob::GetPublicContent(Json::Value& value)
  {
    CleaningInstancesJob::GetPublicContent(value);

    value["IsAnonymization"] = isAnonymization_;

    if (output_.get() != NULL)
    {
      output_->Format(value);
    }

    if (transcode_)
    {
      value["Transcode"] = GetTransferSyntaxUid(transferSyntax_);
    }
  }


  static const char* MODIFICATION = "Modification";
  static const char* ORIGIN = "Origin";
  static const char* IS_ANONYMIZATION = "IsAnonymization";
  static const char* TRANSCODE = "Transcode";
  

  ResourceModificationJob::ResourceModificationJob(ServerContext& context,
                                                   const Json::Value& serialized) :
    CleaningInstancesJob(context, serialized, true /* by default, keep source */),
    transferSyntax_(DicomTransferSyntax_LittleEndianExplicit)  // dummy initialization
  {
    assert(serialized.type() == Json::objectValue);

    isAnonymization_ = SerializationToolbox::ReadBoolean(serialized, IS_ANONYMIZATION);
    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);
    modification_.reset(new DicomModification(serialized[MODIFICATION]));

    if (serialized.isMember(TRANSCODE))
    {
      SetTranscode(SerializationToolbox::ReadString(serialized, TRANSCODE));
    }
    else
    {
      transcode_ = false;
    }
  }
  
  bool ResourceModificationJob::Serialize(Json::Value& value)
  {
    if (!CleaningInstancesJob::Serialize(value))
    {
      return false;
    }
    else
    {
      assert(value.type() == Json::objectValue);
      
      value[IS_ANONYMIZATION] = isAnonymization_;

      if (transcode_)
      {
        value[TRANSCODE] = GetTransferSyntaxUid(transferSyntax_);
      }
      
      origin_.Serialize(value[ORIGIN]);
      
      Json::Value tmp;
      modification_->Serialize(tmp);
      value[MODIFICATION] = tmp;

      return true;
    }
  }
}
