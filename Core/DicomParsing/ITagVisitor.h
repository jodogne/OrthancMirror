/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

#include "../DicomFormat/DicomTag.h"

#include <vector>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ITagVisitor : public boost::noncopyable
  {
  public:
    enum Action
    {
      Action_Replace,
      Action_None
    };

    virtual ~ITagVisitor()
    {
    }

    // Visiting a DICOM element that is internal to DCMTK
    virtual void VisitNotSupported(const std::vector<DicomTag>& parentTags,
                                   const std::vector<size_t>& parentIndexes,
                                   const DicomTag& tag,
                                   ValueRepresentation vr) = 0;

    // SQ
    virtual void VisitEmptySequence(const std::vector<DicomTag>& parentTags,
                                    const std::vector<size_t>& parentIndexes,
                                    const DicomTag& tag) = 0;

    // SL, SS, UL, US
    virtual void VisitIntegers(const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::vector<int64_t>& values) = 0;

    // FL, FD, OD, OF
    virtual void VisitDoubles(const std::vector<DicomTag>& parentTags,
                              const std::vector<size_t>& parentIndexes,
                              const DicomTag& tag,
                              ValueRepresentation vr,
                              const std::vector<double>& values) = 0;

    // AT
    virtual void VisitAttributes(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 const std::vector<DicomTag>& values) = 0;

    // OB, OL, OW, UN
    virtual void VisitBinary(const std::vector<DicomTag>& parentTags,
                             const std::vector<size_t>& parentIndexes,
                             const DicomTag& tag,
                             ValueRepresentation vr,
                             const void* data,
                             size_t size) = 0;

    // Visiting an UTF-8 string
    virtual Action VisitString(std::string& newValue,
                               const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::string& value) = 0;
  };
}
