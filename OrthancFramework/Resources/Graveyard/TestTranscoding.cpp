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


  bool FromDcmtkBridge::SaveToMemoryBuffer(std::string& buffer,
                                           DcmFileFormat& dicom,
                                           DicomTransferSyntax syntax)
  {
    E_TransferSyntax xfer;
    if (!LookupDcmtkTransferSyntax(xfer, syntax))
    {
      return false;
    }
    else if (!dicom.validateMetaInfo(xfer).good())
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot setup the transfer syntax to write a DICOM instance");
    }
    else
    {
      return SaveToMemoryBufferInternal(buffer, dicom, xfer);
    }
  }


  bool FromDcmtkBridge::SaveToMemoryBuffer(std::string& buffer,
                                           DcmFileFormat& dicom)
  {
    E_TransferSyntax xfer = dicom.getDataset()->getCurrentXfer();
    if (xfer == EXS_Unknown)
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot write a DICOM instance with unknown transfer syntax");
    }
    else if (!dicom.validateMetaInfo(xfer).good())
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot setup the transfer syntax to write a DICOM instance");
    }
    else
    {
      return SaveToMemoryBufferInternal(buffer, dicom, xfer);
    }
  }





#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpxitem.h>

#include "../Core/DicomParsing/Internals/DicomFrameIndex.h"

namespace Orthanc
{
  class IParsedDicomImage : public boost::noncopyable
  {
  public:
    virtual ~IParsedDicomImage()
    {
    }

    virtual DicomTransferSyntax GetTransferSyntax() = 0;

    virtual std::string GetSopClassUid() = 0;

    virtual std::string GetSopInstanceUid() = 0;

    virtual unsigned int GetFramesCount() = 0;

    // Can return NULL, for compressed transfer syntaxes
    virtual ImageAccessor* GetUncompressedFrame(unsigned int frame) = 0;
    
    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) = 0;
    
