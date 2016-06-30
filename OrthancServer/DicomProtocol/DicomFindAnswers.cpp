/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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


#include "../PrecompiledHeadersServer.h"
#include "DicomFindAnswers.h"

#include "../FromDcmtkBridge.h"
#include "../ToDcmtkBridge.h"
#include "../../Core/OrthancException.h"

#include <memory>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <boost/noncopyable.hpp>


namespace Orthanc
{
  class DicomFindAnswers::Answer : public boost::noncopyable
  {
  private:
    ParsedDicomFile* dicom_;
    DicomMap*        map_;

    void CleanupDicom(bool isWorklist)
    {
      if (isWorklist &&
          dicom_ != NULL)
      {
        // These lines are necessary when serving worklists, otherwise
        // Orthanc does not behave as "wlmscpfs"
        dicom_->Remove(DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID);
        dicom_->Remove(DICOM_TAG_SOP_INSTANCE_UID);
      }
    }

  public:
    Answer(bool isWorklist,
           ParsedDicomFile& dicom) : 
      dicom_(dicom.Clone()),
      map_(NULL)
    {
      CleanupDicom(isWorklist);
    }

    Answer(bool isWorklist,
           const void* dicom,
           size_t size) : 
      dicom_(new ParsedDicomFile(dicom, size)),
      map_(NULL)
    {
      CleanupDicom(isWorklist);
    }

    Answer(const DicomMap& map) : 
      dicom_(NULL),
      map_(map.Clone())
    {
    }

    ~Answer()
    {
      if (dicom_ != NULL)
      {
        delete dicom_;
      }

      if (map_ != NULL)
      {
        delete map_;
      }
    }

    ParsedDicomFile& GetDicomFile()
    {
      if (dicom_ == NULL)
      {
        assert(map_ != NULL);
        dicom_ = new ParsedDicomFile(*map_);
      }

      return *dicom_;
    }

    DcmDataset* ExtractDcmDataset() const
    {
      if (dicom_ != NULL)
      {
        return new DcmDataset(*dicom_->GetDcmtkObject().getDataset());
      }
      else
      {
        assert(map_ != NULL);
        return ToDcmtkBridge::Convert(*map_);
      }
    }
  };


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
    answers_.push_back(new Answer(map));
  }


  void DicomFindAnswers::Add(ParsedDicomFile& dicom)
  {
    answers_.push_back(new Answer(isWorklist_, dicom));
  }


  void DicomFindAnswers::Add(const void* dicom,
                             size_t size)
  {
    answers_.push_back(new Answer(isWorklist_, dicom, size));
  }


  DicomFindAnswers::Answer& DicomFindAnswers::GetAnswerInternal(size_t index) const
  {
    if (index < answers_.size())
    {
      return *answers_.at(index);
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ParsedDicomFile& DicomFindAnswers::GetAnswer(size_t index) const
  {
    return GetAnswerInternal(index).GetDicomFile();
  }


  DcmDataset* DicomFindAnswers::ExtractDcmDataset(size_t index) const
  {
    return GetAnswerInternal(index).ExtractDcmDataset();
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                size_t index,
                                bool simplify) const
  {
    DicomToJsonFormat format = (simplify ? DicomToJsonFormat_Human : DicomToJsonFormat_Full);
    GetAnswer(index).ToJson(target, format, DicomToJsonFlags_None, 0);
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                bool simplify) const
  {
    target = Json::arrayValue;

    for (size_t i = 0; i < GetSize(); i++)
    {
      Json::Value answer;
      ToJson(answer, i, simplify);
      target.append(answer);
    }
  }
}
