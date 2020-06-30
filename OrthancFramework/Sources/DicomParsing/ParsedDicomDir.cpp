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


#include "../PrecompiledHeaders.h"
#include "ParsedDicomDir.h"

#include "../Compatibility.h"
#include "../OrthancException.h"
#include "ParsedDicomFile.h"
#include "FromDcmtkBridge.h"

#include <dcmtk/dcmdata/dcdeftag.h>


namespace Orthanc
{
  void ParsedDicomDir::Clear()
  {
    for (size_t i = 0; i < content_.size(); i++)
    {
      assert(content_[i] != NULL);
      delete content_[i];
    }
  }

  
  bool ParsedDicomDir::LookupIndexOfOffset(size_t& target,
                                           unsigned int offset) const
  {
    if (offset == 0)
    {
      return false;
    }

    OffsetToIndex::const_iterator found = offsetToIndex_.find(offset);
    if (found == offsetToIndex_.end())
    {
      // Error in the algorithm that computes the offsets
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      target = found->second;
      return true;
    }
  }


  ParsedDicomDir::ParsedDicomDir(const std::string content)
  {
    ParsedDicomFile dicom(content);

    DcmSequenceOfItems* sequence = NULL;
    if (dicom.GetDcmtkObject().getDataset() == NULL ||
        !dicom.GetDcmtkObject().getDataset()->findAndGetSequence(DCM_DirectoryRecordSequence, sequence).good() ||
        sequence == NULL)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Not a DICOMDIR");
    }

    content_.resize(sequence->card());
    nextOffsets_.resize(content_.size());
    lowerOffsets_.resize(content_.size());

    // Manually reconstruct the list of all the available offsets of
    // "DcmItem", as "fStartPosition" is a protected member in DCMTK
    // API
    std::set<uint32_t> availableOffsets;
    availableOffsets.insert(0);


    for (unsigned long i = 0; i < sequence->card(); i++)
    {
      DcmItem* item = sequence->getItem(i);
      if (item == NULL)
      {
        Clear();
        throw OrthancException(ErrorCode_InternalError);
      }

      Uint32 next, lower;
      if (!item->findAndGetUint32(DCM_OffsetOfTheNextDirectoryRecord, next).good() ||
          !item->findAndGetUint32(DCM_OffsetOfReferencedLowerLevelDirectoryEntity, lower).good())
      {
        item->writeXML(std::cout);
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing offsets in DICOMDIR");
      }          

      nextOffsets_[i] = next;
      lowerOffsets_[i] = lower;

      std::unique_ptr<DicomMap> entry(new DicomMap);
      FromDcmtkBridge::ExtractDicomSummary(*entry, *item);

      if (next != 0)
      {
        availableOffsets.insert(next);
      }

      if (lower != 0)
      {
        availableOffsets.insert(lower);
      }

      content_[i] = entry.release();
    }

    if (content_.size() != availableOffsets.size())
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Inconsistent offsets in DICOMDIR");
    }

    unsigned int index = 0;
    for (std::set<uint32_t>::const_iterator it = availableOffsets.begin();
         it != availableOffsets.end(); ++it)
    {
      offsetToIndex_[*it] = index;
      index ++;
    }    
  }


  const DicomMap& ParsedDicomDir::GetItem(size_t i) const
  {
    if (i >= content_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(content_[i] != NULL);
      return *content_[i];
    }
  }


  bool ParsedDicomDir::LookupNext(size_t& target,
                                  size_t index) const
  {
    if (index >= nextOffsets_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return LookupIndexOfOffset(target, nextOffsets_[index]);
    }
  }


  bool ParsedDicomDir::LookupLower(size_t& target,
                                   size_t index) const
  {
    if (index >= lowerOffsets_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return LookupIndexOfOffset(target, lowerOffsets_[index]);
    }
  }
}