    virtual void WriteToMemoryBuffer(std::string& target) = 0;
  };


  class IDicomImageReader : public boost::noncopyable
  {
  public:
    virtual ~IDicomImageReader()
    {
    }

    virtual IParsedDicomImage* Read(const void* data,
                                    size_t size) = 0;

    virtual IParsedDicomImage* Transcode(const void* data,
                                         size_t size,
                                         DicomTransferSyntax syntax,
                                         bool allowNewSopInstanceUid) = 0;
  };


  class DcmtkImageReader : public IDicomImageReader
  {
  private:
    class Image : public IParsedDicomImage
    {
    private:
      std::unique_ptr<DcmFileFormat>    dicom_;
      std::unique_ptr<DicomFrameIndex>  index_;
      DicomTransferSyntax               transferSyntax_;
      std::string                       sopClassUid_;
      std::string                       sopInstanceUid_;

      static std::string GetStringTag(DcmDataset& dataset,
                                      const DcmTagKey& tag)
      {
        const char* value = NULL;

        if (!dataset.findAndGetString(tag, value).good() ||
            value == NULL)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Missing SOP class/instance UID in DICOM instance");
        }
        else
        {
          return std::string(value);
        }
      }

    public:
      Image(DcmFileFormat* dicom,
            DicomTransferSyntax syntax) :
        dicom_(dicom),
        transferSyntax_(syntax)
      {
        if (dicom == NULL ||
            dicom_->getDataset() == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        DcmDataset& dataset = *dicom_->getDataset();
        index_.reset(new DicomFrameIndex(dataset));

        sopClassUid_ = GetStringTag(dataset, DCM_SOPClassUID);
        sopInstanceUid_ = GetStringTag(dataset, DCM_SOPInstanceUID);
      }

      virtual DicomTransferSyntax GetTransferSyntax() ORTHANC_OVERRIDE
      {
        return transferSyntax_;
      }

      virtual std::string GetSopClassUid() ORTHANC_OVERRIDE
      {
        return sopClassUid_;
      }
    
      virtual std::string GetSopInstanceUid() ORTHANC_OVERRIDE
      {
        return sopInstanceUid_;
      }

      virtual unsigned int GetFramesCount() ORTHANC_OVERRIDE
      {
        return index_->GetFramesCount();
      }

      virtual void WriteToMemoryBuffer(std::string& target) ORTHANC_OVERRIDE
      {
        assert(dicom_.get() != NULL);
        if (!FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, transferSyntax_))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot write the DICOM instance to a memory buffer");
        }
      }

      virtual ImageAccessor* GetUncompressedFrame(unsigned int frame) ORTHANC_OVERRIDE
      {
        assert(dicom_.get() != NULL &&
               dicom_->getDataset() != NULL);
        return DicomImageDecoder::Decode(*dicom_->getDataset(), frame);
      }

      virtual void GetCompressedFrame(std::string& target,
                                      unsigned int frame) ORTHANC_OVERRIDE
      {
        assert(index_.get() != NULL);
        index_->GetRawFrame(target, frame);
      }
    };

    unsigned int lossyQuality_;

    static DicomTransferSyntax DetectTransferSyntax(DcmFileFormat& dicom)
    {
      if (dicom.getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
        
      DcmDataset& dataset = *dicom.getDataset();

      E_TransferSyntax xfer = dataset.getCurrentXfer();
      if (xfer == EXS_Unknown)
      {
        dataset.updateOriginalXfer();
        xfer = dataset.getCurrentXfer();
        if (xfer == EXS_Unknown)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Cannot determine the transfer syntax of the DICOM instance");
        }
      }

      DicomTransferSyntax syntax;
      if (FromDcmtkBridge::LookupOrthancTransferSyntax(syntax, xfer))
      {
        return syntax;
      }
      else
      {
        throw OrthancException(
          ErrorCode_BadFileFormat,
          "Unsupported transfer syntax: " + boost::lexical_cast<std::string>(xfer));
      }
    }


    static uint16_t GetBitsStored(DcmFileFormat& dicom)
    {
      if (dicom.getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      uint16_t bitsStored;
      if (dicom.getDataset()->findAndGetUint16(DCM_BitsStored, bitsStored).good())
      {
        return bitsStored;
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      
    }
    
      
  public:
    DcmtkImageReader() :
      lossyQuality_(90)
    {
    }

    void SetLossyQuality(unsigned int quality)
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

    unsigned int GetLossyQuality() const
    {
      return lossyQuality_;
    }

    virtual IParsedDicomImage* Read(const void* data,
                                    size_t size)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(data, size));
      if (dicom.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      DicomTransferSyntax transferSyntax = DetectTransferSyntax(*dicom);

      return new Image(dicom.release(), transferSyntax);
    }

    virtual IParsedDicomImage* Transcode(const void* data,
                                         size_t size,
                                         DicomTransferSyntax syntax,
                                         bool allowNewSopInstanceUid)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(data, size));
      if (dicom.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      const uint16_t bitsStored = GetBitsStored(*dicom);

      if (syntax == DetectTransferSyntax(*dicom))
      {
        // No transcoding is needed
        return new Image(dicom.release(), syntax);
      }
      
      if (syntax == DicomTransferSyntax_LittleEndianImplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_LittleEndianImplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

      if (syntax == DicomTransferSyntax_LittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_LittleEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }
      
      if (syntax == DicomTransferSyntax_BigEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_BigEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

      if (syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_DeflatedLittleEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess1 &&
          allowNewSopInstanceUid &&
          bitsStored == 8)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        
        if (FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_JPEGProcess1, &rpLossy))
        {
          return new Image(dicom.release(), syntax);
        }
      }
#endif
      
#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess2_4 &&
          allowNewSopInstanceUid &&
          bitsStored <= 12)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        if (FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_JPEGProcess2_4, &rpLossy))
        {
          return new Image(dicom.release(), syntax);
        }
      }
