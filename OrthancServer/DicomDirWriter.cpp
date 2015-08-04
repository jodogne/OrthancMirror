/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "DicomDirWriter.h"

#include "FromDcmtkBridge.h"
#include "ToDcmtkBridge.h"

#include "../Core/Logging.h"
#include "../Core/OrthancException.h"
#include "../Core/Uuid.h"

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
    std::string fileSetId_;
    Toolbox::TemporaryFile file_;
    std::auto_ptr<DcmDicomDir> dir_;

    typedef std::pair<ResourceType, std::string>  IndexKey;
    typedef std::map<IndexKey, DcmDirectoryRecord* >  Index;
    Index  index_;


    /*******************************************************************************
     * Functions adapted from "dcmdata/libsrc/dcddirif.cc" from DCMTK 3.6.0
     *******************************************************************************/

    // print an error message to the console (stderr) that something went wrong with an attribute
    static void printAttributeErrorMessage(const DcmTagKey &key,
                                           const OFCondition &error,
                                           const char *operation)
    {
      if (error.bad())
      {
        OFString str;
        if (operation != NULL)
        {
          str = "cannot ";
          str += operation;
          str += " ";
        }
        LOG(ERROR) << error.text() << ": " << str << DcmTag(key).getTagName() << " " << key;
      }
    }

    // copy element from dataset to directory record
    static void copyElement(DcmItem& dataset,
                            const DcmTagKey &key,
                            DcmDirectoryRecord& record,
                            const OFBool optional,
                            const OFBool copyEmpty)
    {
      /* check whether tag exists in source dataset (if optional) */
      if (!optional || (copyEmpty && dataset.tagExists(key)) || dataset.tagExistsWithValue(key))
      {
        DcmElement *delem = NULL;
        /* get copy of element from source dataset */
        OFCondition status = dataset.findAndGetElement(key, delem, OFFalse /*searchIntoSub*/, OFTrue /*createCopy*/);
        if (status.good())
        {
          /* ... and insert it into the destination dataset (record) */
          status = record.insert(delem, OFTrue /*replaceOld*/);
          if (status.good())
          {
            DcmTag tag(key);
            /* check for correct VR in the dataset */
            if (delem->getVR() != tag.getEVR())
            {
              /* create warning message */
              LOG(WARNING) << "DICOMDIR: possibly wrong VR: "
                           << tag.getTagName() << " " << key << " with "
                           << DcmVR(delem->getVR()).getVRName() << " found, expected "
                           << tag.getVRName() << " instead";
            }
          } else
            delete delem;
        } else if (status == EC_TagNotFound)
          status = record.insertEmptyElement(key);
        printAttributeErrorMessage(key, status, "insert");
      }
    }

    // copy optional string value from dataset to directory record
    static void copyStringWithDefault(DcmItem& dataset,
                                      const DcmTagKey &key,
                                      DcmDirectoryRecord& record,
                                      const char *defaultValue,
                                      const OFBool printWarning)
    {
        OFCondition status;
        if (dataset.tagExistsWithValue(key))
        {
          OFString stringValue;
          /* retrieve string value from source dataset and put it into the destination dataset */
          status = dataset.findAndGetOFStringArray(key, stringValue);
          if (status.good())
            status = record.putAndInsertString(key, stringValue.c_str());
        } else {
          if (printWarning && (defaultValue != NULL))
          {
            /* create warning message */
            LOG(WARNING) << "DICOMDIR: " << DcmTag(key).getTagName() << " "
                         << key << " missing, using alternative: " << defaultValue;
          }
          /* put default value */
          status = record.putAndInsertString(key, defaultValue);
        }
    }

    // create alternative study date if absent in dataset
    static OFString &alternativeStudyDate(DcmItem& dataset,
                                          OFString &result)
    {
      /* use another date if present */
      if (dataset.findAndGetOFStringArray(DCM_SeriesDate, result).bad() || result.empty())
      {
        if (dataset.findAndGetOFStringArray(DCM_AcquisitionDate, result).bad() || result.empty())
        {
          if (dataset.findAndGetOFStringArray(DCM_ContentDate, result).bad() || result.empty())
          {
            /* use current date, "19000101" in case of error */
            DcmDate::getCurrentDate(result);
          }
        }
      }
      return result;
    }


    // create alternative study time if absent in dataset
    static OFString &alternativeStudyTime(DcmItem& dataset,
                                          OFString &result)
    {
      /* use another time if present */
      if (dataset.findAndGetOFStringArray(DCM_SeriesTime, result).bad() || result.empty())
      {
        if (dataset.findAndGetOFStringArray(DCM_AcquisitionTime, result).bad() || result.empty())
        {
          if (dataset.findAndGetOFStringArray(DCM_ContentTime, result).bad() || result.empty())
          {
            /* use current time, "0000" in case of error */
            DcmTime::getCurrentTime(result);
          }
        }
      }
      return result;
    }


    static void copyElementType1(DcmItem& dataset,
                                 const DcmTagKey &key,
                                 DcmDirectoryRecord& record)
    {
      copyElement(dataset, key, record, OFFalse /*optional*/, OFFalse /*copyEmpty*/);
    }

    static void copyElementType1C(DcmItem& dataset,
                                  const DcmTagKey &key,
                                  DcmDirectoryRecord& record)
    {
      copyElement(dataset, key, record, OFTrue /*optional*/, OFFalse /*copyEmpty*/);
    }

    static void copyElementType2(DcmItem& dataset,
                                 const DcmTagKey &key,
                                 DcmDirectoryRecord& record)
    {
      copyElement(dataset, key, record, OFFalse /*optional*/, OFTrue /*copyEmpty*/);
    }

    /*******************************************************************************
     * End of functions adapted from "dcmdata/libsrc/dcddirif.cc" from DCMTK 3.6.0
     *******************************************************************************/


    DcmDicomDir& GetDicomDir()
    {
      if (dir_.get() == NULL)
      {
        dir_.reset(new DcmDicomDir(file_.GetPath().c_str(), 
                                   fileSetId_.c_str()));
      }

      return *dir_;
    }


    DcmDirectoryRecord& GetRoot()
    {
      return GetDicomDir().getRootRecord();
    }


  public:
    PImpl() : fileSetId_("ORTHANC_MEDIA")
    {
    }

    void FillPatient(DcmDirectoryRecord& record,
                     DcmItem& dicom)
    {
      // cf. "DicomDirInterface::buildPatientRecord()"

      copyElementType1C(dicom, DCM_PatientID, record);
      copyElementType2(dicom, DCM_PatientName, record);
    }

    void FillStudy(DcmDirectoryRecord& record,
                   DcmItem& dicom)
    {
      // cf. "DicomDirInterface::buildStudyRecord()"

      OFString tmpString;
      /* copy attribute values from dataset to study record */
      copyStringWithDefault(dicom, DCM_StudyDate, record, 
                            alternativeStudyDate(dicom, tmpString).c_str(), OFTrue /*printWarning*/);
      copyStringWithDefault(dicom, DCM_StudyTime, record, 
                            alternativeStudyTime(dicom, tmpString).c_str(), OFTrue /*printWarning*/);
      copyElementType2(dicom, DCM_StudyDescription, record);
      copyElementType1(dicom, DCM_StudyInstanceUID, record);
      /* use type 1C instead of 1 in order to avoid unwanted overwriting */
      copyElementType1C(dicom, DCM_StudyID, record);
      copyElementType2(dicom, DCM_AccessionNumber, record);
    }

    void FillSeries(DcmDirectoryRecord& record,
                    DcmItem& dicom)
    {
      // cf. "DicomDirInterface::buildSeriesRecord()"

      /* copy attribute values from dataset to series record */
      copyElementType1(dicom, DCM_Modality, record);
      copyElementType1(dicom, DCM_SeriesInstanceUID, record);
      /* use type 1C instead of 1 in order to avoid unwanted overwriting */
      copyElementType1C(dicom, DCM_SeriesNumber, record);
    }

    void FillInstance(DcmDirectoryRecord& record,
                      DcmItem& dicom,
                      DcmMetaInfo& metaInfo,
                      const char* path)
    {
      // cf. "DicomDirInterface::buildImageRecord()"

      /* copy attribute values from dataset to image record */
      copyElementType1(dicom, DCM_InstanceNumber, record);
      //copyElementType1C(dicom, DCM_ImageType, record);
      copyElementType1C(dicom, DCM_ReferencedImageSequence, record);

      OFString tmp;

      DcmElement* item = record.remove(DCM_ReferencedImageSequence);
      if (item != NULL)
      {
        delete item;
      }

      if (record.putAndInsertString(DCM_ReferencedFileID, path).bad() ||
          dicom.findAndGetOFStringArray(DCM_SOPClassUID, tmp).bad() ||
          record.putAndInsertString(DCM_ReferencedSOPClassUIDInFile, tmp.c_str()).bad() ||
          dicom.findAndGetOFStringArray(DCM_SOPInstanceUID, tmp).bad() ||
          record.putAndInsertString(DCM_ReferencedSOPInstanceUIDInFile, tmp.c_str()).bad() ||
          metaInfo.findAndGetOFStringArray(DCM_TransferSyntaxUID, tmp).bad() ||
          record.putAndInsertString(DCM_ReferencedTransferSyntaxUIDInFile, tmp.c_str()).bad())
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    

    bool CreateResource(DcmDirectoryRecord*& target,
                        ResourceType level,
                        DcmFileFormat& dicom,
                        const char* filename,
                        const char* path)
    {
      DcmDataset& dataset = *dicom.getDataset();

      OFCondition result;
      OFString id;
      E_DirRecType type;

      switch (level)
      {
        case ResourceType_Patient:
          result = dataset.findAndGetOFString(DCM_PatientID, id);
          type = ERT_Patient;
          break;

        case ResourceType_Study:
          result = dataset.findAndGetOFString(DCM_StudyInstanceUID, id);
          type = ERT_Study;
          break;

        case ResourceType_Series:
          result = dataset.findAndGetOFString(DCM_SeriesInstanceUID, id);
          type = ERT_Series;
          break;

        case ResourceType_Instance:
          result = dataset.findAndGetOFString(DCM_SOPInstanceUID, id);
          type = ERT_Image;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      if (!result.good())
      {
        throw OrthancException(ErrorCode_InternalError);
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
          FillPatient(*record, dataset);
          break;

        case ResourceType_Study:
          FillStudy(*record, dataset);
          break;

        case ResourceType_Series:
          FillSeries(*record, dataset);
          break;

        case ResourceType_Instance:
          FillInstance(*record, dataset, *dicom.getMetaInfo(), path);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      if (record->isAffectedBySpecificCharacterSet())
      {
        copyElementType1C(dataset, DCM_SpecificCharacterSet, *record);
      }

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

    DcmFileFormat& fileFormat = *reinterpret_cast<DcmFileFormat*>(dicom.GetDcmtkObject());

    DcmDirectoryRecord* instance;
    bool isNewInstance = pimpl_->CreateResource(instance, ResourceType_Instance, fileFormat, filename.c_str(), path.c_str());
    if (isNewInstance)
    {
      DcmDirectoryRecord* series;
      bool isNewSeries = pimpl_->CreateResource(series, ResourceType_Series, fileFormat, filename.c_str(), NULL);
      series->insertSub(instance);

      if (isNewSeries)
      {
        DcmDirectoryRecord* study;
        bool isNewStudy = pimpl_->CreateResource(study, ResourceType_Study, fileFormat, filename.c_str(), NULL);
        study->insertSub(series);
  
        if (isNewStudy)
        {
          DcmDirectoryRecord* patient;
          pimpl_->CreateResource(patient, ResourceType_Patient, fileFormat, filename.c_str(), NULL);
          patient->insertSub(study);
        }
      }
    }
  }

  void DicomDirWriter::Encode(std::string& target)
  {
    pimpl_->Read(target);
  }
}
