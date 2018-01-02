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


#include "../PrecompiledHeaders.h"
#include "DicomModification.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "FromDcmtkBridge.h"

#include <memory>   // For std::auto_ptr


static const std::string ORTHANC_DEIDENTIFICATION_METHOD_2008 =
  "Orthanc " ORTHANC_VERSION " - PS 3.15-2008 Table E.1-1";

static const std::string ORTHANC_DEIDENTIFICATION_METHOD_2017c =
  "Orthanc " ORTHANC_VERSION " - PS 3.15-2017c Table E.1-1 Basic Profile";

namespace Orthanc
{
  bool DicomModification::CancelReplacement(const DicomTag& tag)
  {
    Replacements::iterator it = replacements_.find(tag);
    
    if (it != replacements_.end())
    {
      delete it->second;
      replacements_.erase(it);
      return true;
    }
    else
    {
      return false;
    }
  }


  void DicomModification::ReplaceInternal(const DicomTag& tag,
                                          const Json::Value& value)
  {
    Replacements::iterator it = replacements_.find(tag);

    if (it != replacements_.end())
    {
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
      delete it->second;
    }

    replacements_.clear();
  }


  void DicomModification::MarkNotOrthancAnonymization()
  {
    Replacements::iterator it = replacements_.find(DICOM_TAG_DEIDENTIFICATION_METHOD);

    if (it != replacements_.end() &&
        (it->second->asString() == ORTHANC_DEIDENTIFICATION_METHOD_2008 ||
         it->second->asString() == ORTHANC_DEIDENTIFICATION_METHOD_2017c))
    {
      delete it->second;
      replacements_.erase(it);
    }
  }


  void DicomModification::MapDicomIdentifier(ParsedDicomFile& dicom,
                                             ResourceType level)
  {
    std::auto_ptr<DicomTag> tag;

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
    if (!dicom.GetTagValue(original, *tag))
    {
      original = "";
    }

    std::string mapped;

    UidMap::const_iterator previous = uidMap_.find(std::make_pair(level, original));
    if (previous == uidMap_.end())
    {
      mapped = FromDcmtkBridge::GenerateUniqueIdentifier(level);
      uidMap_.insert(std::make_pair(std::make_pair(level, original), mapped));
    }
    else
    {
      mapped = previous->second;
    }    

    dicom.Replace(*tag, mapped, false /* don't try and decode data URI scheme for UIDs */, DicomReplaceMode_InsertIfAbsent);
  }
  
  DicomModification::DicomModification() :
    removePrivateTags_(false),
    level_(ResourceType_Instance),
    allowManualIdentifiers_(true),
    keepStudyInstanceUid_(false),
    keepSeriesInstanceUid_(false)
  {
  }

  DicomModification::~DicomModification()
  {
    ClearReplacements();
  }

  void DicomModification::Keep(const DicomTag& tag)
  {
    bool wasRemoved = IsRemoved(tag);
    bool wasCleared = IsCleared(tag);
    
    removals_.erase(tag);
    clearings_.erase(tag);

    bool wasReplaced = CancelReplacement(tag);

    if (tag == DICOM_TAG_STUDY_INSTANCE_UID)
    {
      keepStudyInstanceUid_ = true;
    }
    else if (tag == DICOM_TAG_SERIES_INSTANCE_UID)
    {
      keepSeriesInstanceUid_ = true;
    }
    else if (tag.IsPrivate())
    {
      privateTagsToKeep_.insert(tag);
    }
    else if (!wasRemoved &&
             !wasReplaced &&
             !wasCleared)
    {
      LOG(WARNING) << "Marking this tag as to be kept has no effect: " << tag.Format();
    }

    MarkNotOrthancAnonymization();
  }

  void DicomModification::Remove(const DicomTag& tag)
  {
    removals_.insert(tag);
    clearings_.erase(tag);
    CancelReplacement(tag);
    privateTagsToKeep_.erase(tag);

    MarkNotOrthancAnonymization();
  }

  void DicomModification::Clear(const DicomTag& tag)
  {
    removals_.erase(tag);
    clearings_.insert(tag);
    CancelReplacement(tag);
    privateTagsToKeep_.erase(tag);

    MarkNotOrthancAnonymization();
  }