#endif

      //LOG(INFO) << "Unable to transcode DICOM image using the built-in reader";
      return NULL;
    }
  };
  

  
  class IDicomTranscoder1 : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder1()
    {
    }

    virtual DcmFileFormat& GetDicom() = 0;

    virtual DicomTransferSyntax GetTransferSyntax() = 0;

    virtual std::string GetSopClassUid() = 0;

    virtual std::string GetSopInstanceUid() = 0;

    virtual unsigned int GetFramesCount() = 0;

    virtual ImageAccessor* DecodeFrame(unsigned int frame) = 0;

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) = 0;

    // NB: Transcoding can change the value of "GetSopInstanceUid()"
    // and "GetTransferSyntax()" if lossy compression is applied
    virtual bool Transcode(std::string& target,
                           DicomTransferSyntax syntax,
                           bool allowNewSopInstanceUid) = 0;

    virtual void WriteToMemoryBuffer(std::string& target) = 0;
  };


  class DcmtkTranscoder2 : public IDicomTranscoder1
  {
  private:
    std::unique_ptr<DcmFileFormat>    dicom_;
    std::unique_ptr<DicomFrameIndex>  index_;
    DicomTransferSyntax               transferSyntax_;
    std::string                       sopClassUid_;
    std::string                       sopInstanceUid_;
    uint16_t                          bitsStored_;
    unsigned int                      lossyQuality_;

    static std::string GetStringTag(DcmDataset& dataset,
                                    const DcmTagKey& tag)
    {
      const char* value = NULL;

      if (!dataset.findAndGetString(tag, value).good() ||
          value == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing SOP class/instance UID in DICOM instance");
      }
      else
      {
        return std::string(value);
      }
    }

    void Setup(DcmFileFormat* dicom)
    {
      lossyQuality_ = 90;
      
      dicom_.reset(dicom);
      
      if (dicom == NULL ||
          dicom_->getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      DcmDataset& dataset = *dicom_->getDataset();
      index_.reset(new DicomFrameIndex(dataset));

      E_TransferSyntax xfer = dataset.getCurrentXfer();
      if (xfer == EXS_Unknown)
      {
        dataset.updateOriginalXfer();
        xfer = dataset.getCurrentXfer();
        if (xfer == EXS_Unknown)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Cannot determine the transfer syntax of the DICOM instance");
        }
      }

      if (!FromDcmtkBridge::LookupOrthancTransferSyntax(transferSyntax_, xfer))
      {
        throw OrthancException(
          ErrorCode_BadFileFormat,
          "Unsupported transfer syntax: " + boost::lexical_cast<std::string>(xfer));
      }

      if (!dataset.findAndGetUint16(DCM_BitsStored, bitsStored_).good())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      

      sopClassUid_ = GetStringTag(dataset, DCM_SOPClassUID);
      sopInstanceUid_ = GetStringTag(dataset, DCM_SOPInstanceUID);
    }
    
  public:
    DcmtkTranscoder2(DcmFileFormat* dicom)  // Takes ownership
    {
      Setup(dicom);
    }

    DcmtkTranscoder2(const void* dicom,
                    size_t size)
    {
      Setup(FromDcmtkBridge::LoadFromMemoryBuffer(dicom, size));
    }

    void SetLossyQuality(unsigned int quality)
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

    unsigned int GetLossyQuality() const
    {
      return lossyQuality_;
    }

    unsigned int GetBitsStored() const
    {
      return bitsStored_;
    }

    virtual DcmFileFormat& GetDicom()
    {
      assert(dicom_ != NULL);
      return *dicom_;
    }

    virtual DicomTransferSyntax GetTransferSyntax() ORTHANC_OVERRIDE
    {
      return transferSyntax_;
    }

    virtual std::string GetSopClassUid() ORTHANC_OVERRIDE
    {
      return sopClassUid_;
    }
    
    virtual std::string GetSopInstanceUid() ORTHANC_OVERRIDE
    {
      return sopInstanceUid_;
    }

    virtual unsigned int GetFramesCount() ORTHANC_OVERRIDE
    {
      return index_->GetFramesCount();
    }

    virtual void WriteToMemoryBuffer(std::string& target) ORTHANC_OVERRIDE
    {
      if (!FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_))
      {
        throw OrthancException(ErrorCode_InternalError,
                               "Cannot write the DICOM instance to a memory buffer");
      }
    }

    virtual ImageAccessor* DecodeFrame(unsigned int frame) ORTHANC_OVERRIDE
    {
      assert(dicom_->getDataset() != NULL);
      return DicomImageDecoder::Decode(*dicom_->getDataset(), frame);
    }

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) ORTHANC_OVERRIDE
    {
      index_->GetRawFrame(target, frame);
    }

    virtual bool Transcode(std::string& target,
                           DicomTransferSyntax syntax,
                           bool allowNewSopInstanceUid) ORTHANC_OVERRIDE
    {
      assert(dicom_ != NULL &&
             dicom_->getDataset() != NULL);
      
      if (syntax == GetTransferSyntax())
      {
        printf("NO TRANSCODING\n");
        
        // No change in the transfer syntax => simply serialize the current dataset
        WriteToMemoryBuffer(target);
        return true;
      }
      
      printf(">> %d\n", bitsStored_);

      if (syntax == DicomTransferSyntax_LittleEndianImplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianImplicit;
        return true;
      }

      if (syntax == DicomTransferSyntax_LittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianExplicit;
        return true;
      }
      
      if (syntax == DicomTransferSyntax_BigEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_BigEndianExplicit;
        return true;
      }

      if (syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_DeflatedLittleEndianExplicit;
        return true;
      }

#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess1 &&
          allowNewSopInstanceUid &&
          GetBitsStored() == 8)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        
        if (FromDcmtkBridge::Transcode(*dicom_, syntax, &rpLossy) &&
            FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
        {
          transferSyntax_ = DicomTransferSyntax_JPEGProcess1;
          sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
          return true;
        }
      }
