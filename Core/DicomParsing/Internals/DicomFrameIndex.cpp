/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../../PrecompiledHeaders.h"
#include "DicomFrameIndex.h"

#include "../../OrthancException.h"
#include "../../DicomFormat/DicomImageInformation.h"
#include "../FromDcmtkBridge.h"
#include "../../Endianness.h"
#include "DicomImageDecoder.h"

#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmdata/dcpixseq.h>

namespace Orthanc
{
  class DicomFrameIndex::FragmentIndex : public DicomFrameIndex::IIndex
  {
  private:
    DcmPixelSequence*           pixelSequence_;
    std::vector<DcmPixelItem*>  startFragment_;
    std::vector<unsigned int>   countFragments_;
    std::vector<unsigned int>   frameSize_;

    void GetOffsetTable(std::vector<uint32_t>& table)
    {
      DcmPixelItem* item = NULL;
      if (!pixelSequence_->getItem(item, 0).good() ||
          item == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      uint32_t length = item->getLength();
      if (length == 0)
      {
        table.clear();
        return;
      }

      if (length % 4 != 0)
      {
        // Error: Each fragment is index with 4 bytes (uint32_t)
        throw OrthancException(ErrorCode_BadFileFormat);        
      }

      uint8_t* content = NULL;
      if (!item->getUint8Array(content).good() ||
          content == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      table.resize(length / 4);

      // The offset table is always in little endian in the DICOM
      // file. Swap it to host endianness if needed.
      const uint32_t* offset = reinterpret_cast<const uint32_t*>(content);
      for (size_t i = 0; i < table.size(); i++, offset++)
      {
        table[i] = le32toh(*offset);
      }
    }


  public:
    FragmentIndex(DcmPixelSequence* pixelSequence,
                  unsigned int countFrames) :
      pixelSequence_(pixelSequence)
    {
      assert(pixelSequence != NULL);

      startFragment_.resize(countFrames);
      countFragments_.resize(countFrames);
      frameSize_.resize(countFrames);

      // The first fragment corresponds to the offset table
      unsigned int countFragments = static_cast<unsigned int>(pixelSequence_->card());
      if (countFragments < countFrames + 1)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      if (countFragments == countFrames + 1)
      {
        // Simple case: There is one fragment per frame.

        DcmObject* fragment = pixelSequence_->nextInContainer(NULL);  // Skip the offset table
        if (fragment == NULL)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        for (unsigned int i = 0; i < countFrames; i++)
        {
          fragment = pixelSequence_->nextInContainer(fragment);
          startFragment_[i] = dynamic_cast<DcmPixelItem*>(fragment);
          frameSize_[i] = fragment->getLength();
          countFragments_[i] = 1;
        }

        return;
      }

      // Parse the offset table
      std::vector<uint32_t> offsetOfFrame;
      GetOffsetTable(offsetOfFrame);
      
      if (offsetOfFrame.size() != countFrames ||
          offsetOfFrame[0] != 0)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }


      // Loop over the fragments (ignoring the offset table). This is
      // an alternative, faster implementation to DCMTK's
      // "DcmCodec::determineStartFragment()".
      DcmObject* fragment = pixelSequence_->nextInContainer(NULL);
      if (fragment == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      fragment = pixelSequence_->nextInContainer(fragment); // Skip the offset table
      if (fragment == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      uint32_t offset = 0;
      unsigned int currentFrame = 0;
      startFragment_[0] = dynamic_cast<DcmPixelItem*>(fragment);

      unsigned int currentFragment = 1;
      while (fragment != NULL)
      {
        if (currentFrame + 1 < countFrames &&
            offset == offsetOfFrame[currentFrame + 1])
        {
          currentFrame += 1;
          startFragment_[currentFrame] = dynamic_cast<DcmPixelItem*>(fragment);
        }

        frameSize_[currentFrame] += fragment->getLength();
        countFragments_[currentFrame]++;

        // 8 bytes = overhead for the item tag and length field
        offset += fragment->getLength() + 8;

        currentFragment++;
        fragment = pixelSequence_->nextInContainer(fragment);
      }

      if (currentFragment != countFragments ||
          currentFrame + 1 != countFrames ||
          fragment != NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      assert(startFragment_.size() == countFragments_.size() &&
             startFragment_.size() == frameSize_.size());
    }


    virtual void GetRawFrame(std::string& frame,
                             unsigned int index) const
    {
      if (index >= startFragment_.size())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      frame.resize(frameSize_[index]);
      if (frame.size() == 0)
      {
        return;
      }

      uint8_t* target = reinterpret_cast<uint8_t*>(&frame[0]);

      size_t offset = 0;
      DcmPixelItem* fragment = startFragment_[index];
      for (unsigned int i = 0; i < countFragments_[index]; i++)
      {
        uint8_t* content = NULL;
        if (!fragment->getUint8Array(content).good() ||
            content == NULL)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        assert(offset + fragment->getLength() <= frame.size());

        memcpy(target + offset, content, fragment->getLength());
        offset += fragment->getLength();

        fragment = dynamic_cast<DcmPixelItem*>(pixelSequence_->nextInContainer(fragment));
      }
    }
  };



  class DicomFrameIndex::UncompressedIndex : public DicomFrameIndex::IIndex
  {
  private:
    uint8_t*  pixelData_;
    size_t    frameSize_;

  public: 
    UncompressedIndex(DcmDataset& dataset,
                      unsigned int countFrames,
                      size_t frameSize) :
      pixelData_(NULL),
      frameSize_(frameSize)
    {
      size_t size = 0;

      DcmElement* e;
      if (dataset.findAndGetElement(DCM_PixelData, e).good() &&
          e != NULL)
      {
        size = e->getLength();

        if (size > 0)
        {
          pixelData_ = NULL;
          if (!e->getUint8Array(pixelData_).good() ||
              pixelData_ == NULL)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
        }
      }

      if (size < frameSize_ * countFrames)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    virtual void GetRawFrame(std::string& frame,
                             unsigned int index) const
    {
      frame.resize(frameSize_);
      if (frameSize_ > 0)
      {
        memcpy(&frame[0], pixelData_ + index * frameSize_, frameSize_);
      }
    }
  };


  class DicomFrameIndex::PsmctRle1Index : public DicomFrameIndex::IIndex
  {
  private:
    std::string  pixelData_;
    size_t       frameSize_;

  public: 
    PsmctRle1Index(DcmDataset& dataset,
                   unsigned int countFrames,
                   size_t frameSize) :
      frameSize_(frameSize)
    {
      if (!DicomImageDecoder::DecodePsmctRle1(pixelData_, dataset) ||
          pixelData_.size() < frameSize * countFrames)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    virtual void GetRawFrame(std::string& frame,
                             unsigned int index) const
    {
      frame.resize(frameSize_);
      if (frameSize_ > 0)
      {
        memcpy(&frame[0], reinterpret_cast<const uint8_t*>(&pixelData_[0]) + index * frameSize_, frameSize_);
      }
    }
  };



  bool DicomFrameIndex::IsVideo(DcmFileFormat& dicom)
  {
    // Retrieve the transfer syntax from the DICOM header
    const char* value = NULL;
    if (!dicom.getMetaInfo()->findAndGetString(DCM_TransferSyntaxUID, value).good() ||
        value == NULL)
    {
      return false;
    }

    const std::string transferSyntax(value);

    // Video standards supported in DICOM 2016a
    // http://dicom.nema.org/medical/dicom/2016a/output/html/part05.html
    if (transferSyntax == "1.2.840.10008.1.2.4.100" ||  // MPEG2 MP@ML option of ISO/IEC MPEG2
        transferSyntax == "1.2.840.10008.1.2.4.101" ||  // MPEG2 MP@HL option of ISO/IEC MPEG2
        transferSyntax == "1.2.840.10008.1.2.4.102" ||  // MPEG-4 AVC/H.264 High Profile / Level 4.1 of ITU-T H.264
        transferSyntax == "1.2.840.10008.1.2.4.103" ||  // MPEG-4 AVC/H.264 BD-compat High Profile / Level 4.1 of ITU-T H.264
        transferSyntax == "1.2.840.10008.1.2.4.104" ||  // MPEG-4 AVC/H.264 High Profile / Level 4.2 of ITU-T H.264
        transferSyntax == "1.2.840.10008.1.2.4.105" ||  // MPEG-4 AVC/H.264 High Profile / Level 4.2 of ITU-T H.264
        transferSyntax == "1.2.840.10008.1.2.4.106")    // MPEG-4 AVC/H.264 Stereo High Profile / Level 4.2 of the ITU-T H.264
    {
      return true;
    }

    return false;
  }


  unsigned int DicomFrameIndex::GetFramesCount(DcmFileFormat& dicom)
  {
    // Assume 1 frame for video transfer syntaxes
    if (IsVideo(dicom))
    {
      return 1;
    }        

    const char* tmp = NULL;
    if (!dicom.getDataset()->findAndGetString(DCM_NumberOfFrames, tmp).good() ||
        tmp == NULL)
    {
      return 1;
    }

    int count = -1;
    try
    {
      count = boost::lexical_cast<int>(tmp);
    }
    catch (boost::bad_lexical_cast&)
    {
    }

    if (count < 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat);        
    }
    else
    {
      return count;
    }
  }


  DicomFrameIndex::DicomFrameIndex(DcmFileFormat& dicom)
  {
    countFrames_ = GetFramesCount(dicom);
    if (countFrames_ == 0)
    {
      // The image has no frame. No index is to be built.
      return;
    }

    DcmDataset& dataset = *dicom.getDataset();

    // Test whether this image is composed of a sequence of fragments
    DcmPixelSequence* pixelSequence = FromDcmtkBridge::GetPixelSequence(dataset);
    if (pixelSequence != NULL)
    {
      index_.reset(new FragmentIndex(pixelSequence, countFrames_));
      return;
    }

    // Extract information about the image structure
    DicomMap tags;
    FromDcmtkBridge::ExtractDicomSummary(tags, dataset);

    DicomImageInformation information(tags);

    // Access to the raw pixel data
    if (DicomImageDecoder::IsPsmctRle1(dataset))
    {
      index_.reset(new PsmctRle1Index(dataset, countFrames_, information.GetFrameSize()));
    }
    else
    {
      index_.reset(new UncompressedIndex(dataset, countFrames_, information.GetFrameSize()));
    }
  }


  void DicomFrameIndex::GetRawFrame(std::string& frame,
                                    unsigned int index) const
  {
    if (index >= countFrames_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else if (index_.get() != NULL)
    {
      return index_->GetRawFrame(frame, index);
    }
    else
    {
      frame.clear();
    }
  }
}
