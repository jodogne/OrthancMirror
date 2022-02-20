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
#include "DcmtkTranscoder.h"


#if !defined(ORTHANC_ENABLE_DCMTK_JPEG)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS must be defined
#endif


#include "FromDcmtkBridge.h"
#include "../Logging.h"
#include "../OrthancException.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmjpeg/djrploss.h>  // for DJ_RPLossy
#include <dcmtk/dcmjpeg/djrplol.h>   // for DJ_RPLossless
#include <dcmtk/dcmjpls/djrparam.h>  // for DJLSRepresentationParameter

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  DcmtkTranscoder::DcmtkTranscoder() :
    lossyQuality_(90)
  {
  }


  static bool GetBitsStored(uint16_t& bitsStored,
                            DcmDataset& dataset)
  {
    return dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good();
  }

  
  void DcmtkTranscoder::SetLossyQuality(unsigned int quality)
  {
    if (quality == 0 ||
        quality > 100)
    {
      throw OrthancException(
        ErrorCode_ParameterOutOfRange,
        "The quality for lossy transcoding must be an integer between 1 and 100, received: " +
        boost::lexical_cast<std::string>(quality));
    }
    else
    {
      LOG(INFO) << "Quality for lossy transcoding using DCMTK is set to: " << quality;
      lossyQuality_ = quality;
    }
  }

  unsigned int DcmtkTranscoder::GetLossyQuality() const
  {
    return lossyQuality_;
  }


  bool DcmtkTranscoder::InplaceTranscode(DicomTransferSyntax& selectedSyntax /* out */,
                                         DcmFileFormat& dicom,
                                         const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                         bool allowNewSopInstanceUid) 
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    DicomTransferSyntax syntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(syntax, dicom))
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot determine the transfer syntax");
    }

    uint16_t bitsStored;
    bool hasBitsStored = GetBitsStored(bitsStored, *dicom.getDataset());
    
    if (allowedSyntaxes.find(syntax) != allowedSyntaxes.end())
    {
      // No transcoding is needed
      return true;
    }
      
    if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianImplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianImplicit, NULL))
    {
      selectedSyntax = DicomTransferSyntax_LittleEndianImplicit;
      return true;
    }

    if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianExplicit, NULL))
    {
      selectedSyntax = DicomTransferSyntax_LittleEndianExplicit;
      return true;
    }
      
    if (allowedSyntaxes.find(DicomTransferSyntax_BigEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_BigEndianExplicit, NULL))
    {
      selectedSyntax = DicomTransferSyntax_BigEndianExplicit;
      return true;
    }

    if (allowedSyntaxes.find(DicomTransferSyntax_DeflatedLittleEndianExplicit) != allowedSyntaxes.end() &&
        FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_DeflatedLittleEndianExplicit, NULL))
    {
      selectedSyntax = DicomTransferSyntax_DeflatedLittleEndianExplicit;
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
        selectedSyntax = DicomTransferSyntax_JPEGProcess1;
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
        selectedSyntax = DicomTransferSyntax_JPEGProcess2_4;
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
        selectedSyntax = DicomTransferSyntax_JPEGProcess14;
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
        selectedSyntax = DicomTransferSyntax_JPEGProcess14SV1;
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
        selectedSyntax = DicomTransferSyntax_JPEGLSLossless;
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
        selectedSyntax = DicomTransferSyntax_JPEGLSLossy;
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


  bool DcmtkTranscoder::Transcode(DicomImage& target,
                                  DicomImage& source /* in, "GetParsed()" possibly modified */,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  bool allowNewSopInstanceUid)
  {
    target.Clear();
    
    DicomTransferSyntax sourceSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, source.GetParsed()))
    {
      LOG(ERROR) << "Unsupport transfer syntax for transcoding";
      return false;
    }

    {
      std::string s;
      for (std::set<DicomTransferSyntax>::const_iterator
             it = allowedSyntaxes.begin(); it != allowedSyntaxes.end(); ++it)
      {
        if (!s.empty())
        {
          s += ", ";
        }

        s += GetTransferSyntaxUid(*it);
      }

      if (s.empty())
      {
        s = "<none>";
      }
      
      LOG(INFO) << "DCMTK transcoding from " << GetTransferSyntaxUid(sourceSyntax)
                << " to one of: " << s;
    }

#if !defined(NDEBUG)
    const std::string sourceSopInstanceUid = GetSopInstanceUid(source.GetParsed());
#endif

    DicomTransferSyntax targetSyntax;
    if (allowedSyntaxes.find(sourceSyntax) != allowedSyntaxes.end())
    {
      // No transcoding is needed
      target.AcquireParsed(source);
      target.AcquireBuffer(source);
      return true;
    }
    else if (InplaceTranscode(targetSyntax, source.GetParsed(),
                              allowedSyntaxes, allowNewSopInstanceUid))
    {   
      // Sanity check
      DicomTransferSyntax targetSyntax2;
      if (FromDcmtkBridge::LookupOrthancTransferSyntax(targetSyntax2, source.GetParsed()) &&
          targetSyntax == targetSyntax2 &&
          allowedSyntaxes.find(targetSyntax2) != allowedSyntaxes.end())
      {
        target.AcquireParsed(source);
        source.Clear();
        
#if !defined(NDEBUG)
        // Only run the sanity check in debug mode
        CheckTranscoding(target, sourceSyntax, sourceSopInstanceUid,
                         allowedSyntaxes, allowNewSopInstanceUid);
#endif
        
        return true;
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }  
    }
    else
    {
      // Cannot transcode
      return false;
    }
  }
}