#endif
      
#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess2_4 &&
          allowNewSopInstanceUid &&
          GetBitsStored() <= 12)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        if (FromDcmtkBridge::Transcode(*dicom_, syntax, &rpLossy) &&
            FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
        {
          transferSyntax_ = DicomTransferSyntax_JPEGProcess2_4;
          sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
          return true;
        }
      }
#endif

      return false;
    }
  };
}




#include <boost/filesystem.hpp>


static void TestFile(const std::string& path)
{
  static unsigned int count = 0;
  count++;
  

  printf("** %s\n", path.c_str());

  std::string s;
  SystemToolbox::ReadFile(s, path);

  Orthanc::DcmtkTranscoder2 transcoder(s.c_str(), s.size());

  /*if (transcoder.GetBitsStored() != 8)  // TODO
    return; */

  {
    char buf[1024];
    sprintf(buf, "/tmp/source-%06d.dcm", count);
    printf(">> %s\n", buf);
    Orthanc::SystemToolbox::WriteFile(s, buf);
  }

  printf("[%s] [%s] [%s] %d %d\n", GetTransferSyntaxUid(transcoder.GetTransferSyntax()),
         transcoder.GetSopClassUid().c_str(), transcoder.GetSopInstanceUid().c_str(),
         transcoder.GetFramesCount(), transcoder.GetTransferSyntax());

  for (size_t i = 0; i < transcoder.GetFramesCount(); i++)
  {
    std::string f;
    transcoder.GetCompressedFrame(f, i);

    if (i == 0)
    {
      char buf[1024];
      sprintf(buf, "/tmp/frame-%06d.raw", count);
      printf(">> %s\n", buf);
      Orthanc::SystemToolbox::WriteFile(f, buf);
    }
  }

  {
    std::string t;
    transcoder.WriteToMemoryBuffer(t);

    Orthanc::DcmtkTranscoder2 transcoder2(t.c_str(), t.size());
    printf(">> %d %d ; %lu bytes\n", transcoder.GetTransferSyntax(), transcoder2.GetTransferSyntax(), t.size());
  }

  {
    std::string a = transcoder.GetSopInstanceUid();
    DicomTransferSyntax b = transcoder.GetTransferSyntax();
    
    DicomTransferSyntax syntax = DicomTransferSyntax_JPEGProcess2_4;
    //DicomTransferSyntax syntax = DicomTransferSyntax_LittleEndianExplicit;

    std::string t;
    bool ok = transcoder.Transcode(t, syntax, true);
    printf("Transcoding: %d\n", ok);

    if (ok)
    {
      printf("[%s] => [%s]\n", a.c_str(), transcoder.GetSopInstanceUid().c_str());
      printf("[%s] => [%s]\n", GetTransferSyntaxUid(b),
             GetTransferSyntaxUid(transcoder.GetTransferSyntax()));
      
      {
        char buf[1024];
        sprintf(buf, "/tmp/transcoded-%06d.dcm", count);
        printf(">> %s\n", buf);
        Orthanc::SystemToolbox::WriteFile(t, buf);
      }

      Orthanc::DcmtkTranscoder2 transcoder2(t.c_str(), t.size());
      printf("  => transcoded transfer syntax %d ; %lu bytes\n", transcoder2.GetTransferSyntax(), t.size());
    }
  }
  
  printf("\n");
}

