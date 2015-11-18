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


#include "../PrecompiledHeadersServer.h"
#include "DicomFindAnswers.h"

#include "../FromDcmtkBridge.h"
#include "../ToDcmtkBridge.h"
#include "../../Core/OrthancException.h"

#include <memory>

namespace Orthanc
{
  void DicomFindAnswers::Clear()
  {
    for (size_t i = 0; i < items_.size(); i++)
    {
      assert(items_[i] != NULL);
      delete items_[i];
    }

    items_.clear();
  }

  void DicomFindAnswers::Reserve(size_t size)
  {
    if (size > items_.size())
    {
      items_.reserve(size);
    }
  }


  void DicomFindAnswers::Add(const DicomMap& map)
  {
    items_.push_back(new ParsedDicomFile(map));
  }

  void DicomFindAnswers::Add(ParsedDicomFile& dicom)
  {
    items_.push_back(dicom.Clone());
  }

  void DicomFindAnswers::Add(const char* dicom,
                             size_t size)
  {
    items_.push_back(new ParsedDicomFile(dicom, size));
  }


  ParsedDicomFile& DicomFindAnswers::GetAnswer(size_t index) const
  {
    if (index < items_.size())
    {
      return *items_.at(index);
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void DicomFindAnswers::ToJson(Json::Value& target,
                                size_t index,
                                bool simplify) const
  {
    DicomToJsonFormat format = (simplify ? DicomToJsonFormat_Simple : DicomToJsonFormat_Full);
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
