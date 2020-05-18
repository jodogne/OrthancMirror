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
#include "DcmtkTranscoder.h"


#if !defined(ORTHANC_ENABLE_DCMTK_JPEG)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS must be defined
#endif


#include "FromDcmtkBridge.h"
#include "../OrthancException.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmjpeg/djrploss.h>  // for DJ_RPLossy
#include <dcmtk/dcmjpeg/djrplol.h>   // for DJ_RPLossless
#include <dcmtk/dcmjpls/djrparam.h>  // for DJLSRepresentationParameter


namespace Orthanc
{
  static bool GetBitsStored(uint16_t& bitsStored,
                            DcmDataset& dataset)
  {
    return dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good();
  }

  
  static std::string GetSopInstanceUid(DcmDataset& dataset)
  {
    const char* v = NULL;

    if (dataset.findAndGetString(DCM_SOPInstanceUID, v).good() &&
        v != NULL)
    {
      return std::string(v);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "File without SOP instance UID");
    }
  }

  
  static void CheckSopInstanceUid(DcmFileFormat& dicom,
                                  const std::string& sopInstanceUid,
                                  bool mustEqual)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    bool ok;
      
    if (dicom.getDataset()->tagExists(DCM_PixelData))
    {
      if (mustEqual)
      {
        ok = (GetSopInstanceUid(*dicom.getDataset()) == sopInstanceUid);
      }
      else
      {
        ok = (GetSopInstanceUid(*dicom.getDataset()) != sopInstanceUid);
      }
    }
    else
    {
      // No pixel data: Transcoding must not change the SOP instance UID
      ok = (GetSopInstanceUid(*dicom.getDataset()) == sopInstanceUid);
    }
    
    if (!ok)
    {
      throw OrthancException(ErrorCode_InternalError,
                             mustEqual ? "The SOP instance UID has changed unexpectedly during transcoding" :
                             "The SOP instance UID has not changed as expected during transcoding");
    }
  }
    

  void DcmtkTranscoder::SetLossyQuality(unsigned int quality)
  {
    if (quality <= 0 ||
        quality > 100)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      lossyQuality_ = quality;
    }
  }

    
  bool DcmtkTranscoder::InplaceTranscode(bool& hasSopInstanceUidChanged /* out */,
                                         DcmFileFormat& dicom,
                                         const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                         bool allowNewSopInstanceUid) 
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    hasSopInstanceUidChanged = false;

    DicomTransferSyntax syntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(syntax, dicom))
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot determine the transfer syntax");
    }

    uint16_t bitsStored;
    bool hasBitsStored = GetBitsStored(bitsStored, *dicom.getDataset());
    
    std::string sourceSopInstanceUid = GetSopInstanceUid(*dicom.getDataset());
    
    if (allowedSyntaxes.find(syntax) != allowedSyntaxes.end())
    {
      // No transcoding is needed
      return true;
    }
      
    if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianImplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianImplicit, NULL))
    {
      CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
      return true;
    }

    if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianExplicit, NULL))
    {
      CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
      return true;
    }
      
    if (allowedSyntaxes.find(DicomTransferSyntax_BigEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_BigEndianExplicit, NULL))
    {
      CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
      return true;
    }

    if (allowedSyntaxes.find(DicomTransferSyntax_DeflatedLittleEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_DeflatedLittleEndianExplicit, NULL))
    {
      CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
      return true;
    }

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess1) != allowedSyntaxes.end() &&
        allowNewSopInstanceUid &&
        (!hasBitsStored || bitsStored == 8))
    {
      // Check out "dcmjpeg/apps/dcmcjpeg.cc"
      DJ_RPLossy parameters(lossyQuality_);
        
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess1, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, false);
        hasSopInstanceUidChanged = true;
        return true;
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess2_4) != allowedSyntaxes.end() &&
        allowNewSopInstanceUid &&
        (!hasBitsStored || bitsStored <= 12))
    {
      // Check out "dcmjpeg/apps/dcmcjpeg.cc"
      DJ_RPLossy parameters(lossyQuality_);
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess2_4, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, false);
        hasSopInstanceUidChanged = true;
        return true;
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess14) != allowedSyntaxes.end())
    {
      // Check out "dcmjpeg/apps/dcmcjpeg.cc"
      DJ_RPLossless parameters(6 /* opt_selection_value */,
                               0 /* opt_point_transform */);
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess14, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
        return true;
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess14SV1) != allowedSyntaxes.end())
    {
      // Check out "dcmjpeg/apps/dcmcjpeg.cc"
      DJ_RPLossless parameters(6 /* opt_selection_value */,
                               0 /* opt_point_transform */);
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess14SV1, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
        return true;
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGLSLossless) != allowedSyntaxes.end())
    {
      // Check out "dcmjpls/apps/dcmcjpls.cc"
      DJLSRepresentationParameter parameters(2 /* opt_nearlossless_deviation */,
                                             OFTrue /* opt_useLosslessProcess */);

      /**
       * WARNING: This call results in a segmentation fault if using
       * the DCMTK package 3.6.2 from Ubuntu 18.04.
       **/              
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGLSLossless, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, true);
        return true;
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    if (allowNewSopInstanceUid &&
        allowedSyntaxes.find(DicomTransferSyntax_JPEGLSLossy) != allowedSyntaxes.end())
    {
      // Check out "dcmjpls/apps/dcmcjpls.cc"
      DJLSRepresentationParameter parameters(2 /* opt_nearlossless_deviation */,
                                             OFFalse /* opt_useLosslessProcess */);

      /**
       * WARNING: This call results in a segmentation fault if using
       * the DCMTK package 3.6.2 from Ubuntu 18.04.
       **/              
      if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGLSLossy, &parameters))
      {
        CheckSopInstanceUid(dicom, sourceSopInstanceUid, false);
        hasSopInstanceUidChanged = true;
        return true;
      }
    }