TEST(Toto, DISABLED_Transcode)
{
  //OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);

  if (1)
  {
    const char* const PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes";
    
    for (boost::filesystem::directory_iterator it(PATH);
         it != boost::filesystem::directory_iterator(); ++it)
    {
      if (boost::filesystem::is_regular_file(it->status()))
      {
        TestFile(it->path().string());
      }
    }
  }

  if (0)
  {
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Multiframe.dcm");
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Issue44/Monochrome1-Jpeg.dcm");
  }

  if (0)
  {
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/1.2.840.10008.1.2.1.dcm");
  }
}


TEST(Toto, DISABLED_Transcode2)
{
  for (int i = 0; i <= DicomTransferSyntax_XML; i++)
  {
    DicomTransferSyntax a = (DicomTransferSyntax) i;

    std::string path = ("/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/" +
                        std::string(GetTransferSyntaxUid(a)) + ".dcm");
    if (Orthanc::SystemToolbox::IsRegularFile(path))
    {
      printf("\n======= %s\n", GetTransferSyntaxUid(a));

      std::string source;
      Orthanc::SystemToolbox::ReadFile(source, path);

      DcmtkImageReader reader;

      {
        std::unique_ptr<IParsedDicomImage> image(
          reader.Read(source.c_str(), source.size()));
        ASSERT_TRUE(image.get() != NULL);
        ASSERT_EQ(a, image->GetTransferSyntax());

        std::string target;
        image->WriteToMemoryBuffer(target);
      }

      for (int j = 0; j <= DicomTransferSyntax_XML; j++)
      {
        DicomTransferSyntax b = (DicomTransferSyntax) j;
        //if (a == b) continue;

        std::unique_ptr<IParsedDicomImage> image(
          reader.Transcode(source.c_str(), source.size(), b, true));
        if (image.get() != NULL)
        {
          printf("[%s] -> [%s]\n", GetTransferSyntaxUid(a), GetTransferSyntaxUid(b));

          std::string target;
          image->WriteToMemoryBuffer(target);

          char buf[1024];
          sprintf(buf, "/tmp/%s-%s.dcm", GetTransferSyntaxUid(a), GetTransferSyntaxUid(b));
          
          SystemToolbox::WriteFile(target, buf);
        }
        else if (a != DicomTransferSyntax_JPEG2000 &&
                 a != DicomTransferSyntax_JPEG2000LosslessOnly)
        {
          ASSERT_TRUE(b != DicomTransferSyntax_LittleEndianImplicit &&
                      b != DicomTransferSyntax_LittleEndianExplicit &&
                      b != DicomTransferSyntax_BigEndianExplicit &&
                      b != DicomTransferSyntax_DeflatedLittleEndianExplicit);
        }
      }
    }
  }
}


#include "../Core/DicomNetworking/DicomAssociation.h"
#include "../Core/DicomNetworking/DicomControlUserConnection.h"
#include "../Core/DicomNetworking/DicomStoreUserConnection.h"

TEST(Toto, DISABLED_DicomAssociation)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("PACS");
  params.SetRemotePort(2001);

