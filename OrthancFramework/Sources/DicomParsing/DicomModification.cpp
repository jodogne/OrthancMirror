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
#include "DicomModification.h"

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "FromDcmtkBridge.h"
#include "ITagVisitor.h"

#include <memory>   // For std::unique_ptr


static const std::string ORTHANC_DEIDENTIFICATION_METHOD_2008 =
  "Orthanc " ORTHANC_VERSION " - PS 3.15-2008 Table E.1-1";

static const std::string ORTHANC_DEIDENTIFICATION_METHOD_2017c =
  "Orthanc " ORTHANC_VERSION " - PS 3.15-2017c Table E.1-1 Basic Profile";

static const std::string ORTHANC_DEIDENTIFICATION_METHOD_2021b =
  "Orthanc " ORTHANC_VERSION " - PS 3.15-2021b Table E.1-1 Basic Profile";

namespace Orthanc
{
  namespace
  {
    enum TagOperation
    {
      TagOperation_Keep,
      TagOperation_Remove
    };
  }

  
  DicomModification::DicomTagRange::DicomTagRange(uint16_t groupFrom,
                                                  uint16_t groupTo,
                                                  uint16_t elementFrom,
                                                  uint16_t elementTo) :
    groupFrom_(groupFrom),
    groupTo_(groupTo),
    elementFrom_(elementFrom),
    elementTo_(elementTo)
  {
  }

  
  bool DicomModification::DicomTagRange::Contains(const DicomTag& tag) const
  {
    return (tag.GetGroup() >= groupFrom_ &&
            tag.GetGroup() <= groupTo_ &&
            tag.GetElement() >= elementFrom_ &&
            tag.GetElement() <= elementTo_);
  }


  class DicomModification::RelationshipsVisitor : public ITagVisitor
  {
  private:
    DicomModification&  that_;

    // This method is only applicable to first-level tags
    bool IsManuallyModified(const DicomTag& tag) const
    {
      return (that_.IsCleared(tag) ||
              that_.IsRemoved(tag) ||
              that_.IsReplaced(tag));
    }

    bool IsKeptSequence(const std::vector<DicomTag>& parentTags,
                        const std::vector<size_t>& parentIndexes,
                        const DicomTag& tag)
    {
      for (DicomModification::ListOfPaths::const_iterator
             it = that_.keepSequences_.begin(); it != that_.keepSequences_.end(); ++it)
      {
        if (DicomPath::IsMatch(*it, parentTags, parentIndexes, tag))
        {
          return true;
        }
      }

      return false;
    }

    Action GetDefaultAction(const std::vector<DicomTag>& parentTags,
                            const std::vector<size_t>& parentIndexes,
                            const DicomTag& tag)
    {
      if (parentTags.empty() ||
          !that_.isAnonymization_)
      {
        // Don't interfere with first-level tags or with modification
        return Action_None;
      }
      else if (IsKeptSequence(parentTags, parentIndexes, tag))
      {
        return Action_None;
      }
      else if (that_.ArePrivateTagsRemoved() &&
               tag.IsPrivate())
      {
        // New in Orthanc 1.9.5
        // https://groups.google.com/g/orthanc-users/c/l1mcYCC2u-k/m/jOdGYuagAgAJ
        return Action_Remove;
      }
      else if (that_.IsCleared(tag) ||
               that_.IsRemoved(tag))
      {
        // New in Orthanc 1.9.5
        // https://groups.google.com/g/orthanc-users/c/l1mcYCC2u-k/m/jOdGYuagAgAJ
        return Action_Remove;
      }
      else
      {
        return Action_None;
      }
    }

  public:
    explicit RelationshipsVisitor(DicomModification& that) :
      that_(that)
    {
    }

    virtual Action VisitNotSupported(const std::vector<DicomTag>& parentTags,
                                     const std::vector<size_t>& parentIndexes,
                                     const DicomTag& tag,
                                     ValueRepresentation vr) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitSequence(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 size_t countItems) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitBinary(const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const void* data,
                               size_t size) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitIntegers(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 ValueRepresentation vr,
                                 const std::vector<int64_t>& values) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitDoubles(const std::vector<DicomTag>& parentTags,
                                const std::vector<size_t>& parentIndexes,
                                const DicomTag& tag,
                                ValueRepresentation vr,
                                const std::vector<double>& value) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitAttributes(const std::vector<DicomTag>& parentTags,
                                   const std::vector<size_t>& parentIndexes,
                                   const DicomTag& tag,
                                   const std::vector<DicomTag>& value) ORTHANC_OVERRIDE
    {
      return GetDefaultAction(parentTags, parentIndexes, tag);
    }

    virtual Action VisitString(std::string& newValue,
                               const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::string& value) ORTHANC_OVERRIDE
    {
      /**
       * Note that all the tags in "uids_" have the VR UI (unique
       * identifier), and are considered as strings.
       *
       * Also, the tags "SOP Instance UID", "Series Instance UID" and
       * "Study Instance UID" are *never* included in "uids_", as they
       * are separately handed by "MapDicomTags()".
       **/

      assert(that_.uids_.find(DICOM_TAG_STUDY_INSTANCE_UID) == that_.uids_.end());
      assert(that_.uids_.find(DICOM_TAG_SERIES_INSTANCE_UID) == that_.uids_.end());
      assert(that_.uids_.find(DICOM_TAG_SOP_INSTANCE_UID) == that_.uids_.end());

      if (parentTags.empty())
      {
        // We are on a first-level tag
        if (that_.uids_.find(tag) != that_.uids_.end() &&
            !IsManuallyModified(tag))
        {
          if (tag == DICOM_TAG_PATIENT_ID ||
              tag == DICOM_TAG_PATIENT_NAME)
          {
            assert(vr == ValueRepresentation_LongString ||
                   vr == ValueRepresentation_PersonName);
            newValue = that_.MapDicomIdentifier(value, ResourceType_Patient);            
          }
          else
          {
            // This is a first-level UID tag that must be anonymized
            assert(vr == ValueRepresentation_UniqueIdentifier ||
                   vr == ValueRepresentation_NotSupported /* for older versions of DCMTK */);
            newValue = that_.MapDicomIdentifier(value, ResourceType_Instance);
          }
          
          return Action_Replace;
        }
        else
        {
          return Action_None;
        }
      }
      else
      {
        // We are within a sequence

        if (IsKeptSequence(parentTags, parentIndexes, tag))
        {
          // New in Orthanc 1.9.4 - Solves issue LSD-629
          return Action_None;
        }

        if (that_.isAnonymization_)
        {
          // New in Orthanc 1.9.5, similar to "GetDefaultAction()"
          // https://groups.google.com/g/orthanc-users/c/l1mcYCC2u-k/m/jOdGYuagAgAJ
          if (that_.ArePrivateTagsRemoved() &&
              tag.IsPrivate())
          {
            return Action_Remove;
          }
          else if (that_.IsRemoved(tag))
          {
            return Action_Remove;
          }
          else if (that_.IsCleared(tag))
          {
            // This is different from "GetDefaultAction()", because we know how to clear string tags
            newValue.clear();
            return Action_Replace;
          }
        }

        if (tag == DICOM_TAG_STUDY_INSTANCE_UID)
        {
          newValue = that_.MapDicomIdentifier(value, ResourceType_Study);
          return Action_Replace;
        }
        else if (tag == DICOM_TAG_SERIES_INSTANCE_UID)
        {
          newValue = that_.MapDicomIdentifier(value, ResourceType_Series);
          return Action_Replace;
        }
        else if (tag == DICOM_TAG_SOP_INSTANCE_UID)
        {  
          newValue = that_.MapDicomIdentifier(value, ResourceType_Instance);
          return Action_Replace;
        }
        else if (that_.uids_.find(tag) != that_.uids_.end())
        {
          if (tag == DICOM_TAG_PATIENT_ID ||
              tag == DICOM_TAG_PATIENT_NAME)
          {
            newValue = that_.MapDicomIdentifier(value, ResourceType_Patient);
          }
          else
          {
            assert(vr == ValueRepresentation_UniqueIdentifier ||
                   vr == ValueRepresentation_NotSupported /* for older versions of DCMTK */);

            if (parentTags.size() == 2 &&
                parentTags[0] == DICOM_TAG_REFERENCED_FRAME_OF_REFERENCE_SEQUENCE &&
                parentTags[1] == DICOM_TAG_RT_REFERENCED_STUDY_SEQUENCE &&
                tag == DICOM_TAG_REFERENCED_SOP_INSTANCE_UID)
            {
              /**
               * In RT-STRUCT, this ReferencedSOPInstanceUID is actually
               * referencing a StudyInstanceUID !! (observed in many
               * data sets including:
               * https://wiki.cancerimagingarchive.net/display/Public/Lung+CT+Segmentation+Challenge+2017)
               * Tested in "test_anonymize_relationships_5". Introduced
               * in: https://hg.orthanc-server.com/orthanc/rev/3513
               **/
              newValue = that_.MapDicomIdentifier(value, ResourceType_Study);
            }
            else
            {
              newValue = that_.MapDicomIdentifier(value, ResourceType_Instance);
            }
          }

          return Action_Replace;
        }
        else
        {
          return Action_None;
        }
      }
    }

