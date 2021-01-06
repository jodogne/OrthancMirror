/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#pragma once

#include "../DicomParsing/ParsedDicomFile.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomFindAnswers : public boost::noncopyable
  {
  private:
    Encoding                      encoding_;
    bool                          isWorklist_;
    std::vector<ParsedDicomFile*> answers_;
    bool                          complete_;

    void AddAnswerInternal(ParsedDicomFile* answer);

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
    void Add(ParsedDicomFile& dicom);
#endif

  public:
    explicit DicomFindAnswers(bool isWorklist);

    ~DicomFindAnswers();

    Encoding GetEncoding() const;

    void SetEncoding(Encoding encoding);

    void SetWorklist(bool isWorklist);

    bool IsWorklist() const;

    void Clear();

    void Reserve(size_t index);

    void Add(const DicomMap& map);

    void Add(const ParsedDicomFile& dicom);

    void Add(const void* dicom,
             size_t size);

    size_t GetSize() const;

    ParsedDicomFile& GetAnswer(size_t index) const;

    DcmDataset* ExtractDcmDataset(size_t index) const;

    void ToJson(Json::Value& target,
                bool simplify) const;

    void ToJson(Json::Value& target,
                size_t index,
                bool simplify) const;

    bool IsComplete() const;

    void SetComplete(bool isComplete);
  };
}
