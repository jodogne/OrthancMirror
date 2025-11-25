/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "Internals/SopInstanceUidFixer.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmjpeg/djrploss.h>  // for DJ_RPLossy
#include <dcmtk/dcmjpeg/djrplol.h>   // for DJ_RPLossless
#include <dcmtk/dcmjpls/djrparam.h>  // for DJLSRepresentationParameter

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  DcmtkTranscoder::DcmtkTranscoder(unsigned int maxConcurrentExecutions) :
  defaultLossyQuality_(90),
    maxConcurrentExecutionsSemaphore_(maxConcurrentExecutions)
  {
  }


  static bool GetBitsStored(uint16_t& bitsStored,
                            DcmDataset& dataset)
  {
    return dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good();
  }

  
  void DcmtkTranscoder::SetDefaultLossyQuality(unsigned int quality)
  {
    if (quality == 0 ||
        quality > 100)
    {
      throw OrthancException(
        ErrorCode_ParameterOutOfRange,
        "The default quality for lossy transcoding must be an integer between 1 and 100, received: " +
        boost::lexical_cast<std::string>(quality));
    }
    else
    {
      LOG(INFO) << "Default quality for lossy transcoding using DCMTK is set to: " << quality;
      defaultLossyQuality_ = quality;
    }
  }

  unsigned int DcmtkTranscoder::GetDefaultLossyQuality() const
  {
    return defaultLossyQuality_;
  }

  bool TryTranscode(std::vector<std::string>& failureReasons, /* out */
                    DicomTransferSyntax& selectedSyntax, /* out*/
                    DcmFileFormat& dicom, /* in/out */
                    const std::set<DicomTransferSyntax>& allowedSyntaxes,
                    DicomTransferSyntax trySyntax)
  {
    if (allowedSyntaxes.find(trySyntax) != allowedSyntaxes.end())
    {
      if (FromDcmtkBridge::Transcode(dicom, trySyntax, NULL))
      {
        selectedSyntax = trySyntax;
        return true;
      }

      failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(trySyntax));
    }
    return false;
  }


  bool DcmtkTranscoder::InplaceTranscode(DicomTransferSyntax& selectedSyntax /* out */,
                                         std::string& failureReason /* out */,
                                         DcmFileFormat& dicom, /* in/out */
                                         const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                         TranscodingSopInstanceUidMode mode,
                                         unsigned int lossyQuality) 
  {
    std::vector<std::string> failureReasons;

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
    
    if (TryTranscode(failureReasons, selectedSyntax, dicom, allowedSyntaxes, DicomTransferSyntax_LittleEndianImplicit))
    {
      return true;
    }

    if (TryTranscode(failureReasons, selectedSyntax, dicom, allowedSyntaxes, DicomTransferSyntax_LittleEndianExplicit))
    {
      return true;
    }

    if (TryTranscode(failureReasons, selectedSyntax, dicom, allowedSyntaxes, DicomTransferSyntax_BigEndianExplicit))
    {
      return true;
    }

    if (TryTranscode(failureReasons, selectedSyntax, dicom, allowedSyntaxes, DicomTransferSyntax_DeflatedLittleEndianExplicit))
    {
      return true;
    }

    // The SOP Instance UID fixer has only an effect if "mode" is TranscodingSopInstanceUidMode_Preserve
    // and if transcoding to a lossy transfer syntax
    const Internals::SopInstanceUidFixer fixer(mode, dicom);

    const bool allowNewSopInstanceUid = (mode == TranscodingSopInstanceUidMode_AllowNew ||
                                         mode == TranscodingSopInstanceUidMode_Preserve);

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess1) != allowedSyntaxes.end())
    {
      if (!allowNewSopInstanceUid)
      {
        failureReasons.push_back(std::string("Can not transcode to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess1) + " without generating new SOPInstanceUID");
      }
      else if (hasBitsStored && bitsStored != 8)
      {
        failureReasons.push_back(std::string("Can not transcode to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess1) + " if BitsStored != 8");
      }
      else
      {
        // Check out "dcmjpeg/apps/dcmcjpeg.cc"
        DJ_RPLossy parameters(lossyQuality);
          
        if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess1, &parameters))
        {
          fixer.Apply(dicom);
          selectedSyntax = DicomTransferSyntax_JPEGProcess1;
          return true;
        }
        failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess1));
      }
    }
#endif
      
#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess2_4) != allowedSyntaxes.end())
    {
      if (!allowNewSopInstanceUid)
      {
        failureReasons.push_back(std::string("Can not transcode to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess2_4) + " without generating new SOPInstanceUID");
      }
      else if (hasBitsStored && bitsStored > 12)
      {
        failureReasons.push_back(std::string("Can not transcode to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess2_4) + " if BitsStored != 8");
      }
      else
      {
        // Check out "dcmjpeg/apps/dcmcjpeg.cc"
        DJ_RPLossy parameters(lossyQuality);
        if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess2_4, &parameters))
        {
          fixer.Apply(dicom);
          selectedSyntax = DicomTransferSyntax_JPEGProcess2_4;
          return true;
        }
        failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess2_4));
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
        fixer.Apply(dicom);
        selectedSyntax = DicomTransferSyntax_JPEGProcess14;
        return true;
      }
      failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess14));
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
      failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGProcess14SV1));
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
      failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGLSLossless));
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
        fixer.Apply(dicom);
        selectedSyntax = DicomTransferSyntax_JPEGLSLossy;
        return true;
      }
      failureReasons.push_back(std::string("Internal error while transcoding to ") + GetTransferSyntaxUid(DicomTransferSyntax_JPEGLSLossy));
    }
#endif

    Orthanc::Toolbox::JoinStrings(failureReason, failureReasons, ", ");
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
                                  TranscodingSopInstanceUidMode mode)
  {
    return Transcode(target, source, allowedSyntaxes, mode, defaultLossyQuality_);
  }


  bool DcmtkTranscoder::Transcode(DicomImage& target,
                                  DicomImage& source /* in, "GetParsed()" possibly modified */,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  TranscodingSopInstanceUidMode mode,
                                  unsigned int lossyQuality)
  {
    Semaphore::Locker lock(maxConcurrentExecutionsSemaphore_); // limit the number of concurrent executions

    target.Clear();
    
    DicomTransferSyntax sourceSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, source.GetParsed()))
    {
      LOG(ERROR) << "Unsupport transfer syntax for transcoding";
      return false;
    }

    std::string failureReason;
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
    else if (InplaceTranscode(targetSyntax, failureReason, source.GetParsed(),
                              allowedSyntaxes, mode, lossyQuality))
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
        {
          // Only run the sanity check in debug mode
          const bool allowNewSopInstanceUid = (mode == TranscodingSopInstanceUidMode_AllowNew ||
                                               mode == TranscodingSopInstanceUidMode_Preserve);
          CheckTranscoding(target, sourceSyntax, sourceSopInstanceUid,
                           allowedSyntaxes, allowNewSopInstanceUid);
        }
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
      LOG(WARNING) << "DCMTK was unable to transcode from " << GetTransferSyntaxUid(sourceSyntax)
                   << " to one of: " << s << " " << failureReason;
      return false;
    }
  }
}