    void RemoveRelationships(ParsedDicomFile& dicom) const
    {
      for (SetOfTags::const_iterator it = that_.uids_.begin(); it != that_.uids_.end(); ++it)
      {
        assert(*it != DICOM_TAG_STUDY_INSTANCE_UID &&
               *it != DICOM_TAG_SERIES_INSTANCE_UID &&
               *it != DICOM_TAG_SOP_INSTANCE_UID);

        if (!IsManuallyModified(*it))
        {
          dicom.Remove(*it);
        }
      }

      // The only two sequences with to the "X/Z/U*" rule in the
      // basic profile. They were already present in Orthanc 1.9.3.
      if (!IsManuallyModified(DICOM_TAG_REFERENCED_IMAGE_SEQUENCE))
      {
        dicom.Remove(DICOM_TAG_REFERENCED_IMAGE_SEQUENCE);
      }
      
      if (!IsManuallyModified(DICOM_TAG_SOURCE_IMAGE_SEQUENCE))
      {
        dicom.Remove(DICOM_TAG_SOURCE_IMAGE_SEQUENCE);
      }
    }
  };


  void DicomModification::CancelReplacement(const DicomTag& tag)
  {
    Replacements::iterator it = replacements_.find(tag);
    
    if (it != replacements_.end())
    {
      assert(it->second != NULL);
      delete it->second;
      replacements_.erase(it);
    }
  }


  void DicomModification::ReplaceInternal(const DicomTag& tag,
                                          const Json::Value& value)
  {
    Replacements::iterator it = replacements_.find(tag);

    if (it != replacements_.end())
    {
      assert(it->second != NULL);
      delete it->second;
      it->second = NULL;   // In the case of an exception during the clone
      it->second = new Json::Value(value);  // Clone
    }
    else
    {
      replacements_[tag] = new Json::Value(value);  // Clone
    }
  }


  void DicomModification::ClearReplacements()
  {
    for (Replacements::iterator it = replacements_.begin();
         it != replacements_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }

    replacements_.clear();

    for (SequenceReplacements::iterator it = sequenceReplacements_.begin();
         it != sequenceReplacements_.end(); ++it)
    {
      assert(*it != NULL);
      assert((*it)->GetPath().GetPrefixLength() > 0);
      delete *it;
    }

    sequenceReplacements_.clear();
  }


  void DicomModification::MarkNotOrthancAnonymization()
  {
    Replacements::iterator it = replacements_.find(DICOM_TAG_DEIDENTIFICATION_METHOD);

    if (it != replacements_.end())
    {
      assert(it->second != NULL);

      if (it->second->asString() == ORTHANC_DEIDENTIFICATION_METHOD_2008 ||
          it->second->asString() == ORTHANC_DEIDENTIFICATION_METHOD_2017c ||
          it->second->asString() == ORTHANC_DEIDENTIFICATION_METHOD_2021b)
      {
        delete it->second;
        replacements_.erase(it);
      }
    }
  }

  void DicomModification::RegisterMappedDicomIdentifier(const std::string& original,
                                                        const std::string& mapped,
                                                        ResourceType level)
  {
    UidMap::const_iterator previous = uidMap_.find(std::make_pair(level, original));

    if (previous == uidMap_.end())
    {
      uidMap_.insert(std::make_pair(std::make_pair(level, original), mapped));
    }
  }

  std::string DicomModification::MapDicomIdentifier(const std::string& original,
                                                    ResourceType level)
  {
    const std::string stripped = Toolbox::StripSpaces(original);
    
    std::string mapped;

    UidMap::const_iterator previous = uidMap_.find(std::make_pair(level, stripped));

    if (previous == uidMap_.end())
    {
      if (identifierGenerator_ == NULL)
      {
        mapped = FromDcmtkBridge::GenerateUniqueIdentifier(level);
      }
      else
      {
        if (!identifierGenerator_->Apply(mapped, stripped, level, currentSource_))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Unable to generate an anonymized ID");
        }
      }

      uidMap_.insert(std::make_pair(std::make_pair(level, stripped), mapped));
    }
    else
    {
      mapped = previous->second;
    }

