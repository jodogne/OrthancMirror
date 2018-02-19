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





/*=========================================================================

  This file is based on portions of the following project:

  Program: DCMTK 3.6.0
  Module:  http://dicom.offis.de/dcmtk.php.en

Copyright (C) 1994-2011, OFFIS e.V.
All rights reserved.

This software and supporting documentation were developed by

  OFFIS e.V.
  R&D Division Health
  Escherweg 2
  26121 Oldenburg, Germany

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

- Neither the name of OFFIS nor the names of its contributors may be
  used to endorse or promote products derived from this software
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/



/***
    
    Validation:

    # sudo apt-get install dicom3tools
    # dciodvfy DICOMDIR 2>&1 | less
    # dcentvfy DICOMDIR 2>&1 | less

    http://www.dclunie.com/dicom3tools/dciodvfy.html

    DICOMDIR viewer working with Wine under Linux:
    http://www.microdicom.com/

 ***/


#include "../PrecompiledHeaders.h"
#include "DicomDirWriter.h"

#include "FromDcmtkBridge.h"
#include "ToDcmtkBridge.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../TemporaryFile.h"
#include "../Toolbox.h"
#include "../SystemToolbox.h"

#include <dcmtk/dcmdata/dcdicdir.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcddirif.h>
#include <dcmtk/dcmdata/dcvrui.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcostrmf.h>
#include "dcmtk/dcmdata/dcvrda.h"     /* for class DcmDate */
#include "dcmtk/dcmdata/dcvrtm.h"     /* for class DcmTime */

#include <memory>

namespace Orthanc
{
  class DicomDirWriter::PImpl
  {
  private:
    bool                       utc_;
    std::string                fileSetId_;
    bool                       extendedSopClass_;
    TemporaryFile              file_;
    std::auto_ptr<DcmDicomDir> dir_;

    typedef std::pair<ResourceType, std::string>  IndexKey;
    typedef std::map<IndexKey, DcmDirectoryRecord* >  Index;
    Index  index_;


    DcmDicomDir& GetDicomDir()
    {
      if (dir_.get() == NULL)
      {
        dir_.reset(new DcmDicomDir(file_.GetPath().c_str(), 
                                   fileSetId_.c_str()));
        //SetTagValue(dir_->getRootRecord(), DCM_SpecificCharacterSet, GetDicomSpecificCharacterSet(Encoding_Utf8));
      }

      return *dir_;
    }


    DcmDirectoryRecord& GetRoot()
    {
      return GetDicomDir().getRootRecord();
    }


    static bool GetUtf8TagValue(std::string& result,
                                DcmItem& source,
                                Encoding encoding,
                                const DcmTagKey& key)
    {
      DcmElement* element = NULL;

      if (source.findAndGetElement(key, element).good())
      {
        char* s = NULL;
        if (element->isLeaf() &&
            element->getString(s).good() &&
            s != NULL)
        {
          result = Toolbox::ConvertToUtf8(s, encoding);
          return true;
        }
      }

      result.clear();
      return false;
    }