#if 0
  DicomAssociation assoc;
  assoc.ProposeGenericPresentationContext(UID_StorageCommitmentPushModelSOPClass);
  assoc.ProposeGenericPresentationContext(UID_VerificationSOPClass);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEGProcess1);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEGProcess2_4);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEG2000);
  
  assoc.Open(params);

  int presID = ASC_findAcceptedPresentationContextID(&assoc.GetDcmtkAssociation(), UID_ComputedRadiographyImageStorage);
  printf(">> %d\n", presID);
    
  std::map<DicomTransferSyntax, uint8_t> pc;
  printf(">> %d\n", assoc.LookupAcceptedPresentationContext(pc, UID_ComputedRadiographyImageStorage));
  
  for (std::map<DicomTransferSyntax, uint8_t>::const_iterator
         it = pc.begin(); it != pc.end(); ++it)
  {
    printf("[%s] => %d\n", GetTransferSyntaxUid(it->first), it->second);
  }
#else
  {
    DicomControlUserConnection assoc(params);

    try
    {
      printf(">> %d\n", assoc.Echo());
    }
    catch (OrthancException&)
    {
    }
  }
    
  params.SetRemoteApplicationEntityTitle("PACS");
  params.SetRemotePort(2000);

  {
    DicomControlUserConnection assoc(params);
    printf(">> %d\n", assoc.Echo());
  }

#endif
}

static void TestTranscode(DicomStoreUserConnection& scu,
                          const std::string& sopClassUid,
                          DicomTransferSyntax transferSyntax)
{
  std::set<DicomTransferSyntax> accepted;

  scu.LookupTranscoding(accepted, sopClassUid, transferSyntax);
  if (accepted.empty())
  {
    throw OrthancException(ErrorCode_NetworkProtocol,
                           "The SOP class is not supported by the remote modality");
  }

  {
    unsigned int count = 0;
    for (std::set<DicomTransferSyntax>::const_iterator
           it = accepted.begin(); it != accepted.end(); ++it)
    {
      LOG(INFO) << "available for transcoding " << (count++) << ": " << sopClassUid
                << " / " << GetTransferSyntaxUid(*it);
    }
  }
  
  if (accepted.find(transferSyntax) != accepted.end())
  {
    printf("**** OK, without transcoding !! [%s]\n", GetTransferSyntaxUid(transferSyntax));
  }
  else
  {
    // Transcoding - only in Orthanc >= 1.7.0

    const DicomTransferSyntax uncompressed[] = {
      DicomTransferSyntax_LittleEndianImplicit,  // Default transfer syntax
      DicomTransferSyntax_LittleEndianExplicit,
      DicomTransferSyntax_BigEndianExplicit
    };

    bool found = false;
    for (size_t i = 0; i < 3; i++)
    {
      if (accepted.find(uncompressed[i]) != accepted.end())
      {
        printf("**** TRANSCODING to %s\n", GetTransferSyntaxUid(uncompressed[i]));
        found = true;
        break;
      }
    }

    if (!found)
    {
      printf("**** KO KO KO\n");
    }
  }
}


TEST(Toto, DISABLED_Store)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("STORESCP");
  params.SetRemotePort(2000);

  DicomStoreUserConnection assoc(params);
  assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_JPEGProcess1);
  assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_JPEGProcess2_4);
  //assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_LittleEndianExplicit);

  //assoc.SetUncompressedSyntaxesProposed(false);  // Necessary for transcoding
  assoc.SetCommonClassesProposed(false);
  assoc.SetRetiredBigEndianProposed(true);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_LittleEndianExplicit);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_JPEG2000);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_JPEG2000);
}


TEST(Toto, DISABLED_Store2)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("STORESCP");
  params.SetRemotePort(2000);

  DicomStoreUserConnection assoc(params);
  //assoc.SetCommonClassesProposed(false);
  assoc.SetRetiredBigEndianProposed(true);

  std::string s;
  Orthanc::SystemToolbox::ReadFile(s, "/tmp/i/" + std::string(GetTransferSyntaxUid(DicomTransferSyntax_BigEndianExplicit)) +".dcm");

  std::string c, i;
  assoc.Store(c, i, s.c_str(), s.size());
  printf("[%s] [%s]\n", c.c_str(), i.c_str());
}

