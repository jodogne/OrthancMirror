/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
  static void FormatResource(Json::Value& target,
                             ResourceType level,
                             const std::string& id)
  {
    target["Type"] = EnumerationToString(level);
    target["ID"] = id;
    target["Path"] = GetBasePath(level, id);
  }
  
  class ResourceModificationJob::SingleOutput : public IOutput
  {
  private:
    ResourceType  level_;
    bool          isFirst_;
    std::string   id_;
    std::string   patientId_;

  public:
    explicit SingleOutput(ResourceType level) :
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

    virtual void Update(DicomInstanceHasher& hasher) ORTHANC_OVERRIDE
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

    virtual void Format(Json::Value& target) const ORTHANC_OVERRIDE
    {
      assert(target.type() == Json::objectValue);

      if (!isFirst_)
      {
        FormatResource(target, level_, id_);
        target["PatientID"] = patientId_;
      }
    }

    virtual bool IsSingleResource() const ORTHANC_OVERRIDE
    {
      return true;
    }

    ResourceType GetLevel() const
    {
      return level_;
    }
  };
    

  class ResourceModificationJob::MultipleOutputs : public IOutput
  {
  private:
    static void FormatResources(Json::Value& target,
                                ResourceType level,
                                const std::set<std::string>& resources)
    {
      assert(target.type() == Json::arrayValue);

      for (std::set<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        Json::Value item = Json::objectValue;
        FormatResource(item, level, *it);
        target.append(item);        
      }
    }
    
    std::set<std::string>  instances_;
    std::set<std::string>  series_;
    std::set<std::string>  studies_;
    std::set<std::string>  patients_;

  public:
    virtual void Update(DicomInstanceHasher& hasher) ORTHANC_OVERRIDE
    {
      instances_.insert(hasher.HashInstance());
      series_.insert(hasher.HashSeries());
      studies_.insert(hasher.HashStudy());
      patients_.insert(hasher.HashPatient());
    }

    virtual void Format(Json::Value& target) const ORTHANC_OVERRIDE
    {
      assert(target.type() == Json::objectValue);
      Json::Value resources = Json::arrayValue;
      FormatResources(resources, ResourceType_Instance, instances_);
      FormatResources(resources, ResourceType_Series, series_);
      FormatResources(resources, ResourceType_Study, studies_);
      FormatResources(resources, ResourceType_Patient, patients_);
      target["Resources"] = resources;
    }

    virtual bool IsSingleResource() const ORTHANC_OVERRIDE
    {
      return false;
    }
  };

  // Reset is called when resubmitting a failed job
  void ResourceModificationJob::Reset()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    // TODO: cleanup the instances that have been generated during the previous run
    modifiedSeries_.clear();
    instancesToReconstruct_.clear();

    ThreadedSetOfInstancesJob::Reset();
  }

  void ResourceModificationJob::PostProcessInstances()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    // reconstruct the parents MainDicomTags in case one of them has changed
    if (instancesToReconstruct_.size() > 0)
    {
      for (std::set<std::string>::const_iterator it = instancesToReconstruct_.begin(); it != instancesToReconstruct_.end(); ++it)
      {
        ServerContext::DicomCacheLocker locker(GetContext(), *it);
        ParsedDicomFile& modifiedDicom = locker.GetDicom();

        GetContext().GetIndex().ReconstructInstance(modifiedDicom);
      }
    }
    
  }

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
      throw;
    }


    /**
     * Compute the resulting DICOM instance.
     **/

    {
      boost::recursive_mutex::scoped_lock lock(mutex_);  // DicomModification object is not thread safe, we must protect it from here

      modification_->Apply(*modified);
    }

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
    toStore->SetSkipIngestTranscoding(transcode_); // do not apply IngestTranscoding if you have forced the transfer syntax during the modification/anonymization


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
    ServerContext::StoreResult result = GetContext().Store(modifiedInstance, *toStore, StoreInstanceMode_Default);
    if (result.GetStatus() != StoreStatus_Success && result.GetStatus() != StoreStatus_AlreadyStored) // when retrying a job, we might save the same data again
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

    {
      boost::recursive_mutex::scoped_lock lock(outputMutex_);

      output_->Update(modifiedHasher);
      if (modifiedSeries_.find(modifiedHasher.HashSeries()) == modifiedSeries_.end())
      {
        modifiedSeries_.insert(modifiedHasher.HashSeries());
        // add an instance to reconstruct for each series
        instancesToReconstruct_.insert(modifiedHasher.HashInstance());
      }
      
    }

    return true;
  }


  ResourceModificationJob::ResourceModificationJob(ServerContext& context, unsigned int workersCount) :
    ThreadedSetOfInstancesJob(context, true /* post processing step */, true /* by default, keep source */, workersCount),
    isAnonymization_(false),
    transcode_(false),
    transferSyntax_(DicomTransferSyntax_LittleEndianExplicit)  // dummy initialization
  {
  }


  void ResourceModificationJob::SetSingleResourceModification(DicomModification* modification,
                                                              ResourceType outputLevel,
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
      output_.reset(new SingleOutput(outputLevel));
      isAnonymization_ = isAnonymization;
    }
  }


  void ResourceModificationJob::SetMultipleResourcesModification(DicomModification* modification,
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
      output_.reset(new MultipleOutputs);
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


#if ORTHANC_BUILD_UNIT_TESTS == 1
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
#endif

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


  bool ResourceModificationJob::IsSingleResourceModification() const
  {
    if (modification_.get() == NULL)
    {
      assert(output_.get() == NULL);
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      boost::recursive_mutex::scoped_lock lock(outputMutex_);

      assert(output_.get() != NULL);
      return output_->IsSingleResource();
    }
  }
  

  ResourceType ResourceModificationJob::GetOutputLevel() const
  {
    boost::recursive_mutex::scoped_lock lock(outputMutex_);

    if (IsSingleResourceModification())
    {
      assert(modification_.get() != NULL &&
             output_.get() != NULL);
      return dynamic_cast<const SingleOutput&>(*output_).GetLevel();
    }
    else
    {
      // Not applicable if multiple resources
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void ResourceModificationJob::GetPublicContent(Json::Value& value)
  {
    boost::recursive_mutex::scoped_lock lock(outputMutex_);

    ThreadedSetOfInstancesJob::GetPublicContent(value);

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
  static const char* OUTPUT_LEVEL = "OutputLevel";
  static const char* IS_SINGLE_RESOURCE = "IsSingleResource";
  

  ResourceModificationJob::ResourceModificationJob(ServerContext& context,
                                                   const Json::Value& serialized) :
    ThreadedSetOfInstancesJob(context, serialized, false /* no post processing step */, true /* by default, keep source */),
    transferSyntax_(DicomTransferSyntax_LittleEndianExplicit)  // dummy initialization
  {
    assert(serialized.type() == Json::objectValue);

    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);

    if (serialized.isMember(TRANSCODE))
    {
      SetTranscode(SerializationToolbox::ReadString(serialized, TRANSCODE));
    }
    else
    {
      transcode_ = false;
    }

    bool isSingleResource;
    if (serialized.isMember(IS_SINGLE_RESOURCE))
    {
      isSingleResource = SerializationToolbox::ReadBoolean(serialized, IS_SINGLE_RESOURCE);
    }
    else
    {
      isSingleResource = true;  // Backward compatibility with Orthanc <= 1.9.3
    }

    bool isAnonymization = SerializationToolbox::ReadBoolean(serialized, IS_ANONYMIZATION);
    std::unique_ptr<DicomModification> modification(new DicomModification(serialized[MODIFICATION]));

    if (isSingleResource)
    {
      ResourceType outputLevel;
      
      if (serialized.isMember(OUTPUT_LEVEL))
      {
        // New in Orthanc 1.9.4. This fixes an *incorrect* behavior in
        // Orthanc <= 1.9.3, in which "outputLevel" would be set to
        // "modification->GetLevel()"
        outputLevel = StringToResourceType(SerializationToolbox::ReadString(serialized, OUTPUT_LEVEL).c_str());
      }
      else
      {
        // Use the buggy convention from Orthanc <= 1.9.3 (which is
        // the only thing we have at hand)
        outputLevel = modification->GetLevel();

        if (outputLevel == ResourceType_Instance)
        {
          // This should never happen, but as "SingleOutput" doesn't
          // support instance-level anonymization, don't take any risk
          // and choose an arbitrary output level
          outputLevel = ResourceType_Patient;
        }
      }
      
      SetSingleResourceModification(modification.release(), outputLevel, isAnonymization);
    }
    else
    {
      // New in Orthanc 1.9.4
      SetMultipleResourcesModification(modification.release(), isAnonymization);
    }
  }
  
  bool ResourceModificationJob::Serialize(Json::Value& value)
  {
    if (modification_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!ThreadedSetOfInstancesJob::Serialize(value))
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

      {
        boost::recursive_mutex::scoped_lock lock(mutex_);  // DicomModification object is not thread safe, we must protect it from here
  
        modification_->Serialize(tmp);
      }

      value[MODIFICATION] = tmp;

      // New in Orthanc 1.9.4
      value[IS_SINGLE_RESOURCE] = IsSingleResourceModification();
      if (IsSingleResourceModification())
      {
        value[OUTPUT_LEVEL] = EnumerationToString(GetOutputLevel());
      }
      
      return true;
    }
  }

  void ResourceModificationJob::PerformSanityChecks()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);  // because we access the parentResources_

    std::set<DicomTag> emptyRequestedTags;

    if (modification_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    bool replacePatientMainDicomTags = false;

    ResourceType modificationLevel = modification_->GetLevel();
    std::set<DicomTag> replacedTags;
    modification_->GetReplacedTags(replacedTags);

    for (std::set<DicomTag>::const_iterator it = replacedTags.begin(); it != replacedTags.end(); ++it)
    {
      replacePatientMainDicomTags |= DicomMap::IsMainDicomTag(*it, ResourceType_Patient);
    }

    if ((modificationLevel == ResourceType_Study ||
         modificationLevel == ResourceType_Patient) &&
        !modification_->IsReplaced(DICOM_TAG_PATIENT_ID) &&
        modification_->IsKept(DICOM_TAG_STUDY_INSTANCE_UID) &&
        modification_->IsKept(DICOM_TAG_SERIES_INSTANCE_UID) &&
        modification_->IsKept(DICOM_TAG_SOP_INSTANCE_UID))
    {
      // if we keep the SOPInstanceUID, it very likely means that we are modifying existing resources 'in place'

      // we must make sure we do not delete them at the end of the job
      if (!IsKeepSource()) // note: we can refine this criteria -> this is valid only if all DicomUIDs are kept identical (but this can happen through Keep or Replace options)
      {
        throw OrthancException(ErrorCode_BadRequest,
                              "When keeping StudyInstanceUID, SeriesInstanceUID and SOPInstanceUID tag, you must set KeepSource to true to avoid deleting the modified files at the end of the process");
      }

      // and we must make sure that we overwite them with the modified resources
      if (IsKeepSource() && !GetContext().IsOverwriteInstances())
      {
        throw OrthancException(ErrorCode_BadRequest,
                              "When keeping StudyInstanceUID, SeriesInstanceUID and SOPInstanceUID tag, you must have the 'OverwriteInstances' Orthanc configuration set to true in order to replace the modified resources");
      }
    }

    if (modificationLevel == ResourceType_Study && replacePatientMainDicomTags)
    {
      for (std::set<std::string>::const_iterator studyId = parentResources_.begin(); studyId != parentResources_.end(); ++studyId)
      {
        // When modifying a study, you may not modify patient tags as you wish.
        // - If this is the patient's only study, you may modify all patient tags. This could be performed in 2 steps (modify the patient and then, the study) but, 
        //   for many use cases, it's helpful to be able to do it one step (e.g, to modify a name in a study that has just been acquired)
        // - If the patient already has other studies, you may only 'attach' the study to an existing patient by modifying 
        //   all patient tags from the study to match those of the target patient.
        // - Otherwise, you can't modify the patient tags
        
        std::string targetPatientId;
        if (modification_->IsReplaced(DICOM_TAG_PATIENT_ID))
        {
          targetPatientId = modification_->GetReplacementAsString(DICOM_TAG_PATIENT_ID);
        }
        else
        {
          ExpandedResource originalStudy;
          if (GetContext().GetIndex().ExpandResource(originalStudy, *studyId, ResourceType_Study, emptyRequestedTags, ExpandResourceFlags_IncludeMainDicomTags))
          {
            targetPatientId = originalStudy.GetMainDicomTags().GetStringValue(DICOM_TAG_PATIENT_ID, "", false);
          }
          else
          {
            throw OrthancException(ErrorCode_UnknownResource, "Study not found");
          }
        }

        // try to find the targetPatient
        std::vector<std::string> lookupPatientResult;
        GetContext().GetIndex().LookupIdentifierExact(lookupPatientResult, ResourceType_Patient, DICOM_TAG_PATIENT_ID, targetPatientId);

        // if the patient exists, check how many child studies it has.
        if (lookupPatientResult.size() >= 1)
        {
          ExpandedResource targetPatient;
          
          if (GetContext().GetIndex().ExpandResource(targetPatient, lookupPatientResult[0], ResourceType_Patient, emptyRequestedTags, static_cast<ExpandResourceFlags>(ExpandResourceFlags_IncludeMainDicomTags | ExpandResourceFlags_IncludeChildren)))
          {
            const std::list<std::string> childrenIds = targetPatient.childrenIds_;
            bool targetPatientHasOtherStudies = childrenIds.size() > 1;
            if (childrenIds.size() == 1)
            {
              targetPatientHasOtherStudies = std::find(childrenIds.begin(), childrenIds.end(), *studyId) == childrenIds.end();  // if the patient has one study that is not the one being modified
            }

            if (targetPatientHasOtherStudies)
            {
              // this is allowed if all patient replacedTags do match the target patient tags
              DicomMap targetPatientTags;
              targetPatient.GetMainDicomTags().ExtractPatientInformation(targetPatientTags);

              std::set<DicomTag> mainPatientTags;
              DicomMap::GetMainDicomTags(mainPatientTags, ResourceType_Patient);
              
              for (std::set<DicomTag>::const_iterator mainPatientTag = mainPatientTags.begin();
                   mainPatientTag != mainPatientTags.end(); ++mainPatientTag)
              {
                if (targetPatientTags.HasTag(*mainPatientTag) &&
                    (!modification_->IsReplaced(*mainPatientTag) ||
                     modification_->GetReplacementAsString(*mainPatientTag) != targetPatientTags.GetStringValue(*mainPatientTag, "", false)))
                {
                  throw OrthancException(ErrorCode_BadRequest, std::string("Trying to change patient tags in a study.  " 
                    "The Patient already exists and has other studies.  All the 'Replace' tags should match the existing patient main dicom tags "
                    "and you should specify all Patient MainDicomTags in your query.  Try using /patients/../modify instead to modify the patient. Failing tag: ") + mainPatientTag->Format());
                }
                else if (!targetPatientTags.HasTag(*mainPatientTag) && modification_->IsReplaced(*mainPatientTag) )
                {
                  throw OrthancException(ErrorCode_BadRequest, std::string("Trying to change patient tags in a study.  "
                    "The Patient already exists and has other studies.  You are trying to replace a tag that is not defined yet in this patient. " 
                    "Try using /patients/../modify instead to modify the patient. Failing tag: ") + mainPatientTag->Format());
                }
              }
            }
          }
        }
      }      
    }
  }
}