  bool DicomModification::IsRemoved(const DicomTag& tag) const
  {
    return removals_.find(tag) != removals_.end();
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

  void DicomModification::SetLevel(ResourceType level)
  {
    uidMap_.clear();
    level_ = level;

    if (level != ResourceType_Patient)
    {
      MarkNotOrthancAnonymization();
    }
  }


  void DicomModification::SetupAnonymization2008()
  {
    // This is Table E.1-1 from PS 3.15-2008 - DICOM Part 15: Security and System Management Profiles
    // https://raw.githubusercontent.com/jodogne/dicom-specification/master/2008/08_15pu.pdf
    
    removals_.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID
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
    removals_.insert(DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID 
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
    removals_.insert(DicomTag(0x0020, 0x0052));  // Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x4000));  // Image Comments 
    removals_.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence 
    removals_.insert(DicomTag(0x0040, 0xa124));  // UID
    removals_.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    removals_.insert(DicomTag(0x0088, 0x0140));  // Storage Media File-set UID 
    removals_.insert(DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID 
    removals_.insert(DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID 

    // Some more removals (from the experience of DICOM files at the CHU of Liege)
    removals_.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals_.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician
    removals_.insert(DicomTag(0x0010, 0x2154));  // PatientTelephoneNumbers
    removals_.insert(DicomTag(0x0010, 0x2000));  // Medical Alerts

    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2008);
  }
  

#if 0
  /**
   * This is a manual implementation by Alain Mazy. Only kept for reference.
   * https://bitbucket.org/sjodogne/orthanc/commits/c6defdc4c611fca2ab528ba2c6937a742e0329a8?at=issue-46-anonymization
   **/
  
  void DicomModification::SetupAnonymization2011()
  {
    // This is Table E.1-1 from PS 3.15-2011 - DICOM Part 15: Security and System Management Profiles
    // https://raw.githubusercontent.com/jodogne/dicom-specification/master/2011/11_15pu.pdf
    
    removals_.insert(DicomTag(0x0000, 0x1000));  // Affected SOP Instance UID
    removals_.insert(DicomTag(0x0000, 0x1001));  // Requested SOP Instance UID
    removals_.insert(DicomTag(0x0002, 0x0003));  // Media Storage SOP Instance UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    removals_.insert(DicomTag(0x0004, 0x1511));  // Referenced SOP Instance UID in File
    removals_.insert(DicomTag(0x0008, 0x0010));  // Irradiation Event UID
    removals_.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID
    //removals_.insert(DicomTag(0x0008, 0x0018));  // SOP Instance UID => set in Apply()
    clearings_.insert(DicomTag(0x0008, 0x0020)); // Study Date
    clearings_.insert(DicomTag(0x0008, 0x0021)); // Series Date
    clearings_.insert(DicomTag(0x0008, 0x0030)); // Study Time
    clearings_.insert(DicomTag(0x0008, 0x0031)); // Series Time
    removals_.insert(DicomTag(0x0008, 0x0022));  // Acquisition Date
    removals_.insert(DicomTag(0x0008, 0x0023));  // Content Date
    removals_.insert(DicomTag(0x0008, 0x0024));  // Overlay Date
    removals_.insert(DicomTag(0x0008, 0x0025));  // Curve Date
    removals_.insert(DicomTag(0x0008, 0x002a));  // Acquisition DateTime
    removals_.insert(DicomTag(0x0008, 0x0032));  // Acquisition Time
    removals_.insert(DicomTag(0x0008, 0x0033));  // Content Time
    removals_.insert(DicomTag(0x0008, 0x0034));  // Overlay Time
    removals_.insert(DicomTag(0x0008, 0x0035));  // Curve Time
    removals_.insert(DicomTag(0x0008, 0x0050));  // Accession Number
    removals_.insert(DicomTag(0x0008, 0x0058));  // Failed SOP Instance UID List
    removals_.insert(DicomTag(0x0008, 0x0080));  // Institution Name
    removals_.insert(DicomTag(0x0008, 0x0081));  // Institution Address
    removals_.insert(DicomTag(0x0008, 0x0082));  // Institution Code Sequence
    removals_.insert(DicomTag(0x0008, 0x0090));  // Referring Physician's Name
    removals_.insert(DicomTag(0x0008, 0x0092));  // Referring Physician's Address 
    removals_.insert(DicomTag(0x0008, 0x0094));  // Referring Physician's Telephone Numbers 
    removals_.insert(DicomTag(0x0008, 0x0096));  // Referring Physician's Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x010d));  // Context Group Extension Creator UID
    removals_.insert(DicomTag(0x0008, 0x0201));  // Timezone Offset From UTC
    removals_.insert(DicomTag(0x0008, 0x0300));  // Current Patient Location
    removals_.insert(DicomTag(0x0008, 0x1010));  // Station Name
    removals_.insert(DicomTag(0x0008, 0x1030));  // Study Description 
    removals_.insert(DicomTag(0x0008, 0x103e));  // Series Description 
    removals_.insert(DicomTag(0x0008, 0x1040));  // Institutional Department Name 
    removals_.insert(DicomTag(0x0008, 0x1048));  // Physician(s) of Record 
    removals_.insert(DicomTag(0x0008, 0x1049));  // Physician(s) of Record Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1050));  // Performing Physicians' Name
    removals_.insert(DicomTag(0x0008, 0x1052));  // Performing Physicians Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1060));  // Name of Physician(s) Reading Study
    removals_.insert(DicomTag(0x0008, 0x1062));  // Physician Reading Study Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1070));  // Operators' Name
    removals_.insert(DicomTag(0x0008, 0x1072));  // Operators' Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1080));  // Admitting Diagnoses Description
    removals_.insert(DicomTag(0x0008, 0x1084));  // Admitting Diagnoses Code Sequence
    removals_.insert(DicomTag(0x0008, 0x1110));  // Referenced Study Sequence
    removals_.insert(DicomTag(0x0008, 0x1111));  // Referenced Performed Procedure Step Sequence
    removals_.insert(DicomTag(0x0008, 0x1120));  // Referenced Patient Sequence
    removals_.insert(DicomTag(0x0008, 0x1140));  // Referenced Image Sequence
    removals_.insert(DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID
    removals_.insert(DicomTag(0x0008, 0x1195));  // Transaction UID
    removals_.insert(DicomTag(0x0008, 0x2111));  // Derivation Description
    removals_.insert(DicomTag(0x0008, 0x2112));  // Source Image Sequence
    removals_.insert(DicomTag(0x0008, 0x4000));  // Identifying Comments
    removals_.insert(DicomTag(0x0008, 0x9123));  // Creator Version UID
    //removals_.insert(DicomTag(0x0010, 0x0010));  // Patient's Name => cf. below (*)
    //removals_.insert(DicomTag(0x0010, 0x0020));  // Patient ID => cf. below (*)
    removals_.insert(DicomTag(0x0010, 0x0030));  // Patient's Birth Date 
    removals_.insert(DicomTag(0x0010, 0x0032));  // Patient's Birth Time 
    clearings_.insert(DicomTag(0x0010, 0x0040)); // Patient's Sex
    removals_.insert(DicomTag(0x0010, 0x0050));  // Patient's Insurance Plan Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0101));  // Patient's Primary Language Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0102));  // Patient's Primary Language Modifier Code Sequence
    removals_.insert(DicomTag(0x0010, 0x1000));  // Other Patient Ids
    removals_.insert(DicomTag(0x0010, 0x1001));  // Other Patient Names 
    removals_.insert(DicomTag(0x0010, 0x1002));  // Other Patient IDs Sequence
    removals_.insert(DicomTag(0x0010, 0x1005));  // Patient's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1010));  // Patient's Age
    removals_.insert(DicomTag(0x0010, 0x1020));  // Patient's Size 
    removals_.insert(DicomTag(0x0010, 0x1030));  // Patient's Weight 
    removals_.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals_.insert(DicomTag(0x0010, 0x1050));  // Insurance Plan Identification
    removals_.insert(DicomTag(0x0010, 0x1060));  // Patient's Mother's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1080));  // Military Rank
    removals_.insert(DicomTag(0x0010, 0x1081));  // Branch of Service
    removals_.insert(DicomTag(0x0010, 0x1090));  // Medical Record Locator
    removals_.insert(DicomTag(0x0010, 0x2000));  // Medical Alerts
    removals_.insert(DicomTag(0x0010, 0x2110));  // Allergies
    removals_.insert(DicomTag(0x0010, 0x2150));  // Country of Residence
    removals_.insert(DicomTag(0x0010, 0x2152));  // Region of Residence
    removals_.insert(DicomTag(0x0010, 0x2154));  // PatientTelephoneNumbers
    removals_.insert(DicomTag(0x0010, 0x2160));  // Ethnic Group
    removals_.insert(DicomTag(0x0010, 0x2180));  // Occupation 
    removals_.insert(DicomTag(0x0010, 0x21a0));  // Smoking Status
    removals_.insert(DicomTag(0x0010, 0x21b0));  // Additional Patient's History
    removals_.insert(DicomTag(0x0010, 0x21c0));  // Pregnancy Status
    removals_.insert(DicomTag(0x0010, 0x21d0));  // Last Menstrual Date
    removals_.insert(DicomTag(0x0010, 0x21f0));  // Patient's Religious Preference
    removals_.insert(DicomTag(0x0010, 0x2203));  // Patient's Sex Neutered
    removals_.insert(DicomTag(0x0010, 0x2297));  // Responsible Person
    removals_.insert(DicomTag(0x0010, 0x2299));  // Responsible Organization
    removals_.insert(DicomTag(0x0010, 0x4000));  // Patient Comments
    removals_.insert(DicomTag(0x0018, 0x0010));  // Contrast Bolus Agent
    removals_.insert(DicomTag(0x0018, 0x1000));  // Device Serial Number
    removals_.insert(DicomTag(0x0018, 0x1002));  // Device UID
    removals_.insert(DicomTag(0x0018, 0x1004));  // Plate ID
    removals_.insert(DicomTag(0x0018, 0x1005));  // Generator ID
    removals_.insert(DicomTag(0x0018, 0x1007));  // Cassette ID
    removals_.insert(DicomTag(0x0018, 0x1008));  // Gantry ID
    removals_.insert(DicomTag(0x0018, 0x1030));  // Protocol Name
    removals_.insert(DicomTag(0x0018, 0x1400));  // Acquisition Device Processing Description
    removals_.insert(DicomTag(0x0018, 0x4000));  // Acquisition Comments
    removals_.insert(DicomTag(0x0018, 0x700a));  // Detector ID
    removals_.insert(DicomTag(0x0018, 0xa003));  // Contribution Description
    removals_.insert(DicomTag(0x0018, 0x9424));  // Acquisition Protocol Description
    //removals_.insert(DicomTag(0x0020, 0x000d));  // Study Instance UID => set in Apply()
    //removals_.insert(DicomTag(0x0020, 0x000e));  // Series Instance UID => set in Apply()
    removals_.insert(DicomTag(0x0020, 0x0010));  // Study ID
    removals_.insert(DicomTag(0x0020, 0x0052));  // Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x3401));  // Modifying Device ID
    removals_.insert(DicomTag(0x0020, 0x3404));  // Modifying Device Manufacturer
    removals_.insert(DicomTag(0x0020, 0x3406));  // Modified Image Description
    removals_.insert(DicomTag(0x0020, 0x4000));  // Image Comments
    removals_.insert(DicomTag(0x0020, 0x9158));  // Frame Comments
    removals_.insert(DicomTag(0x0020, 0x9161));  // Concatenation UID
    removals_.insert(DicomTag(0x0020, 0x9164));  // Dimension Organization UID
    //removals_.insert(DicomTag(0x0028, 0x1199));  // Palette Color Lookup Table UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    //removals_.insert(DicomTag(0x0028, 0x1214));  // Large Palette Color Lookup Table UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    removals_.insert(DicomTag(0x0028, 0x4000));  // Image Presentation Comments
    removals_.insert(DicomTag(0x0032, 0x0012));  // Study ID Issuer
    removals_.insert(DicomTag(0x0032, 0x1020));  // Scheduled Study Location
    removals_.insert(DicomTag(0x0032, 0x1021));  // Scheduled Study Location AE Title
    removals_.insert(DicomTag(0x0032, 0x1030));  // Reason for Study
    removals_.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician
    removals_.insert(DicomTag(0x0032, 0x1033));  // Requesting Service
    removals_.insert(DicomTag(0x0032, 0x1060));  // Requesting Procedure Description
    removals_.insert(DicomTag(0x0032, 0x1070));  // Requested Contrast Agent
    removals_.insert(DicomTag(0x0032, 0x4000));  // Study Comments
    removals_.insert(DicomTag(0x0038, 0x0010));  // Admission ID
    removals_.insert(DicomTag(0x0038, 0x0011));  // Issuer of Admission ID
    removals_.insert(DicomTag(0x0038, 0x001e));  // Scheduled Patient Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0020));  // Admitting Date
    removals_.insert(DicomTag(0x0038, 0x0021));  // Admitting Time
    removals_.insert(DicomTag(0x0038, 0x0040));  // Discharge Diagnosis Description
    removals_.insert(DicomTag(0x0038, 0x0050));  // Special Needs
    removals_.insert(DicomTag(0x0038, 0x0060));  // Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0061));  // Issuer of Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0062));  // Service Episode Description
    removals_.insert(DicomTag(0x0038, 0x0400));  // Patient's Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0500));  // Patient State
    removals_.insert(DicomTag(0x0038, 0x4000));  // Visit Comments
    removals_.insert(DicomTag(0x0038, 0x1234));  // Referenced Patient Alias Sequence
    removals_.insert(DicomTag(0x0040, 0x0001));  // Scheduled Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0002));  // Scheduled Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0003));  // Scheduled Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0004));  // Scheduled Procedure Step End Date
    removals_.insert(DicomTag(0x0040, 0x0005));  // Scheduled Procedure Step End Time
    removals_.insert(DicomTag(0x0040, 0x0006));  // Scheduled Performing Physician Name
    removals_.insert(DicomTag(0x0040, 0x0007));  // Scheduled Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x000b));  // Scheduled Performing Physician Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x0010));  // Scheduled Station Name
    removals_.insert(DicomTag(0x0040, 0x0011));  // Scheduled Procedure Step Location
    removals_.insert(DicomTag(0x0040, 0x0012));  // Pre-Medication
    removals_.insert(DicomTag(0x0040, 0x0241));  // Performed Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0242));  // Performed Station Name
    removals_.insert(DicomTag(0x0040, 0x0243));  // Performed Location
    removals_.insert(DicomTag(0x0040, 0x0244));  // Performed Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0245));  // Performed Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0248));  // Performed Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x0253));  // Performed Procedure Step ID
    removals_.insert(DicomTag(0x0040, 0x0254));  // Performed Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence
    removals_.insert(DicomTag(0x0040, 0x0280));  // Comments on Performed Procedure Step
    removals_.insert(DicomTag(0x0040, 0x0555));  // Acquisition Context Sequence
    removals_.insert(DicomTag(0x0040, 0x1001));  // Requested Procedure ID
    removals_.insert(DicomTag(0x0040, 0x1010));  // Names of Intended Recipient of Results
    removals_.insert(DicomTag(0x0040, 0x1011));  // Intended Recipient of Results Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x1004));  // Patient Transport Arrangements
    removals_.insert(DicomTag(0x0040, 0x1005));  // Requested Procedure Location
    removals_.insert(DicomTag(0x0040, 0x1101));  // Person Identification Code Sequence
    removals_.insert(DicomTag(0x0040, 0x1102));  // Person Address
    removals_.insert(DicomTag(0x0040, 0x1103));  // Person Telephone Numbers
    removals_.insert(DicomTag(0x0040, 0x1400));  // Requested Procedure Comments
    removals_.insert(DicomTag(0x0040, 0x2001));  // Reason for Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2008));  // Order Entered By
    removals_.insert(DicomTag(0x0040, 0x2009));  // Order Enterer Location
    removals_.insert(DicomTag(0x0040, 0x2010));  // Order Callback Phone Number
    removals_.insert(DicomTag(0x0040, 0x2016));  // Placer Order Number of Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2017));  // Filler Order Number of Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2400));  // Imaging Service Request Comments
    removals_.insert(DicomTag(0x0040, 0x4023));  // Referenced General Purpose Scheduled Procedure Step Transaction UID
    removals_.insert(DicomTag(0x0040, 0x4025));  // Scheduled Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4027));  // Scheduled Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4030));  // Performed Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4034));  // Scheduled Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4035));  // Actual Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4036));  // Human Performers Organization
    removals_.insert(DicomTag(0x0040, 0x4037));  // Human Performers Name
    removals_.insert(DicomTag(0x0040, 0xa027));  // Verifying Organization
    removals_.insert(DicomTag(0x0040, 0xa073));  // Verifying Observer Sequence
    removals_.insert(DicomTag(0x0040, 0xa075));  // Verifying Observer Name
    removals_.insert(DicomTag(0x0040, 0xa078));  // Author Observer Sequence
    removals_.insert(DicomTag(0x0040, 0xa07a));  // Participant Sequence
    removals_.insert(DicomTag(0x0040, 0xa07c));  // Custodial Organization Sequence
    removals_.insert(DicomTag(0x0040, 0xa088));  // Verifying Observer Identification Code Sequence
    removals_.insert(DicomTag(0x0040, 0xa123));  // Person Name
    removals_.insert(DicomTag(0x0040, 0xa124));  // UID
    removals_.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    removals_.insert(DicomTag(0x0040, 0x3001));  // Confidentiality Constraint on Patient Data Description
    removals_.insert(DicomTag(0x0040, 0xdb0c));  // Template Extension Organization UID
    removals_.insert(DicomTag(0x0040, 0xdb0d));  // Template Extension Creator UID
    removals_.insert(DicomTag(0x0070, 0x0001));  // Graphic Annotation Sequence
    removals_.insert(DicomTag(0x0070, 0x0084));  // Content Creator's Name
    removals_.insert(DicomTag(0x0070, 0x0086));  // Content Creator's Identification Code Sequence
    removals_.insert(DicomTag(0x0070, 0x031a));  // Fiducial UID
    removals_.insert(DicomTag(0x0088, 0x0140));  // Storage Media File-set UID
    removals_.insert(DicomTag(0x0088, 0x0200));  // Icon Image Sequence
    removals_.insert(DicomTag(0x0088, 0x0904));  // Topic Title
    removals_.insert(DicomTag(0x0088, 0x0906));  // Topic Subject
    removals_.insert(DicomTag(0x0088, 0x0910));  // Topic Author
    removals_.insert(DicomTag(0x0088, 0x0912));  // Topic Key Words
    removals_.insert(DicomTag(0x0400, 0x0100));  // Digital Signature UID
    removals_.insert(DicomTag(0x0400, 0x0402));  // Referenced Digital Signature Sequence
    removals_.insert(DicomTag(0x0400, 0x0403));  // Referenced SOP Instance MAC Sequence
    removals_.insert(DicomTag(0x0400, 0x0404));  // MAC
    removals_.insert(DicomTag(0x0400, 0x0550));  // Modified Attributes Sequence
    removals_.insert(DicomTag(0x0400, 0x0561));  // Original Attributes Sequence
    removals_.insert(DicomTag(0x2030, 0x0020));  // Text String
    removals_.insert(DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID
    removals_.insert(DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID 
    removals_.insert(DicomTag(0x300a, 0x0013));  // Dose Reference UID
    removals_.insert(DicomTag(0x300e, 0x0008));  // Reviewer Name
    removals_.insert(DicomTag(0x4000, 0x0010));  // Arbitrary
    removals_.insert(DicomTag(0x4000, 0x4000));  // Text Comments
    removals_.insert(DicomTag(0x4008, 0x0042));  // Results ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0102));  // Interpretation Recorder
    removals_.insert(DicomTag(0x4008, 0x010a));  // Interpretation Transcriber
    removals_.insert(DicomTag(0x4008, 0x010b));  // Interpretation Text
    removals_.insert(DicomTag(0x4008, 0x010c));  // Interpretation Author
    removals_.insert(DicomTag(0x4008, 0x0111));  // Interpretation Approver Sequence
    removals_.insert(DicomTag(0x4008, 0x0114));  // Physician Approving Interpretation
    removals_.insert(DicomTag(0x4008, 0x0115));  // Interpretation Diagnosis Description
    removals_.insert(DicomTag(0x4008, 0x0118));  // Results Distribution List Sequence
    removals_.insert(DicomTag(0x4008, 0x0119));  // Distribution Name
    removals_.insert(DicomTag(0x4008, 0x011a));  // Distribution Address
    removals_.insert(DicomTag(0x4008, 0x0202));  // Interpretation ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0300));  // Impressions
    removals_.insert(DicomTag(0x4008, 0x4000));  // Results Comments
    removals_.insert(DicomTag(0xfffa, 0xfffa));  // Digital Signature Sequence
    removals_.insert(DicomTag(0xfffc, 0xfffc));  // Data Set Trailing Padding
    //removals_.insert(DicomTag(0x60xx, 0x4000));  // Overlay Comments => TODO
    //removals_.insert(DicomTag(0x60xx, 0x3000));  // Overlay Data => TODO

    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2011);
  }
#endif
  
  

  void DicomModification::SetupAnonymization2017c()
  {
    /**
     * This is Table E.1-1 from PS 3.15-2017c (DICOM Part 15: Security
     * and System Management Profiles), "basic profile" column. It was
     * generated automatically with the
     * "../Resources/GenerateAnonymizationProfile.py" script.
     * https://raw.githubusercontent.com/jodogne/dicom-specification/master/2017c/part15.pdf
     **/
    
    // TODO: (50xx,xxxx) with rule X                                 // Curve Data
    // TODO: (60xx,3000) with rule X                                 // Overlay Data
    // TODO: (60xx,4000) with rule X                                 // Overlay Comments
    // Tag (0x0008, 0x0018) is set in Apply()                        // SOP Instance UID
    // Tag (0x0010, 0x0010) is set below (*)                         // Patient's Name
    // Tag (0x0010, 0x0020) is set below (*)                         // Patient ID
    // Tag (0x0020, 0x000d) is set in Apply()                        // Study Instance UID
    // Tag (0x0020, 0x000e) is set in Apply()                        // Series Instance UID
    clearings_.insert(DicomTag(0x0008, 0x0020));                     // Study Date
    clearings_.insert(DicomTag(0x0008, 0x0023));  /* Z/D */          // Content Date
    clearings_.insert(DicomTag(0x0008, 0x0030));                     // Study Time
    clearings_.insert(DicomTag(0x0008, 0x0033));  /* Z/D */          // Content Time
    clearings_.insert(DicomTag(0x0008, 0x0050));                     // Accession Number
    clearings_.insert(DicomTag(0x0008, 0x0090));                     // Referring Physician's Name
    clearings_.insert(DicomTag(0x0008, 0x009c));                     // Consulting Physician's Name
    clearings_.insert(DicomTag(0x0010, 0x0030));                     // Patient's Birth Date
    clearings_.insert(DicomTag(0x0010, 0x0040));                     // Patient's Sex
    clearings_.insert(DicomTag(0x0018, 0x0010));  /* Z/D */          // Contrast Bolus Agent
    clearings_.insert(DicomTag(0x0020, 0x0010));                     // Study ID
    clearings_.insert(DicomTag(0x0040, 0x1101));  /* D */            // Person Identification Code Sequence
    clearings_.insert(DicomTag(0x0040, 0x2016));                     // Placer Order Number / Imaging Service Request
    clearings_.insert(DicomTag(0x0040, 0x2017));                     // Filler Order Number / Imaging Service Request
    clearings_.insert(DicomTag(0x0040, 0xa073));  /* D */            // Verifying Observer Sequence
    clearings_.insert(DicomTag(0x0040, 0xa075));  /* D */            // Verifying Observer Name
    clearings_.insert(DicomTag(0x0040, 0xa088));                     // Verifying Observer Identification Code Sequence
    clearings_.insert(DicomTag(0x0040, 0xa123));  /* D */            // Person Name
    clearings_.insert(DicomTag(0x0070, 0x0001));  /* D */            // Graphic Annotation Sequence
    clearings_.insert(DicomTag(0x0070, 0x0084));                     // Content Creator's Name
    removals_.insert(DicomTag(0x0000, 0x1000));                      // Affected SOP Instance UID
    removals_.insert(DicomTag(0x0000, 0x1001));   /* TODO UID */     // Requested SOP Instance UID
    removals_.insert(DicomTag(0x0002, 0x0003));   /* TODO UID */     // Media Storage SOP Instance UID
    removals_.insert(DicomTag(0x0004, 0x1511));   /* TODO UID */     // Referenced SOP Instance UID in File
    removals_.insert(DicomTag(0x0008, 0x0014));   /* TODO UID */     // Instance Creator UID
    removals_.insert(DicomTag(0x0008, 0x0015));                      // Instance Coercion DateTime
    removals_.insert(DicomTag(0x0008, 0x0021));   /* X/D */          // Series Date
    removals_.insert(DicomTag(0x0008, 0x0022));   /* X/Z */          // Acquisition Date
    removals_.insert(DicomTag(0x0008, 0x0024));                      // Overlay Date
    removals_.insert(DicomTag(0x0008, 0x0025));                      // Curve Date
    removals_.insert(DicomTag(0x0008, 0x002a));   /* X/D */          // Acquisition DateTime
    removals_.insert(DicomTag(0x0008, 0x0031));   /* X/D */          // Series Time
    removals_.insert(DicomTag(0x0008, 0x0032));   /* X/Z */          // Acquisition Time
    removals_.insert(DicomTag(0x0008, 0x0034));                      // Overlay Time
    removals_.insert(DicomTag(0x0008, 0x0035));                      // Curve Time
    removals_.insert(DicomTag(0x0008, 0x0058));   /* TODO UID */     // Failed SOP Instance UID List
    removals_.insert(DicomTag(0x0008, 0x0080));   /* X/Z/D */        // Institution Name
    removals_.insert(DicomTag(0x0008, 0x0081));                      // Institution Address
    removals_.insert(DicomTag(0x0008, 0x0082));   /* X/Z/D */        // Institution Code Sequence
    removals_.insert(DicomTag(0x0008, 0x0092));                      // Referring Physician's Address
    removals_.insert(DicomTag(0x0008, 0x0094));                      // Referring Physician's Telephone Numbers
    removals_.insert(DicomTag(0x0008, 0x0096));                      // Referring Physician Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x009d));                      // Consulting Physician Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x0201));                      // Timezone Offset From UTC
    removals_.insert(DicomTag(0x0008, 0x1010));   /* X/Z/D */        // Station Name
    removals_.insert(DicomTag(0x0008, 0x1030));                      // Study Description
    removals_.insert(DicomTag(0x0008, 0x103e));                      // Series Description
    removals_.insert(DicomTag(0x0008, 0x1040));                      // Institutional Department Name
    removals_.insert(DicomTag(0x0008, 0x1048));                      // Physician(s) of Record
    removals_.insert(DicomTag(0x0008, 0x1049));                      // Physician(s) of Record Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1050));                      // Performing Physicians' Name
    removals_.insert(DicomTag(0x0008, 0x1052));                      // Performing Physician Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1060));                      // Name of Physician(s) Reading Study
    removals_.insert(DicomTag(0x0008, 0x1062));                      // Physician(s) Reading Study Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1070));   /* X/Z/D */        // Operators' Name
    removals_.insert(DicomTag(0x0008, 0x1072));   /* X/D */          // Operators' Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1080));                      // Admitting Diagnoses Description
    removals_.insert(DicomTag(0x0008, 0x1084));                      // Admitting Diagnoses Code Sequence
    removals_.insert(DicomTag(0x0008, 0x1110));   /* X/Z */          // Referenced Study Sequence
    removals_.insert(DicomTag(0x0008, 0x1111));   /* X/Z/D */        // Referenced Performed Procedure Step Sequence
    removals_.insert(DicomTag(0x0008, 0x1120));                      // Referenced Patient Sequence
    removals_.insert(DicomTag(0x0008, 0x1140));   /* X/Z/U* */       // Referenced Image Sequence
    removals_.insert(DicomTag(0x0008, 0x1155));   /* TODO UID */     // Referenced SOP Instance UID
    removals_.insert(DicomTag(0x0008, 0x1195));   /* TODO UID */     // Transaction UID
    removals_.insert(DicomTag(0x0008, 0x2111));                      // Derivation Description
    removals_.insert(DicomTag(0x0008, 0x2112));   /* X/Z/U* */       // Source Image Sequence
    removals_.insert(DicomTag(0x0008, 0x3010));   /* TODO UID */     // Irradiation Event UID
    removals_.insert(DicomTag(0x0008, 0x4000));                      // Identifying Comments
    removals_.insert(DicomTag(0x0010, 0x0021));                      // Issuer of Patient ID
    removals_.insert(DicomTag(0x0010, 0x0032));                      // Patient's Birth Time
    removals_.insert(DicomTag(0x0010, 0x0050));                      // Patient's Insurance Plan Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0101));                      // Patient's Primary Language Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0102));                      // Patient's Primary Language Modifier Code Sequence
    removals_.insert(DicomTag(0x0010, 0x1000));                      // Other Patient IDs
    removals_.insert(DicomTag(0x0010, 0x1001));                      // Other Patient Names
    removals_.insert(DicomTag(0x0010, 0x1002));                      // Other Patient IDs Sequence
    removals_.insert(DicomTag(0x0010, 0x1005));                      // Patient's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1010));                      // Patient's Age
    removals_.insert(DicomTag(0x0010, 0x1020));                      // Patient's Size
    removals_.insert(DicomTag(0x0010, 0x1030));                      // Patient's Weight
    removals_.insert(DicomTag(0x0010, 0x1040));                      // Patient Address
    removals_.insert(DicomTag(0x0010, 0x1050));                      // Insurance Plan Identification
    removals_.insert(DicomTag(0x0010, 0x1060));                      // Patient's Mother's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1080));                      // Military Rank
    removals_.insert(DicomTag(0x0010, 0x1081));                      // Branch of Service
    removals_.insert(DicomTag(0x0010, 0x1090));                      // Medical Record Locator
    removals_.insert(DicomTag(0x0010, 0x1100));                      // Referenced Patient Photo Sequence
    removals_.insert(DicomTag(0x0010, 0x2000));                      // Medical Alerts
    removals_.insert(DicomTag(0x0010, 0x2110));                      // Allergies
    removals_.insert(DicomTag(0x0010, 0x2150));                      // Country of Residence
    removals_.insert(DicomTag(0x0010, 0x2152));                      // Region of Residence
    removals_.insert(DicomTag(0x0010, 0x2154));                      // Patient's Telephone Numbers
    removals_.insert(DicomTag(0x0010, 0x2155));                      // Patient's Telecom Information
    removals_.insert(DicomTag(0x0010, 0x2160));                      // Ethnic Group
    removals_.insert(DicomTag(0x0010, 0x2180));                      // Occupation
    removals_.insert(DicomTag(0x0010, 0x21a0));                      // Smoking Status
    removals_.insert(DicomTag(0x0010, 0x21b0));                      // Additional Patient's History
    removals_.insert(DicomTag(0x0010, 0x21c0));                      // Pregnancy Status
    removals_.insert(DicomTag(0x0010, 0x21d0));                      // Last Menstrual Date
    removals_.insert(DicomTag(0x0010, 0x21f0));                      // Patient's Religious Preference
    removals_.insert(DicomTag(0x0010, 0x2203));   /* X/Z */          // Patient Sex Neutered
    removals_.insert(DicomTag(0x0010, 0x2297));                      // Responsible Person
    removals_.insert(DicomTag(0x0010, 0x2299));                      // Responsible Organization
    removals_.insert(DicomTag(0x0010, 0x4000));                      // Patient Comments
    removals_.insert(DicomTag(0x0018, 0x1000));   /* X/Z/D */        // Device Serial Number
    removals_.insert(DicomTag(0x0018, 0x1002));   /* TODO UID */     // Device UID
    removals_.insert(DicomTag(0x0018, 0x1004));                      // Plate ID
    removals_.insert(DicomTag(0x0018, 0x1005));                      // Generator ID
    removals_.insert(DicomTag(0x0018, 0x1007));                      // Cassette ID
    removals_.insert(DicomTag(0x0018, 0x1008));                      // Gantry ID
    removals_.insert(DicomTag(0x0018, 0x1030));   /* X/D */          // Protocol Name
    removals_.insert(DicomTag(0x0018, 0x1400));   /* X/D */          // Acquisition Device Processing Description
    removals_.insert(DicomTag(0x0018, 0x2042));   /* TODO UID */     // Target UID
    removals_.insert(DicomTag(0x0018, 0x4000));                      // Acquisition Comments
    removals_.insert(DicomTag(0x0018, 0x700a));   /* X/D */          // Detector ID
    removals_.insert(DicomTag(0x0018, 0x9424));                      // Acquisition Protocol Description
    removals_.insert(DicomTag(0x0018, 0x9516));   /* X/D */          // Start Acquisition DateTime
    removals_.insert(DicomTag(0x0018, 0x9517));   /* X/D */          // End Acquisition DateTime
    removals_.insert(DicomTag(0x0018, 0xa003));                      // Contribution Description
    removals_.insert(DicomTag(0x0020, 0x0052));   /* TODO UID */     // Frame of Reference UID
    removals_.insert(DicomTag(0x0020, 0x0200));   /* TODO UID */     // Synchronization Frame of Reference UID
    removals_.insert(DicomTag(0x0020, 0x3401));                      // Modifying Device ID
    removals_.insert(DicomTag(0x0020, 0x3404));                      // Modifying Device Manufacturer
    removals_.insert(DicomTag(0x0020, 0x3406));                      // Modified Image Description
    removals_.insert(DicomTag(0x0020, 0x4000));                      // Image Comments
    removals_.insert(DicomTag(0x0020, 0x9158));                      // Frame Comments
    removals_.insert(DicomTag(0x0020, 0x9161));   /* TODO UID */     // Concatenation UID
    removals_.insert(DicomTag(0x0020, 0x9164));   /* TODO UID */     // Dimension Organization UID
    removals_.insert(DicomTag(0x0028, 0x1199));   /* TODO UID */     // Palette Color Lookup Table UID
    removals_.insert(DicomTag(0x0028, 0x1214));   /* TODO UID */     // Large Palette Color Lookup Table UID
    removals_.insert(DicomTag(0x0028, 0x4000));                      // Image Presentation Comments
    removals_.insert(DicomTag(0x0032, 0x0012));                      // Study ID Issuer
    removals_.insert(DicomTag(0x0032, 0x1020));                      // Scheduled Study Location
    removals_.insert(DicomTag(0x0032, 0x1021));                      // Scheduled Study Location AE Title
    removals_.insert(DicomTag(0x0032, 0x1030));                      // Reason for Study
    removals_.insert(DicomTag(0x0032, 0x1032));                      // Requesting Physician
    removals_.insert(DicomTag(0x0032, 0x1033));                      // Requesting Service
    removals_.insert(DicomTag(0x0032, 0x1060));   /* X/Z */          // Requested Procedure Description
    removals_.insert(DicomTag(0x0032, 0x1070));                      // Requested Contrast Agent
    removals_.insert(DicomTag(0x0032, 0x4000));                      // Study Comments
    removals_.insert(DicomTag(0x0038, 0x0004));                      // Referenced Patient Alias Sequence
    removals_.insert(DicomTag(0x0038, 0x0010));                      // Admission ID
    removals_.insert(DicomTag(0x0038, 0x0011));                      // Issuer of Admission ID
    removals_.insert(DicomTag(0x0038, 0x001e));                      // Scheduled Patient Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0020));                      // Admitting Date
    removals_.insert(DicomTag(0x0038, 0x0021));                      // Admitting Time
    removals_.insert(DicomTag(0x0038, 0x0040));                      // Discharge Diagnosis Description
    removals_.insert(DicomTag(0x0038, 0x0050));                      // Special Needs
    removals_.insert(DicomTag(0x0038, 0x0060));                      // Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0061));                      // Issuer of Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0062));                      // Service Episode Description
    removals_.insert(DicomTag(0x0038, 0x0300));                      // Current Patient Location
    removals_.insert(DicomTag(0x0038, 0x0400));                      // Patient's Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0500));                      // Patient State
    removals_.insert(DicomTag(0x0038, 0x4000));                      // Visit Comments
    removals_.insert(DicomTag(0x0040, 0x0001));                      // Scheduled Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0002));                      // Scheduled Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0003));                      // Scheduled Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0004));                      // Scheduled Procedure Step End Date
    removals_.insert(DicomTag(0x0040, 0x0005));                      // Scheduled Procedure Step End Time
    removals_.insert(DicomTag(0x0040, 0x0006));                      // Scheduled Performing Physician Name
    removals_.insert(DicomTag(0x0040, 0x0007));                      // Scheduled Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x000b));                      // Scheduled Performing Physician Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x0010));                      // Scheduled Station Name
    removals_.insert(DicomTag(0x0040, 0x0011));                      // Scheduled Procedure Step Location
    removals_.insert(DicomTag(0x0040, 0x0012));                      // Pre-Medication
    removals_.insert(DicomTag(0x0040, 0x0241));                      // Performed Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0242));                      // Performed Station Name
    removals_.insert(DicomTag(0x0040, 0x0243));                      // Performed Location
    removals_.insert(DicomTag(0x0040, 0x0244));                      // Performed Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0245));                      // Performed Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0250));                      // Performed Procedure Step End Date
    removals_.insert(DicomTag(0x0040, 0x0251));                      // Performed Procedure Step End Time
    removals_.insert(DicomTag(0x0040, 0x0253));                      // Performed Procedure Step ID
    removals_.insert(DicomTag(0x0040, 0x0254));                      // Performed Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x0275));                      // Request Attributes Sequence
    removals_.insert(DicomTag(0x0040, 0x0280));                      // Comments on the Performed Procedure Step
    removals_.insert(DicomTag(0x0040, 0x0555));                      // Acquisition Context Sequence
    removals_.insert(DicomTag(0x0040, 0x1001));                      // Requested Procedure ID
    removals_.insert(DicomTag(0x0040, 0x1004));                      // Patient Transport Arrangements
    removals_.insert(DicomTag(0x0040, 0x1005));                      // Requested Procedure Location
    removals_.insert(DicomTag(0x0040, 0x1010));                      // Names of Intended Recipient of Results
    removals_.insert(DicomTag(0x0040, 0x1011));                      // Intended Recipients of Results Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x1102));                      // Person Address
    removals_.insert(DicomTag(0x0040, 0x1103));                      // Person's Telephone Numbers
    removals_.insert(DicomTag(0x0040, 0x1104));                      // Person's Telecom Information
    removals_.insert(DicomTag(0x0040, 0x1400));                      // Requested Procedure Comments
    removals_.insert(DicomTag(0x0040, 0x2001));                      // Reason for the Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2008));                      // Order Entered By
    removals_.insert(DicomTag(0x0040, 0x2009));                      // Order Enterer Location
    removals_.insert(DicomTag(0x0040, 0x2010));                      // Order Callback Phone Number
    removals_.insert(DicomTag(0x0040, 0x2011));                      // Order Callback Telecom Information
    removals_.insert(DicomTag(0x0040, 0x2400));                      // Imaging Service Request Comments
    removals_.insert(DicomTag(0x0040, 0x3001));                      // Confidentiality Constraint on Patient Data Description
    removals_.insert(DicomTag(0x0040, 0x4005));                      // Scheduled Procedure Step Start DateTime
    removals_.insert(DicomTag(0x0040, 0x4010));                      // Scheduled Procedure Step Modification DateTime
    removals_.insert(DicomTag(0x0040, 0x4011));                      // Expected Completion DateTime
    removals_.insert(DicomTag(0x0040, 0x4023));   /* TODO UID */     // Referenced General Purpose Scheduled Procedure Step Transaction UID
    removals_.insert(DicomTag(0x0040, 0x4025));                      // Scheduled Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4027));                      // Scheduled Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4028));                      // Performed Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4030));                      // Performed Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4034));                      // Scheduled Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4035));                      // Actual Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4036));                      // Human Performers Organization
    removals_.insert(DicomTag(0x0040, 0x4037));                      // Human Performers Name
    removals_.insert(DicomTag(0x0040, 0x4050));                      // Performed Procedure Step Start DateTime
    removals_.insert(DicomTag(0x0040, 0x4051));                      // Performed Procedure Step End DateTime
    removals_.insert(DicomTag(0x0040, 0x4052));                      // Procedure Step Cancellation DateTime
    removals_.insert(DicomTag(0x0040, 0xa027));                      // Verifying Organization
    removals_.insert(DicomTag(0x0040, 0xa078));                      // Author Observer Sequence
    removals_.insert(DicomTag(0x0040, 0xa07a));                      // Participant Sequence
    removals_.insert(DicomTag(0x0040, 0xa07c));                      // Custodial Organization Sequence
    removals_.insert(DicomTag(0x0040, 0xa124));   /* TODO UID */     // UID
    removals_.insert(DicomTag(0x0040, 0xa171));   /* TODO UID */     // Observation UID
    removals_.insert(DicomTag(0x0040, 0xa172));   /* TODO UID */     // Referenced Observation UID (Trial)
    removals_.insert(DicomTag(0x0040, 0xa192));                      // Observation Date (Trial)
    removals_.insert(DicomTag(0x0040, 0xa193));                      // Observation Time (Trial)
    removals_.insert(DicomTag(0x0040, 0xa307));                      // Current Observer (Trial)
    removals_.insert(DicomTag(0x0040, 0xa352));                      // Verbal Source (Trial)
    removals_.insert(DicomTag(0x0040, 0xa353));                      // Address (Trial)
    removals_.insert(DicomTag(0x0040, 0xa354));                      // Telephone Number (Trial)
    removals_.insert(DicomTag(0x0040, 0xa358));                      // Verbal Source Identifier Code Sequence (Trial)
    removals_.insert(DicomTag(0x0040, 0xa402));   /* TODO UID */     // Observation Subject UID (Trial)
    removals_.insert(DicomTag(0x0040, 0xa730));                      // Content Sequence
    removals_.insert(DicomTag(0x0040, 0xdb0c));   /* TODO UID */     // Template Extension Organization UID
    removals_.insert(DicomTag(0x0040, 0xdb0d));   /* TODO UID */     // Template Extension Creator UID
    removals_.insert(DicomTag(0x0062, 0x0021));   /* TODO UID */     // Tracking UID
    removals_.insert(DicomTag(0x0070, 0x0086));                      // Content Creator's Identification Code Sequence
    removals_.insert(DicomTag(0x0070, 0x031a));   /* TODO UID */     // Fiducial UID
    removals_.insert(DicomTag(0x0070, 0x1101));   /* TODO UID */     // Presentation Display Collection UID
    removals_.insert(DicomTag(0x0070, 0x1102));   /* TODO UID */     // Presentation Sequence Collection UID
    removals_.insert(DicomTag(0x0088, 0x0140));   /* TODO UID */     // Storage Media File-set UID
    removals_.insert(DicomTag(0x0088, 0x0200));                      // Icon Image Sequence(see Note 12)
    removals_.insert(DicomTag(0x0088, 0x0904));                      // Topic Title
    removals_.insert(DicomTag(0x0088, 0x0906));                      // Topic Subject
    removals_.insert(DicomTag(0x0088, 0x0910));                      // Topic Author
    removals_.insert(DicomTag(0x0088, 0x0912));                      // Topic Keywords
    removals_.insert(DicomTag(0x0400, 0x0100));                      // Digital Signature UID
    removals_.insert(DicomTag(0x0400, 0x0402));                      // Referenced Digital Signature Sequence
    removals_.insert(DicomTag(0x0400, 0x0403));                      // Referenced SOP Instance MAC Sequence
    removals_.insert(DicomTag(0x0400, 0x0404));                      // MAC
    removals_.insert(DicomTag(0x0400, 0x0550));                      // Modified Attributes Sequence
    removals_.insert(DicomTag(0x0400, 0x0561));                      // Original Attributes Sequence
    removals_.insert(DicomTag(0x2030, 0x0020));                      // Text String
    removals_.insert(DicomTag(0x3006, 0x0024));   /* TODO UID */     // Referenced Frame of Reference UID
    removals_.insert(DicomTag(0x3006, 0x00c2));   /* TODO UID */     // Related Frame of Reference UID
    removals_.insert(DicomTag(0x3008, 0x0105));                      // Source Serial Number
    removals_.insert(DicomTag(0x300a, 0x0013));   /* TODO UID */     // Dose Reference UID
    removals_.insert(DicomTag(0x300c, 0x0113));                      // Reason for Omission Description
    removals_.insert(DicomTag(0x300e, 0x0008));   /* X/Z */          // Reviewer Name
    removals_.insert(DicomTag(0x4000, 0x0010));                      // Arbitrary
    removals_.insert(DicomTag(0x4000, 0x4000));                      // Text Comments
    removals_.insert(DicomTag(0x4008, 0x0042));                      // Results ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0102));                      // Interpretation Recorder
    removals_.insert(DicomTag(0x4008, 0x010a));                      // Interpretation Transcriber
    removals_.insert(DicomTag(0x4008, 0x010b));                      // Interpretation Text
    removals_.insert(DicomTag(0x4008, 0x010c));                      // Interpretation Author
    removals_.insert(DicomTag(0x4008, 0x0111));                      // Interpretation Approver Sequence
    removals_.insert(DicomTag(0x4008, 0x0114));                      // Physician Approving Interpretation
    removals_.insert(DicomTag(0x4008, 0x0115));                      // Interpretation Diagnosis Description
    removals_.insert(DicomTag(0x4008, 0x0118));                      // Results Distribution List Sequence
    removals_.insert(DicomTag(0x4008, 0x0119));                      // Distribution Name
    removals_.insert(DicomTag(0x4008, 0x011a));                      // Distribution Address
    removals_.insert(DicomTag(0x4008, 0x0202));                      // Interpretation ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0300));                      // Impressions
    removals_.insert(DicomTag(0x4008, 0x4000));                      // Results Comments
    removals_.insert(DicomTag(0xfffa, 0xfffa));                      // Digital Signatures Sequence
    removals_.insert(DicomTag(0xfffc, 0xfffc));                      // Data Set Trailing Padding
    
    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2017c);
  }
  

  void DicomModification::SetupAnonymization(DicomVersion version)
  {
    removals_.clear();
    clearings_.clear();
    ClearReplacements();
    removePrivateTags_ = true;
    level_ = ResourceType_Patient;
    uidMap_.clear();
    privateTagsToKeep_.clear();

    switch (version)
    {
      case DicomVersion_2008:
        SetupAnonymization2008();
        break;

      case DicomVersion_2017c:
        SetupAnonymization2017c();
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Set the PatientIdentityRemoved tag
    ReplaceInternal(DicomTag(0x0012, 0x0062), "YES");

    // (*) Choose a random patient name and ID
    std::string patientId = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Patient);
    ReplaceInternal(DICOM_TAG_PATIENT_ID, patientId);
    ReplaceInternal(DICOM_TAG_PATIENT_NAME, patientId);
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
    if (level_ == ResourceType_Patient && !IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      LOG(ERROR) << "When modifying a patient, her PatientID is required to be modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a patient, the StudyInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }

      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a patient, the SeriesInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }

      if (level_ == ResourceType_Patient && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a patient, the SopInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }
    }


    // Sanity checks at the study level
    if (level_ == ResourceType_Study && IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      LOG(ERROR) << "When modifying a study, the parent PatientID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Study && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a study, the SeriesInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }

      if (level_ == ResourceType_Study && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a study, the SopInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }
    }


    // Sanity checks at the series level
    if (level_ == ResourceType_Series && IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      LOG(ERROR) << "When modifying a series, the parent PatientID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (level_ == ResourceType_Series && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      LOG(ERROR) << "When modifying a series, the parent StudyInstanceUID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (!allowManualIdentifiers_)
    {
      if (level_ == ResourceType_Series && IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
      {
        LOG(ERROR) << "When modifying a series, the SopInstanceUID cannot be manually modified";
        throw OrthancException(ErrorCode_BadRequest);
      }
    }


    // Sanity checks at the instance level
    if (level_ == ResourceType_Instance && IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      LOG(ERROR) << "When modifying an instance, the parent PatientID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (level_ == ResourceType_Instance && IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      LOG(ERROR) << "When modifying an instance, the parent StudyInstanceUID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (level_ == ResourceType_Instance && IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      LOG(ERROR) << "When modifying an instance, the parent SeriesInstanceUID cannot be manually modified";
      throw OrthancException(ErrorCode_BadRequest);
    }


    // (1) Remove the private tags, if need be
    if (removePrivateTags_)
    {
      toModify.RemovePrivateTags(privateTagsToKeep_);
    }

    // (2) Clear the tags specified by the user
    for (SetOfTags::const_iterator it = clearings_.begin(); 
         it != clearings_.end(); ++it)
    {
      toModify.Clear(*it, true /* only clear if the tag exists in the original file */);
    }

    // (3) Remove the tags specified by the user
    for (SetOfTags::const_iterator it = removals_.begin(); 
         it != removals_.end(); ++it)
    {
      toModify.Remove(*it);
    }

    // (4) Replace the tags
    for (Replacements::const_iterator it = replacements_.begin(); 
         it != replacements_.end(); ++it)
    {
      toModify.Replace(it->first, *it->second, true /* decode data URI scheme */, DicomReplaceMode_InsertIfAbsent);
    }

    // (5) Update the DICOM identifiers
    if (level_ <= ResourceType_Study &&
        !IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      if (keepStudyInstanceUid_)
      {
        LOG(WARNING) << "Modifying a study while keeping its original StudyInstanceUID: This should be avoided!";
      }
      else
      {
        MapDicomIdentifier(toModify, ResourceType_Study);
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
        MapDicomIdentifier(toModify, ResourceType_Series);
      }
    }

    if (level_ <= ResourceType_Instance &&  // Always true
        !IsReplaced(DICOM_TAG_SOP_INSTANCE_UID))
    {
      MapDicomIdentifier(toModify, ResourceType_Instance);
    }
  }
}