    static void SetTagValue(DcmDirectoryRecord& target,
                            const DcmTagKey& key,
                            const std::string& valueUtf8)
    {
      std::string s = Toolbox::ConvertFromUtf8(valueUtf8, Encoding_Ascii);

      if (!target.putAndInsertString(key, s.c_str()).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
                            


    static bool CopyString(DcmDirectoryRecord& target,
                           DcmDataset& source,
                           Encoding encoding,
                           const DcmTagKey& key,
                           bool optional,
                           bool copyEmpty)
    {
      if (optional &&
          !source.tagExistsWithValue(key) &&
          !(copyEmpty && source.tagExists(key)))
      {
        return false;
      }

      std::string value;
      bool found = GetUtf8TagValue(value, source, encoding, key);

      if (!found)
      {
        // We don't raise an exception if "!optional", even if this
        // results in an invalid DICOM file
        value.clear();
      }

      SetTagValue(target, key, value);
      return found;
    }


    static void CopyStringType1(DcmDirectoryRecord& target,
                                DcmDataset& source,
                                Encoding encoding,
                                const DcmTagKey& key)
    {
      CopyString(target, source, encoding, key, false, false);
    }

    static void CopyStringType1C(DcmDirectoryRecord& target,
                                 DcmDataset& source,
                                 Encoding encoding,
                                 const DcmTagKey& key)
    {
      CopyString(target, source, encoding, key, true, false);
    }

    static void CopyStringType2(DcmDirectoryRecord& target,
                                DcmDataset& source,
                                Encoding encoding,
                                const DcmTagKey& key)
    {
      CopyString(target, source, encoding, key, false, true);
    }

    static void CopyStringType3(DcmDirectoryRecord& target,
                                DcmDataset& source,
                                Encoding encoding,
                                const DcmTagKey& key)
    {
      CopyString(target, source, encoding, key, true, true);
    }


  public:
    PImpl() :
      utc_(true),   // By default, use UTC (universal time, not local time)
      fileSetId_("ORTHANC_MEDIA"),
      extendedSopClass_(false)
    {
    }
    
    bool IsUtcUsed() const
    {
      return utc_;
    }


    void SetUtcUsed(bool utc)
    {
      utc_ = utc;
    }
    
    void EnableExtendedSopClass(bool enable)
    {
      if (enable)
      {
        LOG(WARNING) << "Generating a DICOMDIR with type 3 attributes, "
                     << "which leads to an Extended SOP Class";
      }
      
      extendedSopClass_ = enable;
    }

    bool IsExtendedSopClass() const
    {
      return extendedSopClass_;
    }

    void FillPatient(DcmDirectoryRecord& record,
                     DcmDataset& dicom,
                     Encoding encoding)
    {
      // cf. "DicomDirInterface::buildPatientRecord()"

      CopyStringType1C(record, dicom, encoding, DCM_PatientID);
      CopyStringType2(record, dicom, encoding, DCM_PatientName);
    }

    void FillStudy(DcmDirectoryRecord& record,
                   DcmDataset& dicom,
                   Encoding encoding)
    {
      // cf. "DicomDirInterface::buildStudyRecord()"

      std::string nowDate, nowTime;
      SystemToolbox::GetNowDicom(nowDate, nowTime, utc_);

      std::string studyDate;
      if (!GetUtf8TagValue(studyDate, dicom, encoding, DCM_StudyDate) &&
          !GetUtf8TagValue(studyDate, dicom, encoding, DCM_SeriesDate) &&
          !GetUtf8TagValue(studyDate, dicom, encoding, DCM_AcquisitionDate) &&
          !GetUtf8TagValue(studyDate, dicom, encoding, DCM_ContentDate))
      {
        studyDate = nowDate;
      }
          
      std::string studyTime;
      if (!GetUtf8TagValue(studyTime, dicom, encoding, DCM_StudyTime) &&
          !GetUtf8TagValue(studyTime, dicom, encoding, DCM_SeriesTime) &&
          !GetUtf8TagValue(studyTime, dicom, encoding, DCM_AcquisitionTime) &&
          !GetUtf8TagValue(studyTime, dicom, encoding, DCM_ContentTime))
      {
        studyTime = nowTime;
      }

      /* copy attribute values from dataset to study record */
      SetTagValue(record, DCM_StudyDate, studyDate);
      SetTagValue(record, DCM_StudyTime, studyTime);
      CopyStringType2(record, dicom, encoding, DCM_StudyDescription);
      CopyStringType1(record, dicom, encoding, DCM_StudyInstanceUID);
      /* use type 1C instead of 1 in order to avoid unwanted overwriting */
      CopyStringType1C(record, dicom, encoding, DCM_StudyID);
      CopyStringType2(record, dicom, encoding, DCM_AccessionNumber);
    }

    void FillSeries(DcmDirectoryRecord& record,
                    DcmDataset& dicom,
                    Encoding encoding)
    {
      // cf. "DicomDirInterface::buildSeriesRecord()"

      /* copy attribute values from dataset to series record */
      CopyStringType1(record, dicom, encoding, DCM_Modality);
      CopyStringType1(record, dicom, encoding, DCM_SeriesInstanceUID);
      /* use type 1C instead of 1 in order to avoid unwanted overwriting */
      CopyStringType1C(record, dicom, encoding, DCM_SeriesNumber);

      // Add extended (non-standard) type 3 tags, those are not generated by DCMTK
      // http://dicom.nema.org/medical/Dicom/2016a/output/chtml/part02/sect_7.3.html
      // https://groups.google.com/d/msg/orthanc-users/Y7LOvZMDeoc/9cp3kDgxAwAJ
      if (extendedSopClass_)
      {
        CopyStringType3(record, dicom, encoding, DCM_SeriesDescription);
      }
    }

    void FillInstance(DcmDirectoryRecord& record,
                      DcmDataset& dicom,
                      Encoding encoding,
                      DcmMetaInfo& metaInfo,
                      const char* path)
    {
      // cf. "DicomDirInterface::buildImageRecord()"

      /* copy attribute values from dataset to image record */
      CopyStringType1(record, dicom, encoding, DCM_InstanceNumber);
      //CopyElementType1C(record, dicom, encoding, DCM_ImageType);

      // REMOVED since 0.9.7: copyElementType1C(dicom, DCM_ReferencedImageSequence, record);

      std::string sopClassUid, sopInstanceUid, transferSyntaxUid;
      if (!GetUtf8TagValue(sopClassUid, dicom, encoding, DCM_SOPClassUID) ||
          !GetUtf8TagValue(sopInstanceUid, dicom, encoding, DCM_SOPInstanceUID) ||
          !GetUtf8TagValue(transferSyntaxUid, metaInfo, encoding, DCM_TransferSyntaxUID))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      SetTagValue(record, DCM_ReferencedFileID, path);
      SetTagValue(record, DCM_ReferencedSOPClassUIDInFile, sopClassUid);
      SetTagValue(record, DCM_ReferencedSOPInstanceUIDInFile, sopInstanceUid);
      SetTagValue(record, DCM_ReferencedTransferSyntaxUIDInFile, transferSyntaxUid);
    }

    

    bool CreateResource(DcmDirectoryRecord*& target,
                        ResourceType level,
                        ParsedDicomFile& dicom,
                        const char* filename,
                        const char* path)
    {
      DcmDataset& dataset = *dicom.GetDcmtkObject().getDataset();
      Encoding encoding = dicom.GetEncoding();

      bool found;
      std::string id;
      E_DirRecType type;

      switch (level)
      {
        case ResourceType_Patient:
          found = GetUtf8TagValue(id, dataset, encoding, DCM_PatientID);
          type = ERT_Patient;
          break;

        case ResourceType_Study:
          found = GetUtf8TagValue(id, dataset, encoding, DCM_StudyInstanceUID);
          type = ERT_Study;
          break;

        case ResourceType_Series:
          found = GetUtf8TagValue(id, dataset, encoding, DCM_SeriesInstanceUID);
          type = ERT_Series;
          break;

        case ResourceType_Instance:
          found = GetUtf8TagValue(id, dataset, encoding, DCM_SOPInstanceUID);
          type = ERT_Image;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      if (!found)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      IndexKey key = std::make_pair(level, std::string(id.c_str()));
      Index::iterator it = index_.find(key);

      if (it != index_.end())
      {
        target = it->second;
        return false; // Already existing
      }

      std::auto_ptr<DcmDirectoryRecord> record(new DcmDirectoryRecord(type, NULL, filename));

      switch (level)
      {
        case ResourceType_Patient:
          FillPatient(*record, dataset, encoding);
          break;

        case ResourceType_Study:
          FillStudy(*record, dataset, encoding);
          break;

        case ResourceType_Series:
          FillSeries(*record, dataset, encoding);
          break;

        case ResourceType_Instance:
          FillInstance(*record, dataset, encoding, *dicom.GetDcmtkObject().getMetaInfo(), path);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      CopyStringType1C(*record, dataset, encoding, DCM_SpecificCharacterSet);

      target = record.get();
      GetRoot().insertSub(record.release());
      index_[key] = target;

      return true;   // Newly created
    }

    void Read(std::string& s)
    {
      if (!GetDicomDir().write(DICOMDIR_DEFAULT_TRANSFERSYNTAX, 
                               EET_UndefinedLength /*encodingType*/, 
                               EGL_withoutGL /*groupLength*/).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      file_.Read(s);
    }

    void SetFileSetId(const std::string& id)
    {
      dir_.reset(NULL);
      fileSetId_ = id;
    }
  };


  DicomDirWriter::DicomDirWriter() : pimpl_(new PImpl)
  {
  }

  DicomDirWriter::~DicomDirWriter()
  {
    if (pimpl_)
    {
      delete pimpl_;
    }
  }

  void DicomDirWriter::SetUtcUsed(bool utc)
  {
    pimpl_->SetUtcUsed(utc);
  }
  
  bool DicomDirWriter::IsUtcUsed() const
  {
    return pimpl_->IsUtcUsed();
  }

  void DicomDirWriter::SetFileSetId(const std::string& id)
  {
    pimpl_->SetFileSetId(id);
  }

  void DicomDirWriter::Add(const std::string& directory,
                           const std::string& filename,
                           ParsedDicomFile& dicom)
  {
    std::string path;
    if (directory.empty())
    {
      path = filename;
    }
    else
    {
      if (directory[directory.length() - 1] == '/' ||
          directory[directory.length() - 1] == '\\')
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      path = directory + '\\' + filename;
    }

    DcmDirectoryRecord* instance;
    bool isNewInstance = pimpl_->CreateResource(instance, ResourceType_Instance, dicom, filename.c_str(), path.c_str());
    if (isNewInstance)
    {
      DcmDirectoryRecord* series;
      bool isNewSeries = pimpl_->CreateResource(series, ResourceType_Series, dicom, filename.c_str(), NULL);
      series->insertSub(instance);

      if (isNewSeries)
      {
        DcmDirectoryRecord* study;
        bool isNewStudy = pimpl_->CreateResource(study, ResourceType_Study, dicom, filename.c_str(), NULL);
        study->insertSub(series);
  
        if (isNewStudy)
        {
          DcmDirectoryRecord* patient;
          pimpl_->CreateResource(patient, ResourceType_Patient, dicom, filename.c_str(), NULL);
          patient->insertSub(study);
        }
      }
    }
  }

  void DicomDirWriter::Encode(std::string& target)
  {
    pimpl_->Read(target);
  }


  void DicomDirWriter::EnableExtendedSopClass(bool enable)
  {
    pimpl_->EnableExtendedSopClass(enable);
  }

  
  bool DicomDirWriter::IsExtendedSopClass() const
  {
    return pimpl_->IsExtendedSopClass();
  }
}
