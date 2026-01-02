/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../PrecompiledHeaders.h"
#include "SopInstanceUidFixer.h"

#include "../../OrthancException.h"

#include <dcmtk/dcmdata/dcdeftag.h>


namespace Orthanc
{
  namespace Internals
  {
    SopInstanceUidFixer::SopInstanceUidFixer(TranscodingSopInstanceUidMode mode,
                                             IDicomTranscoder::DicomImage& source) :
      fix_(mode == TranscodingSopInstanceUidMode_Preserve)
    {
      if (fix_)
      {
        sopInstanceUid_ = IDicomTranscoder::GetSopInstanceUid(source.GetParsed());
      }
    }


    SopInstanceUidFixer::SopInstanceUidFixer(TranscodingSopInstanceUidMode mode,
                                             DcmFileFormat& source) :
      fix_(mode == TranscodingSopInstanceUidMode_Preserve)
    {
      if (fix_)
      {
        sopInstanceUid_ = IDicomTranscoder::GetSopInstanceUid(source);
      }
    }


    void SopInstanceUidFixer::Apply(DcmFileFormat& target) const
    {
      if (fix_)
      {
        if (target.getDataset() == NULL ||
            !target.getDataset()->putAndInsertString(
              DCM_SOPInstanceUID, sopInstanceUid_.c_str(), OFTrue /* replace */).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
    }


    void SopInstanceUidFixer::Apply(IDicomTranscoder::DicomImage& target) const
    {
      if (fix_)
      {
        std::unique_ptr<DcmFileFormat> dicom(target.ReleaseParsed());
        Apply(*dicom);

        target.Clear();
        target.AcquireParsed(dicom.release());
      }
    }
  }
}