#endif

    return false;
  }

    
  bool DcmtkTranscoder::IsSupported(DicomTransferSyntax syntax)
  {
    if (syntax == DicomTransferSyntax_LittleEndianImplicit ||
        syntax == DicomTransferSyntax_LittleEndianExplicit ||
        syntax == DicomTransferSyntax_BigEndianExplicit ||
        syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit)
    {
      return true;
    }

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (syntax == DicomTransferSyntax_JPEGProcess1 ||
        syntax == DicomTransferSyntax_JPEGProcess2_4 ||
        syntax == DicomTransferSyntax_JPEGProcess14 ||
        syntax == DicomTransferSyntax_JPEGProcess14SV1)
    {
      return true;
    }
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    if (syntax == DicomTransferSyntax_JPEGLSLossless ||
        syntax == DicomTransferSyntax_JPEGLSLossy)
    {
      return true;
    }
#endif
    
    return false;
  }



  bool DcmtkTranscoder::TranscodeParsedToBuffer(
    std::string& target /* out */,
    bool& hasSopInstanceUidChanged /* out */,
    DcmFileFormat& dicom /* in, possibly modified */,
    DicomTransferSyntax targetSyntax,
    bool allowNewSopInstanceUid)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::set<DicomTransferSyntax> tmp;
    tmp.insert(targetSyntax);

    if (InplaceTranscode(hasSopInstanceUidChanged, dicom, tmp, allowNewSopInstanceUid))
    {
      DicomTransferSyntax targetSyntax2;
      if (FromDcmtkBridge::LookupOrthancTransferSyntax(targetSyntax2, dicom) &&
          targetSyntax == targetSyntax2 &&
          dicom.getDataset() != NULL)
      {
        FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom.getDataset());
        return true;
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }      
    }
    else
    {
      return false;
    }    
  }


  IDicomTranscoder::TranscodedDicom* DcmtkTranscoder::TranscodeToParsed(
    DcmFileFormat& dicom /* in, possibly modified */,
    const void* buffer /* in, same DICOM file as "dicom" */,
    size_t size,
    const std::set<DicomTransferSyntax>& allowedSyntaxes,
    bool allowNewSopInstanceUid)
  {
    DicomTransferSyntax sourceSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, dicom))
    {
      LOG(ERROR) << "Unsupport transfer syntax for transcoding";
      return NULL;
    }

    bool hasSopInstanceUidChanged;
    
    if (allowedSyntaxes.find(sourceSyntax) != allowedSyntaxes.end())
    {
      // No transcoding is needed
      return TranscodedDicom::CreateFromExternal(dicom, false /* no change in UID */);
    }
    else if (InplaceTranscode(hasSopInstanceUidChanged, dicom,
                              allowedSyntaxes, allowNewSopInstanceUid))
    {
      return TranscodedDicom::CreateFromExternal(dicom, hasSopInstanceUidChanged);
    }
    else
    {
      // Cannot transcode
      return NULL;
    }
  }
}
