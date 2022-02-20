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
#include "DicomFindAnswers.h"

#include "../DicomParsing/FromDcmtkBridge.h"
#include "../OrthancException.h"

#include <memory>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/noncopyable.hpp>


namespace Orthanc
{
  void DicomFindAnswers::AddAnswerInternal(ParsedDicomFile* answer)
  {
    std::unique_ptr<ParsedDicomFile> protection(answer);

    if (isWorklist_)
    {
      // These lines are necessary when serving worklists, otherwise
      // Orthanc does not behave as "wlmscpfs"
      protection->Remove(DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID);
      protection->Remove(DICOM_TAG_SOP_INSTANCE_UID);
    }

    protection->ChangeEncoding(encoding_);

    answers_.push_back(protection.release());
  }


  DicomFindAnswers::DicomFindAnswers(bool isWorklist) : 
    encoding_(GetDefaultDicomEncoding()),
    isWorklist_(isWorklist),
    complete_(true)
  {
  }

  DicomFindAnswers::~DicomFindAnswers()
  {
    Clear();
  }

  Encoding DicomFindAnswers::GetEncoding() const
  {
    return encoding_;
  }


  void DicomFindAnswers::SetEncoding(Encoding encoding)
  {
    for (size_t i = 0; i < answers_.size(); i++)
    {
      assert(answers_[i] != NULL);
      answers_[i]->ChangeEncoding(encoding);
    }

    encoding_ = encoding;
  }


  void DicomFindAnswers::SetWorklist(bool isWorklist)
  {
    if (answers_.empty())
    {
      isWorklist_ = isWorklist;
    }
    else
    {
      // This set of answers is not empty anymore, cannot change its type
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  bool DicomFindAnswers::IsWorklist() const
  {
    return isWorklist_;
  }


  void DicomFindAnswers::Clear()
  {
    for (size_t i = 0; i < answers_.size(); i++)
    {
      assert(answers_[i] != NULL);
      delete answers_[i];
    }

    answers_.clear();
  }


  void DicomFindAnswers::Reserve(size_t size)
  {
    if (size > answers_.size())
    {
      answers_.reserve(size);
    }
  }


  void DicomFindAnswers::Add(const DicomMap& map)
  {
    // We use the permissive mode to be tolerant wrt. invalid DICOM
    // files that contain some tags with out-of-range values (such
    // tags are removed from the answers)
    AddAnswerInternal(new ParsedDicomFile(map, encoding_, true /* permissive */));
                                          //"" /* no private creator */));
  }


  void DicomFindAnswers::Add(const ParsedDicomFile& dicom)
  {
    AddAnswerInternal(dicom.Clone(true));
  }

  void DicomFindAnswers::Add(const void* dicom,
                             size_t size)
  {
    AddAnswerInternal(new ParsedDicomFile(dicom, size));
  }

  size_t DicomFindAnswers::GetSize() const
  {
    return answers_.size();
  }


  ParsedDicomFile& DicomFindAnswers::GetAnswer(size_t index) const
  {
    if (index < answers_.size())
    {
      return *answers_[index];
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  DcmDataset* DicomFindAnswers::ExtractDcmDataset(size_t index) const
  {
    // As "DicomFindAnswers" stores its content using class
    // "ParsedDicomFile" (that internally uses "DcmFileFormat" from
    // DCMTK), the dataset can contain tags that are reserved if
    // storing the media on the disk, notably tag
    // "MediaStorageSOPClassUID" (0002,0002). In this function, we
    // remove all those tags whose group is below 0x0008. The
    // resulting data set is clean for emission in the C-FIND SCP.

    // http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.html#sect_C.4.1.1.3
    // https://groups.google.com/d/msg/orthanc-users/D3kpPuX8yV0/_zgHOzkMEQAJ

    DcmDataset& source = *GetAnswer(index).GetDcmtkObject().getDataset();

    std::unique_ptr<DcmDataset> target(new DcmDataset);

    for (unsigned long i = 0; i < source.card(); i++)
    {
      const DcmElement* element = source.getElement(i);
      assert(element != NULL);

      if (element != NULL &&
          element->getTag().getGroup() >= 0x0008 &&
          element->getTag().getElement() != 0x0000)
      {
        target->insert(dynamic_cast<DcmElement*>(element->clone()));
      }
    }
    
    return target.release();
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                size_t index,
                                DicomToJsonFormat format) const
  {
    const ParsedDicomFile& answer = GetAnswer(index);
    answer.DatasetToJson(target, format, DicomToJsonFlags_None, 0);
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                DicomToJsonFormat format) const
  {
    target = Json::arrayValue;

    for (size_t i = 0; i < GetSize(); i++)
    {
      Json::Value answer;
      ToJson(answer, i, format);
      target.append(answer);
    }
  }


  bool DicomFindAnswers::IsComplete() const
  {
    return complete_;
  }

  void DicomFindAnswers::SetComplete(bool isComplete)
  {
    complete_ = isComplete;
  }


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  void DicomFindAnswers::Add(ParsedDicomFile& dicom)
  {
    return Add(const_cast<const ParsedDicomFile&>(dicom));
  }

  void DicomFindAnswers::ToJson(Json::Value& target,
                                size_t index,
                                bool simplify) const
  {
    DicomToJsonFormat format = (simplify ? DicomToJsonFormat_Human : DicomToJsonFormat_Full);
    ToJson(target, index, format);
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                bool simplify) const
  {
    DicomToJsonFormat format = (simplify ? DicomToJsonFormat_Human : DicomToJsonFormat_Full);
    ToJson(target, format);
  }
#endif
}