    return mapped;
  }


  void DicomModification::MapDicomTags(ParsedDicomFile& dicom,
                                       ResourceType level)
  {
    std::unique_ptr<DicomTag> tag;

    switch (level)
    {
      case ResourceType_Study:
        tag.reset(new DicomTag(DICOM_TAG_STUDY_INSTANCE_UID));
        break;

      case ResourceType_Series:
        tag.reset(new DicomTag(DICOM_TAG_SERIES_INSTANCE_UID));
        break;

      case ResourceType_Instance:
        tag.reset(new DicomTag(DICOM_TAG_SOP_INSTANCE_UID));
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::string original;
    if (!const_cast<const ParsedDicomFile&>(dicom).GetTagValue(original, *tag))
    {
      original = "";
    }

    std::string mapped = MapDicomIdentifier(original, level);

    dicom.Replace(*tag, mapped, 
                  false /* don't try and decode data URI scheme for UIDs */, 
                  DicomReplaceMode_InsertIfAbsent, privateCreator_);
  }

  
  DicomModification::DicomModification() :
    removePrivateTags_(false),
    level_(ResourceType_Instance),
    allowManualIdentifiers_(true),
    keepStudyInstanceUid_(false),
    keepSeriesInstanceUid_(false),
    keepSopInstanceUid_(false),
    updateReferencedRelationships_(true),
    isAnonymization_(false),
    //privateCreator_("PrivateCreator"),
    identifierGenerator_(NULL)
  {
  }

  DicomModification::~DicomModification()
  {
    ClearReplacements();
  }

  void DicomModification::Keep(const DicomTag& tag)
  {
    removals_.erase(tag);
    clearings_.erase(tag);
    uids_.erase(tag);

    CancelReplacement(tag);

    if (tag == DICOM_TAG_STUDY_INSTANCE_UID)
    {
      keepStudyInstanceUid_ = true;
    }
    else if (tag == DICOM_TAG_SERIES_INSTANCE_UID)
    {
      keepSeriesInstanceUid_ = true;
    }
    else if (tag == DICOM_TAG_SOP_INSTANCE_UID)
    {
      keepSopInstanceUid_ = true;
    }
    else if (tag.IsPrivate())
    {
      privateTagsToKeep_.insert(tag);
    }

    MarkNotOrthancAnonymization();
  }

  void DicomModification::Remove(const DicomTag& tag)
  {
    removals_.insert(tag);
    clearings_.erase(tag);
    uids_.erase(tag);
    CancelReplacement(tag);
    privateTagsToKeep_.erase(tag);

    MarkNotOrthancAnonymization();
  }

  void DicomModification::Clear(const DicomTag& tag)
  {
    removals_.erase(tag);
    clearings_.insert(tag);
    uids_.erase(tag);
    CancelReplacement(tag);
    privateTagsToKeep_.erase(tag);

    MarkNotOrthancAnonymization();
  }

  bool DicomModification::IsRemoved(const DicomTag& tag) const
  {
    if (removals_.find(tag) != removals_.end())
    {
      return true;
    }
    else
    {
      for (RemovedRanges::const_iterator it = removedRanges_.begin();
           it != removedRanges_.end(); ++it)
      {
        if (it->Contains(tag))
        {
          return true;
        }
      }

      return false;
    }
  }

  bool DicomModification::IsCleared(const DicomTag& tag) const
  {
    return clearings_.find(tag) != clearings_.end();
  }

  void DicomModification::Replace(const DicomTag& tag,
                                  const Json::Value& value,
                                  bool safeForAnonymization)
  {
    clearings_.erase(tag);
    removals_.erase(tag);
    uids_.erase(tag);
    privateTagsToKeep_.erase(tag);
    ReplaceInternal(tag, value);

    if (!safeForAnonymization)
    {
      MarkNotOrthancAnonymization();
    }
  }


  bool DicomModification::IsReplaced(const DicomTag& tag) const
  {
    return replacements_.find(tag) != replacements_.end();
  }

  const Json::Value& DicomModification::GetReplacement(const DicomTag& tag) const
  {
    Replacements::const_iterator it = replacements_.find(tag);

    if (it == replacements_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(it->second != NULL);
      return *it->second;
    } 
  }


  std::string DicomModification::GetReplacementAsString(const DicomTag& tag) const
  {
    const Json::Value& json = GetReplacement(tag);

    if (json.type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return json.asString();
    }    
  }


  void DicomModification::SetRemovePrivateTags(bool removed)
  {
    removePrivateTags_ = removed;

    if (!removed)
    {
      MarkNotOrthancAnonymization();
    }
  }

  bool DicomModification::ArePrivateTagsRemoved() const
  {
    return removePrivateTags_;
  }

  void DicomModification::SetLevel(ResourceType level)
  {
    uidMap_.clear();
    level_ = level;

    if (level != ResourceType_Patient)
    {
      MarkNotOrthancAnonymization();
    }
  }

  ResourceType DicomModification::GetLevel() const
  {
    return level_;
  }


  static void SetupUidsFromOrthancInternal(std::set<DicomTag>& uids,
                                           std::set<DicomTag>& removals,
                                           const DicomTag& tag)
  {
    uids.insert(tag);
    removals.erase(tag);  // Necessary if unserializing a job from 1.9.3
  }
  

  void DicomModification::SetupUidsFromOrthanc_1_9_3()
  {
    /**
     * Values below come from the hardcoded UID of Orthanc 1.9.3
     * in DicomModification::RelationshipsVisitor::VisitString() and
     * DicomModification::RelationshipsVisitor::RemoveRelationships()
     * https://hg.orthanc-server.com/orthanc/file/Orthanc-1.9.3/OrthancFramework/Sources/DicomParsing/DicomModification.cpp#l117
     **/
    uids_.clear();

    // (*) "PatientID" and "PatientName" are handled as UIDs since Orthanc 1.9.4
    uids_.insert(DICOM_TAG_PATIENT_ID);
    uids_.insert(DICOM_TAG_PATIENT_NAME);
    
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0008, 0x0014));  // Instance Creator UID                   <= from SetupAnonymization2008()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID            <= from VisitString() + RemoveRelationships()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0020, 0x0052));  // Frame of Reference UID                 <= from VisitString() + RemoveRelationships()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID <= from SetupAnonymization2008()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0040, 0xa124));  // UID                                    <= from SetupAnonymization2008()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x0088, 0x0140));  // Storage Media File-set UID             <= from SetupAnonymization2008()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID      <= from VisitString() + RemoveRelationships()
    SetupUidsFromOrthancInternal(uids_, removals_, DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID         <= from VisitString() + RemoveRelationships()
  }


  void DicomModification::SetupAnonymization2008()
  {
    // This is Table E.1-1 from PS 3.15-2008 - DICOM Part 15: Security and System Management Profiles
    // https://raw.githubusercontent.com/jodogne/dicom-specification/master/2008/08_15pu.pdf

    SetupUidsFromOrthanc_1_9_3();
    
    //uids_.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID => set in SetupUidsFromOrthanc_1_9_3()
    //removals_.insert(DicomTag(0x0008, 0x0018));  // SOP Instance UID => set in Apply()
    removals_.insert(DicomTag(0x0008, 0x0050));  // Accession Number
    removals_.insert(DicomTag(0x0008, 0x0080));  // Institution Name
    removals_.insert(DicomTag(0x0008, 0x0081));  // Institution Address
    removals_.insert(DicomTag(0x0008, 0x0090));  // Referring Physician's Name 
    removals_.insert(DicomTag(0x0008, 0x0092));  // Referring Physician's Address 
    removals_.insert(DicomTag(0x0008, 0x0094));  // Referring Physician's Telephone Numbers 
    removals_.insert(DicomTag(0x0008, 0x1010));  // Station Name 
    removals_.insert(DicomTag(0x0008, 0x1030));  // Study Description 
    removals_.insert(DicomTag(0x0008, 0x103e));  // Series Description 
    removals_.insert(DicomTag(0x0008, 0x1040));  // Institutional Department Name 
    removals_.insert(DicomTag(0x0008, 0x1048));  // Physician(s) of Record 
    removals_.insert(DicomTag(0x0008, 0x1050));  // Performing Physicians' Name 
    removals_.insert(DicomTag(0x0008, 0x1060));  // Name of Physician(s) Reading Study 
    removals_.insert(DicomTag(0x0008, 0x1070));  // Operators' Name 
    removals_.insert(DicomTag(0x0008, 0x1080));  // Admitting Diagnoses Description 
    //uids_.insert(DicomTag(0x0008, 0x1155));      // Referenced SOP Instance UID => set in SetupUidsFromOrthanc_1_9_3()
    removals_.insert(DicomTag(0x0008, 0x2111));  // Derivation Description 
    //removals_.insert(DicomTag(0x0010, 0x0010));  // Patient's Name => cf. below (*)
    //removals_.insert(DicomTag(0x0010, 0x0020));  // Patient ID => cf. below (*)
    removals_.insert(DicomTag(0x0010, 0x0030));  // Patient's Birth Date 
    removals_.insert(DicomTag(0x0010, 0x0032));  // Patient's Birth Time 
    removals_.insert(DicomTag(0x0010, 0x0040));  // Patient's Sex 
    removals_.insert(DicomTag(0x0010, 0x1000));  // Other Patient Ids 
    removals_.insert(DicomTag(0x0010, 0x1001));  // Other Patient Names 
    removals_.insert(DicomTag(0x0010, 0x1010));  // Patient's Age 
    removals_.insert(DicomTag(0x0010, 0x1020));  // Patient's Size 
    removals_.insert(DicomTag(0x0010, 0x1030));  // Patient's Weight 
    removals_.insert(DicomTag(0x0010, 0x1090));  // Medical Record Locator 
    removals_.insert(DicomTag(0x0010, 0x2160));  // Ethnic Group 
    removals_.insert(DicomTag(0x0010, 0x2180));  // Occupation 
    removals_.insert(DicomTag(0x0010, 0x21b0));  // Additional Patient's History 
    removals_.insert(DicomTag(0x0010, 0x4000));  // Patient Comments 
    removals_.insert(DicomTag(0x0018, 0x1000));  // Device Serial Number 
    removals_.insert(DicomTag(0x0018, 0x1030));  // Protocol Name 
    //removals_.insert(DicomTag(0x0020, 0x000d));  // Study Instance UID => set in Apply()
    //removals_.insert(DicomTag(0x0020, 0x000e));  // Series Instance UID => set in Apply()
    removals_.insert(DicomTag(0x0020, 0x0010));  // Study ID 
    //uids_.insert(DicomTag(0x0020, 0x0052));      // Frame of Reference UID => set in SetupUidsFromOrthanc_1_9_3()
    //uids_.insert(DicomTag(0x0020, 0x0200));      // Synchronization Frame of Reference UID => set in SetupUidsFromOrthanc_1_9_3()
    removals_.insert(DicomTag(0x0020, 0x4000));  // Image Comments 
    removals_.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence 
    //uids_.insert(DicomTag(0x0040, 0xa124));      // UID => set in SetupUidsFromOrthanc_1_9_3()
    removals_.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    //uids_.insert(DicomTag(0x0088, 0x0140));      // Storage Media File-set UID => set in SetupUidsFromOrthanc_1_9_3()
    //uids_.insert(DicomTag(0x3006, 0x0024));      // Referenced Frame of Reference UID => set in SetupUidsFromOrthanc_1_9_3()
    //uids_.insert(DicomTag(0x3006, 0x00c2));      // Related Frame of Reference UID => set in SetupUidsFromOrthanc_1_9_3()

    // Some more removals (from the experience of DICOM files at the CHU of Liege)
    removals_.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals_.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician
    removals_.insert(DicomTag(0x0010, 0x2154));  // PatientTelephoneNumbers
    removals_.insert(DicomTag(0x0010, 0x2000));  // Medical Alerts

    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2008);
  }
  

  void DicomModification::SetupAnonymization2017c()
  {
    /**
     * This is Table E.1-1 from PS 3.15-2017c (DICOM Part 15: Security
     * and System Management Profiles), "basic profile" column. It was
     * generated automatically by calling:
     * "../../../OrthancServer/Resources/GenerateAnonymizationProfile.py
     * https://raw.githubusercontent.com/jodogne/dicom-specification/master/2017c/part15.xml"
     **/
    
#include "DicomModification_Anonymization2017c.impl.h"
    
    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2017c);
  }
  

  void DicomModification::SetupAnonymization2021b()
  {
    /**
     * This is Table E.1-1 from PS 3.15-2021b (DICOM Part 15: Security
     * and System Management Profiles), "basic profile" column. It was
     * generated automatically by calling:
     * "../../../OrthancServer/Resources/GenerateAnonymizationProfile.py
     * https://raw.githubusercontent.com/jodogne/dicom-specification/master/2021b/part15.xml"
     *
     * http://dicom.nema.org/medical/dicom/2021b/output/chtml/part15/chapter_E.html#table_E.1-1a
     * http://dicom.nema.org/medical/dicom/2021b/output/chtml/part15/chapter_E.html#table_E.1-1
     **/
    
#include "DicomModification_Anonymization2021b.impl.h"
    
    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2021b);
  }
  

  void DicomModification::SetupAnonymization(DicomVersion version)
  {
    isAnonymization_ = true;
    
    removals_.clear();
    clearings_.clear();
    removedRanges_.clear();
    uids_.clear();
    ClearReplacements();
    removePrivateTags_ = true;
    level_ = ResourceType_Patient;
    uidMap_.clear();
    privateTagsToKeep_.clear();
    keepSequences_.clear();
    removeSequences_.clear();    

    switch (version)
    {
      case DicomVersion_2008:
        SetupAnonymization2008();
        break;

      case DicomVersion_2017c:
        SetupAnonymization2017c();
        break;

      case DicomVersion_2021b:
        SetupAnonymization2021b();
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Set the PatientIdentityRemoved tag
    ReplaceInternal(DicomTag(0x0012, 0x0062), "YES");

    // (*) Choose a random patient name and ID
    uids_.insert(DICOM_TAG_PATIENT_ID);
    uids_.insert(DICOM_TAG_PATIENT_NAME);

    // Sanity check
    for (SetOfTags::const_iterator it = uids_.begin(); it != uids_.end(); ++it)
    {
      ValueRepresentation vr = FromDcmtkBridge::LookupValueRepresentation(*it);
      if (*it == DICOM_TAG_PATIENT_ID)
      {
        if (vr != ValueRepresentation_LongString &&
            vr != ValueRepresentation_NotSupported /* if no dictionary loaded */)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else if (*it == DICOM_TAG_PATIENT_NAME)
      {
        if (vr != ValueRepresentation_PersonName &&
            vr != ValueRepresentation_NotSupported /* if no dictionary loaded */)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else if (vr != ValueRepresentation_UniqueIdentifier &&
               vr != ValueRepresentation_NotSupported /* for older versions of DCMTK */)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }        
  }

  void DicomModification::Apply(ParsedDicomFile& toModify)
  {
    // Check the request
    assert(ResourceType_Patient + 1 == ResourceType_Study &&
           ResourceType_Study + 1 == ResourceType_Series &&
           ResourceType_Series + 1 == ResourceType_Instance);

    if (IsRemoved(DICOM_TAG_PATIENT_ID) ||
        IsRemoved(DICOM_TAG_STUDY_INSTANCE_UID) ||
        IsRemoved(DICOM_TAG_SERIES_INSTANCE_UID) ||
        IsRemoved(DICOM_TAG_SOP_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
    

    // Sanity checks at the patient level
    bool isReplacedPatientId = (IsReplaced(DICOM_TAG_PATIENT_ID) ||
                                uids_.find(DICOM_TAG_PATIENT_ID) != uids_.end());
    
    if (level_ == ResourceType_Patient && !isReplacedPatientId)
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying a patient, her PatientID is required to be modified");
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a patient, the StudyInstanceUID cannot be manually modified");
      }

      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a patient, the SeriesInstanceUID cannot be manually modified");
      }

      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a patient, the SopInstanceUID cannot be manually modified");
      }
    }


    // Sanity checks at the study level
    if (level_ == ResourceType_Study && isReplacedPatientId)
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying a study, the parent PatientID cannot be manually modified");
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Study && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a study, the SeriesInstanceUID cannot be manually modified");
      }

      if (level_ == ResourceType_Study && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a study, the SopInstanceUID cannot be manually modified");
      }
    }


    // Sanity checks at the series level
    if (level_ == ResourceType_Series && isReplacedPatientId)
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying a series, the parent PatientID cannot be manually modified");
    }

    if (level_ == ResourceType_Series && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying a series, the parent StudyInstanceUID cannot be manually modified");
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Series && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "When modifying a series, the SopInstanceUID cannot be manually modified");
      }
    }


    // Sanity checks at the instance level
    if (level_ == ResourceType_Instance && isReplacedPatientId)
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying an instance, the parent PatientID cannot be manually modified");
    }

    if (level_ == ResourceType_Instance && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying an instance, the parent StudyInstanceUID cannot be manually modified");
    }

    if (level_ == ResourceType_Instance && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "When modifying an instance, the parent SeriesInstanceUID cannot be manually modified");
    }

    // (0) Create a summary of the source file, if a custom generator
    // is provided
    if (identifierGenerator_ != NULL)
    {
      toModify.ExtractDicomSummary(currentSource_, ORTHANC_MAXIMUM_TAG_LENGTH);
    }

    // (1) Make sure the relationships are updated with the ids that we force too
    // i.e: an RT-STRUCT is referencing its own StudyInstanceUID
    if (isAnonymization_ && updateReferencedRelationships_)
    {
      if (IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
      {
        std::string original;
        std::string replacement = GetReplacementAsString(DICOM_TAG_STUDY_INSTANCE_UID);
        const_cast<const ParsedDicomFile&>(toModify).GetTagValue(original, DICOM_TAG_STUDY_INSTANCE_UID);
        RegisterMappedDicomIdentifier(original, replacement, ResourceType_Study);
      }

      if (IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
      {
        std::string original;
        std::string replacement = GetReplacementAsString(DICOM_TAG_SERIES_INSTANCE_UID);
        const_cast<const ParsedDicomFile&>(toModify).GetTagValue(original, DICOM_TAG_SERIES_INSTANCE_UID);
        RegisterMappedDicomIdentifier(original, replacement, ResourceType_Series);
      }

      if (IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        std::string original;
        std::string replacement = GetReplacementAsString(DICOM_TAG_SOP_INSTANCE_UID);
        const_cast<const ParsedDicomFile&>(toModify).GetTagValue(original, DICOM_TAG_SOP_INSTANCE_UID);
        RegisterMappedDicomIdentifier(original, replacement, ResourceType_Instance);
      }
    }


    // (2) Remove the private tags, if need be
    if (removePrivateTags_)
    {
      toModify.RemovePrivateTags(privateTagsToKeep_);
    }

    // (3) Clear the tags specified by the user
    for (SetOfTags::const_iterator it = clearings_.begin(); 
         it != clearings_.end(); ++it)
    {
      toModify.Clear(*it, true /* only clear if the tag exists in the original file */);
    }

    // (4) Remove the tags specified by the user
    for (SetOfTags::const_iterator it = removals_.begin(); 
         it != removals_.end(); ++it)
    {
      toModify.Remove(*it);
    }

    // (5) Replace the tags
    for (Replacements::const_iterator it = replacements_.begin(); 
         it != replacements_.end(); ++it)
    {
      assert(it->second != NULL);
      toModify.Replace(it->first, *it->second, true /* decode data URI scheme */,
                       DicomReplaceMode_InsertIfAbsent, privateCreator_);
    }

    // (6) Update the DICOM identifiers
    if (level_ <= ResourceType_Study &&
        !IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      if (keepStudyInstanceUid_)
      {
        LOG(WARNING) << "Modifying a study while keeping its original StudyInstanceUID: This should be avoided!";
      }
      else
      {
        MapDicomTags(toModify, ResourceType_Study);
      }
    }

    if (level_ <= ResourceType_Series &&
        !IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      if (keepSeriesInstanceUid_)
      {
        LOG(WARNING) << "Modifying a series while keeping its original SeriesInstanceUID: This should be avoided!";
      }
      else
      {
        MapDicomTags(toModify, ResourceType_Series);
      }
    }

    if (level_ <= ResourceType_Instance &&  // Always true
        !IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
    {
      if (keepSopInstanceUid_)
      {
        LOG(WARNING) << "Modifying an instance while keeping its original SOPInstanceUID: This should be avoided!";
      }
      else
      {
        MapDicomTags(toModify, ResourceType_Instance);
      }
    }

    // (7) Update the "referenced" relationships in the case of an anonymization
    if (isAnonymization_)
    {
      RelationshipsVisitor visitor(*this);

      if (updateReferencedRelationships_)
      {
        const_cast<const ParsedDicomFile&>(toModify).Apply(visitor);
      }
      else
      {
        visitor.RemoveRelationships(toModify);
      }
    }

    // (8) New in Orthanc 1.9.4: Apply modifications to subsequences
    for (ListOfPaths::const_iterator it = removeSequences_.begin();
         it != removeSequences_.end(); ++it)
    {
      assert(it->GetPrefixLength() > 0);
      toModify.RemovePath(*it);
    }

    for (SequenceReplacements::const_iterator it = sequenceReplacements_.begin();
         it != sequenceReplacements_.end(); ++it)
    {
      assert(*it != NULL);
      assert((*it)->GetPath().GetPrefixLength() > 0);
      toModify.ReplacePath((*it)->GetPath(), (*it)->GetValue(), true /* decode data URI scheme */,
                           DicomReplaceMode_InsertIfAbsent, privateCreator_);
    }
  }

  void DicomModification::SetAllowManualIdentifiers(bool check)
  {
    allowManualIdentifiers_ = check;
  }

  bool DicomModification::AreAllowManualIdentifiers() const
  {
    return allowManualIdentifiers_;
  }


  static bool IsDatabaseKey(const DicomTag& tag)
  {
    return (tag == DICOM_TAG_PATIENT_ID ||
            tag == DICOM_TAG_STUDY_INSTANCE_UID ||
            tag == DICOM_TAG_SERIES_INSTANCE_UID ||
            tag == DICOM_TAG_SOP_INSTANCE_UID);
  }


  static void ParseListOfTags(DicomModification& target,
                              const Json::Value& query,
                              TagOperation operation,
                              bool force)
  {
    if (!query.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    for (Json::Value::ArrayIndex i = 0; i < query.size(); i++)
    {
      if (query[i].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }
      
      std::string name = query[i].asString();

      const DicomPath path(DicomPath::Parse(name));

      if (path.GetPrefixLength() == 0 &&
          !force &&
          IsDatabaseKey(path.GetFinalTag()))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Marking tag \"" + name + "\" as to be " +
                               (operation == TagOperation_Keep ? "kept" : "removed") +
                               " requires the \"Force\" option to be set to true");
      }

      switch (operation)
      {
        case TagOperation_Keep:
          target.Keep(path);
          LOG(TRACE) << "Keep: " << name << " = " << path.Format();
          break;

        case TagOperation_Remove:
          target.Remove(path);
          LOG(TRACE) << "Remove: " << name << " = " << path.Format();
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  static void ParseReplacements(DicomModification& target,
                                const Json::Value& replacements,
                                bool force)
  {
    if (!replacements.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    Json::Value::Members members = replacements.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      const Json::Value& value = replacements[name];

      const DicomPath path(DicomPath::Parse(name));

      if (path.GetPrefixLength() == 0 &&
          !force &&
          IsDatabaseKey(path.GetFinalTag()))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Marking tag \"" + name + "\" as to be replaced " +
                               "requires the \"Force\" option to be set to true");
      }
        
      target.Replace(path, value, false /* not safe for anonymization */);

      LOG(TRACE) << "Replace: " << name << " = " << path.Format() 
                 << " by: " << value.toStyledString();
    }
  }


  static bool GetBooleanValue(const std::string& member,
                              const Json::Value& json,
                              bool defaultValue)
  {
    if (!json.isMember(member))
    {
      return defaultValue;
    }
    else if (json[member].type() == Json::booleanValue)
    {
      return json[member].asBool();
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Member \"" + member + "\" should be a Boolean value");
    }
  }


  void DicomModification::ParseModifyRequest(const Json::Value& request)
  {
    if (!request.isObject())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    bool force = GetBooleanValue("Force", request, false);
      
    if (GetBooleanValue("RemovePrivateTags", request, false))
    {
      SetRemovePrivateTags(true);
    }

    if (request.isMember("Remove"))
    {
      ParseListOfTags(*this, request["Remove"], TagOperation_Remove, force);
    }

    if (request.isMember("Replace"))
    {
      ParseReplacements(*this, request["Replace"], force);
    }

    // The "Keep" operation only makes sense for the tags
    // StudyInstanceUID, SeriesInstanceUID and SOPInstanceUID. Avoid
    // this feature as much as possible, as this breaks the DICOM
    // model of the real world, except if you know exactly what
    // you're doing!
    if (request.isMember("Keep"))
    {
      ParseListOfTags(*this, request["Keep"], TagOperation_Keep, force);
    }

    // New in Orthanc 1.6.0
    if (request.isMember("PrivateCreator"))
    {
      privateCreator_ = SerializationToolbox::ReadString(request, "PrivateCreator");
    }
  }


  void DicomModification::ParseAnonymizationRequest(bool& patientNameOverridden,
                                                    const Json::Value& request)
  {
    if (!request.isObject())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    bool force = GetBooleanValue("Force", request, false);
      
    // DicomVersion version = DicomVersion_2008;   // For Orthanc <= 1.2.0
    // DicomVersion version = DicomVersion_2017c;  // For Orthanc between 1.3.0 and 1.9.3
    DicomVersion version = DicomVersion_2021b;     // For Orthanc >= 1.9.4
    if (request.isMember("DicomVersion"))
    {
      if (request["DicomVersion"].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        version = StringToDicomVersion(request["DicomVersion"].asString());
      }
    }
        
    SetupAnonymization(version);

    if (GetBooleanValue("KeepPrivateTags", request, false))
    {
      SetRemovePrivateTags(false);
    }

    if (request.isMember("Remove"))
    {
      ParseListOfTags(*this, request["Remove"], TagOperation_Remove, force);
    }

    if (request.isMember("Replace"))
    {
      ParseReplacements(*this, request["Replace"], force);
    }

    if (request.isMember("Keep"))
    {
      ParseListOfTags(*this, request["Keep"], TagOperation_Keep, force);
    }

    patientNameOverridden = (uids_.find(DICOM_TAG_PATIENT_NAME) == uids_.end());
    
    // New in Orthanc 1.6.0
    if (request.isMember("PrivateCreator"))
    {
      privateCreator_ = SerializationToolbox::ReadString(request, "PrivateCreator");
    }
  }

  void DicomModification::SetDicomIdentifierGenerator(DicomModification::IDicomIdentifierGenerator &generator)
  {
    identifierGenerator_ = &generator;
  }




  static const char* REMOVE_PRIVATE_TAGS = "RemovePrivateTags";
  static const char* LEVEL = "Level";
  static const char* ALLOW_MANUAL_IDENTIFIERS = "AllowManualIdentifiers";
  static const char* KEEP_STUDY_INSTANCE_UID = "KeepStudyInstanceUID";
  static const char* KEEP_SERIES_INSTANCE_UID = "KeepSeriesInstanceUID";
  static const char* KEEP_SOP_INSTANCE_UID = "KeepSOPInstanceUID";
  static const char* UPDATE_REFERENCED_RELATIONSHIPS = "UpdateReferencedRelationships";
  static const char* IS_ANONYMIZATION = "IsAnonymization";
  static const char* REMOVALS = "Removals";
  static const char* CLEARINGS = "Clearings";
  static const char* PRIVATE_TAGS_TO_KEEP = "PrivateTagsToKeep";
  static const char* REPLACEMENTS = "Replacements";
  static const char* MAP_PATIENTS = "MapPatients";
  static const char* MAP_STUDIES = "MapStudies";
  static const char* MAP_SERIES = "MapSeries";
  static const char* MAP_INSTANCES = "MapInstances";
  static const char* PRIVATE_CREATOR = "PrivateCreator";    // New in Orthanc 1.6.0
  static const char* UIDS = "Uids";                         // New in Orthanc 1.9.4
  static const char* REMOVED_RANGES = "RemovedRanges";      // New in Orthanc 1.9.4
  static const char* KEEP_SEQUENCES = "KeepSequences";      // New in Orthanc 1.9.4
  static const char* REMOVE_SEQUENCES = "RemoveSequences";  // New in Orthanc 1.9.4
  static const char* SEQUENCE_REPLACEMENTS = "SequenceReplacements";  // New in Orthanc 1.9.4
  
  void DicomModification::Serialize(Json::Value& value) const
  {
    if (identifierGenerator_ != NULL)
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot serialize a DicomModification with a custom identifier generator");
    }

    value = Json::objectValue;
    value[REMOVE_PRIVATE_TAGS] = removePrivateTags_;
    value[LEVEL] = EnumerationToString(level_);
    value[ALLOW_MANUAL_IDENTIFIERS] = allowManualIdentifiers_;
    value[KEEP_STUDY_INSTANCE_UID] = keepStudyInstanceUid_;
    value[KEEP_SERIES_INSTANCE_UID] = keepSeriesInstanceUid_;
    value[KEEP_SOP_INSTANCE_UID] = keepSopInstanceUid_;
    value[UPDATE_REFERENCED_RELATIONSHIPS] = updateReferencedRelationships_;
    value[IS_ANONYMIZATION] = isAnonymization_;
    value[PRIVATE_CREATOR] = privateCreator_;

    SerializationToolbox::WriteSetOfTags(value, removals_, REMOVALS);
    SerializationToolbox::WriteSetOfTags(value, clearings_, CLEARINGS);
    SerializationToolbox::WriteSetOfTags(value, privateTagsToKeep_, PRIVATE_TAGS_TO_KEEP);

    Json::Value& tmp = value[REPLACEMENTS];

    tmp = Json::objectValue;

    for (Replacements::const_iterator it = replacements_.begin();
         it != replacements_.end(); ++it)
    {
      assert(it->second != NULL);
      tmp[it->first.Format()] = *it->second;
    }

    Json::Value& mapPatients = value[MAP_PATIENTS];
    Json::Value& mapStudies = value[MAP_STUDIES];
    Json::Value& mapSeries = value[MAP_SERIES];
    Json::Value& mapInstances = value[MAP_INSTANCES];

    mapPatients = Json::objectValue;
    mapStudies = Json::objectValue;
    mapSeries = Json::objectValue;
    mapInstances = Json::objectValue;

    for (UidMap::const_iterator it = uidMap_.begin(); it != uidMap_.end(); ++it)
    {
      Json::Value* tmp2 = NULL;

      switch (it->first.first)
      {
        case ResourceType_Patient:
          tmp2 = &mapPatients;
          break;

        case ResourceType_Study:
          tmp2 = &mapStudies;
          break;

        case ResourceType_Series:
          tmp2 = &mapSeries;
          break;

        case ResourceType_Instance:
          tmp2 = &mapInstances;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      assert(tmp2 != NULL);
      (*tmp2) [it->first.second] = it->second;
    }

    // New in Orthanc 1.9.4
    SerializationToolbox::WriteSetOfTags(value, uids_, UIDS);

    // New in Orthanc 1.9.4
    Json::Value ranges = Json::arrayValue;
      
    for (RemovedRanges::const_iterator it = removedRanges_.begin(); it != removedRanges_.end(); ++it)
    {
      Json::Value item = Json::arrayValue;
      item.append(it->GetGroupFrom());
      item.append(it->GetGroupTo());
      item.append(it->GetElementFrom());
      item.append(it->GetElementTo());
      ranges.append(item);
    }

    value[REMOVED_RANGES] = ranges;

    // New in Orthanc 1.9.4
    Json::Value lst = Json::arrayValue;
    for (ListOfPaths::const_iterator it = keepSequences_.begin(); it != keepSequences_.end(); ++it)
    {
      lst.append(it->Format());
    }

    value[KEEP_SEQUENCES] = lst;

    // New in Orthanc 1.9.4
    lst = Json::arrayValue;
    for (ListOfPaths::const_iterator it = removeSequences_.begin(); it != removeSequences_.end(); ++it)
    {
      assert(it->GetPrefixLength() > 0);
      lst.append(it->Format());
    }

    value[REMOVE_SEQUENCES] = lst;

    // New in Orthanc 1.9.4
    lst = Json::objectValue;
    for (SequenceReplacements::const_iterator it = sequenceReplacements_.begin(); it != sequenceReplacements_.end(); ++it)
    {
      assert(*it != NULL);
      assert((*it)->GetPath().GetPrefixLength() > 0);
      lst[(*it)->GetPath().Format()] = (*it)->GetValue();
    }

    value[SEQUENCE_REPLACEMENTS] = lst;
  }

  void DicomModification::UnserializeUidMap(ResourceType level,
                                            const Json::Value& serialized,
                                            const char* field)
  {
    if (!serialized.isMember(field) ||
        serialized[field].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value::Members names = serialized[field].getMemberNames();
    
    for (Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it)
    {
      const Json::Value& value = serialized[field][*it];

      if (value.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        uidMap_[std::make_pair(level, *it)] = value.asString();
      }
    }
  }

  
  DicomModification::DicomModification(const Json::Value& serialized) :
    identifierGenerator_(NULL)
  {
    removePrivateTags_ = SerializationToolbox::ReadBoolean(serialized, REMOVE_PRIVATE_TAGS);
    level_ = StringToResourceType(SerializationToolbox::ReadString(serialized, LEVEL).c_str());
    allowManualIdentifiers_ = SerializationToolbox::ReadBoolean(serialized, ALLOW_MANUAL_IDENTIFIERS);
    keepStudyInstanceUid_ = SerializationToolbox::ReadBoolean(serialized, KEEP_STUDY_INSTANCE_UID);
    keepSeriesInstanceUid_ = SerializationToolbox::ReadBoolean(serialized, KEEP_SERIES_INSTANCE_UID);
    updateReferencedRelationships_ = SerializationToolbox::ReadBoolean
      (serialized, UPDATE_REFERENCED_RELATIONSHIPS);
    isAnonymization_ = SerializationToolbox::ReadBoolean(serialized, IS_ANONYMIZATION);

    if (serialized.isMember(KEEP_SOP_INSTANCE_UID))
    {
      keepSopInstanceUid_ = SerializationToolbox::ReadBoolean(serialized, KEEP_SOP_INSTANCE_UID);
    }
    else
    {
      /**
       * Compatibility with jobs serialized using Orthanc between
       * 1.5.0 and 1.6.1. This compatibility was broken between 1.7.0
       * and 1.9.3: Indeed, an exception was thrown in "ReadBoolean()"
       * if "KEEP_SOP_INSTANCE_UID" was absent, because of changeset:
       * https://hg.orthanc-server.com/orthanc/rev/3860
       **/
      keepSopInstanceUid_ = false;
    }

    if (serialized.isMember(PRIVATE_CREATOR))
    {
      privateCreator_ = SerializationToolbox::ReadString(serialized, PRIVATE_CREATOR);
    }

    SerializationToolbox::ReadSetOfTags(removals_, serialized, REMOVALS);
    SerializationToolbox::ReadSetOfTags(clearings_, serialized, CLEARINGS);
    SerializationToolbox::ReadSetOfTags(privateTagsToKeep_, serialized, PRIVATE_TAGS_TO_KEEP);

    if (!serialized.isMember(REPLACEMENTS) ||
        serialized[REPLACEMENTS].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value::Members names = serialized[REPLACEMENTS].getMemberNames();

    for (Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it)
    {
      DicomTag tag(0, 0);
      if (!DicomTag::ParseHexadecimal(tag, it->c_str()))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        const Json::Value& value = serialized[REPLACEMENTS][*it];
        replacements_.insert(std::make_pair(tag, new Json::Value(value)));
      }
    }

    UnserializeUidMap(ResourceType_Patient, serialized, MAP_PATIENTS);
    UnserializeUidMap(ResourceType_Study, serialized, MAP_STUDIES);
    UnserializeUidMap(ResourceType_Series, serialized, MAP_SERIES);
    UnserializeUidMap(ResourceType_Instance, serialized, MAP_INSTANCES);

    // New in Orthanc 1.9.4
    if (serialized.isMember(UIDS))  // Backward compatibility with Orthanc <= 1.9.3
    {
      SerializationToolbox::ReadSetOfTags(uids_, serialized, UIDS);
    }
    else
    {
      SetupUidsFromOrthanc_1_9_3();
    }

    // New in Orthanc 1.9.4
    removedRanges_.clear();
    if (serialized.isMember(REMOVED_RANGES))  // Backward compatibility with Orthanc <= 1.9.3
    {
      const Json::Value& ranges = serialized[REMOVED_RANGES];
      
      if (ranges.type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        for (Json::Value::ArrayIndex i = 0; i < ranges.size(); i++)
        {
          if (ranges[i].type() != Json::arrayValue ||
              ranges[i].size() != 4 ||
              !ranges[i][0].isUInt() ||
              !ranges[i][1].isUInt() ||
              !ranges[i][2].isUInt() ||
              !ranges[i][3].isUInt())
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
          else
          {
            Json::LargestUInt groupFrom = ranges[i][0].asUInt();
            Json::LargestUInt groupTo = ranges[i][1].asUInt();
            Json::LargestUInt elementFrom = ranges[i][2].asUInt();
            Json::LargestUInt elementTo = ranges[i][3].asUInt();

            if (groupFrom > groupTo ||
                elementFrom > elementTo ||
                groupTo > 0xffffu ||
                elementTo > 0xffffu)
            {
              throw OrthancException(ErrorCode_BadFileFormat);
            }
            else
            {
              removedRanges_.push_back(DicomTagRange(groupFrom, groupTo, elementFrom, elementTo));
            }
          }
        }
      }
    }

    // New in Orthanc 1.9.4
    if (serialized.isMember(KEEP_SEQUENCES))
    {
      const Json::Value& keep = serialized[KEEP_SEQUENCES];
      
      if (keep.type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        for (Json::Value::ArrayIndex i = 0; i < keep.size(); i++)
        {
          if (keep[i].type() != Json::stringValue)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
          else
          {
            keepSequences_.push_back(DicomPath::Parse(keep[i].asString()));
          }
        }
      }
    }

    // New in Orthanc 1.9.4
    if (serialized.isMember(REMOVE_SEQUENCES))
    {
      const Json::Value& remove = serialized[REMOVE_SEQUENCES];
      
      if (remove.type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        for (Json::Value::ArrayIndex i = 0; i < remove.size(); i++)
        {
          if (remove[i].type() != Json::stringValue)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
          else
          {
            removeSequences_.push_back(DicomPath::Parse(remove[i].asString()));
          }
        }
      }
    }

    // New in Orthanc 1.9.4
    if (serialized.isMember(SEQUENCE_REPLACEMENTS))
    {
      const Json::Value& replace = serialized[SEQUENCE_REPLACEMENTS];
      
      if (replace.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        Json::Value::Members members = replace.getMemberNames();
        for (size_t i = 0; i < members.size(); i++)
        {
          sequenceReplacements_.push_back(
            new SequenceReplacement(DicomPath::Parse(members[i]), replace[members[i]]));
        }
      }
    }
  }


  void DicomModification::SetPrivateCreator(const std::string &privateCreator)
  {
    privateCreator_ = privateCreator;
  }

  const std::string &DicomModification::GetPrivateCreator() const
  {
    return privateCreator_;
  }


  void DicomModification::Keep(const DicomPath& path)
  {
    if (path.GetPrefixLength() == 0)
    {
      Keep(path.GetFinalTag());
    }

    keepSequences_.push_back(path);
    MarkNotOrthancAnonymization();
  }
  

  void DicomModification::Remove(const DicomPath& path)
  {
    if (path.GetPrefixLength() == 0)
    {
      Remove(path.GetFinalTag());
    }
    else
    {
      removeSequences_.push_back(path);
      MarkNotOrthancAnonymization();
    }
  }
  

  void DicomModification::Replace(const DicomPath& path,
                                  const Json::Value& value,
                                  bool safeForAnonymization)
  {
    if (path.GetPrefixLength() == 0)
    {
      Replace(path.GetFinalTag(), value, safeForAnonymization);
    }
    else
    {
      sequenceReplacements_.push_back(new SequenceReplacement(path, value));

      if (!safeForAnonymization)
      {
        MarkNotOrthancAnonymization();
      }
    }
  }


  bool DicomModification::IsAlteredTag(const DicomTag& tag) const
  {
    return (uids_.find(tag) != uids_.end() ||
            IsCleared(tag) ||
            IsRemoved(tag) ||
            IsReplaced(tag) ||
            (tag.IsPrivate() &&
             ArePrivateTagsRemoved() &&
             privateTagsToKeep_.find(tag) == privateTagsToKeep_.end()) ||
            (isAnonymization_ && (
              tag == DICOM_TAG_PATIENT_NAME ||
              tag == DICOM_TAG_PATIENT_ID)) ||
            (tag == DICOM_TAG_STUDY_INSTANCE_UID &&
             !keepStudyInstanceUid_) ||
            (tag == DICOM_TAG_SERIES_INSTANCE_UID &&
             !keepSeriesInstanceUid_) ||
            (tag == DICOM_TAG_SOP_INSTANCE_UID &&
             !keepSopInstanceUid_));
  }
}
