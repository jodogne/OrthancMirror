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
#include "DicomStreamReader.h"

#include "../OrthancException.h"

#include <cassert>
#include <sstream>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>


#include <iostream>

namespace Orthanc
{
  static bool IsNormalizationNeeded(const std::string& source,
                                    ValueRepresentation vr)
  {
    return (!source.empty() &&
            (source[source.size() - 1] == ' ' ||
             source[source.size() - 1] == '\0') &&
            // Normalization only applies to string-based VR
            (vr == ValueRepresentation_ApplicationEntity ||
             vr == ValueRepresentation_AgeString ||
             vr == ValueRepresentation_CodeString ||
             vr == ValueRepresentation_DecimalString ||
             vr == ValueRepresentation_IntegerString ||
             vr == ValueRepresentation_LongString ||
             vr == ValueRepresentation_LongText ||
             vr == ValueRepresentation_PersonName ||
             vr == ValueRepresentation_ShortString ||
             vr == ValueRepresentation_ShortText ||
             vr == ValueRepresentation_UniqueIdentifier ||
             vr == ValueRepresentation_UnlimitedText));
  }

  
  static void NormalizeValue(std::string& inplace,
                             ValueRepresentation vr)
  {
    if (IsNormalizationNeeded(inplace, vr))
    {
      assert(!inplace.empty());
      inplace.resize(inplace.size() - 1);
    }
  }

    
  static uint16_t ReadUnsignedInteger16(const char* dicom,
                                        bool littleEndian)
  {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

    if (littleEndian)
    {
      return (static_cast<uint16_t>(p[0]) |
              (static_cast<uint16_t>(p[1]) << 8));
    }
    else
    {
      return (static_cast<uint16_t>(p[1]) |
              (static_cast<uint16_t>(p[0]) << 8));
    }
  }


  static uint32_t ReadUnsignedInteger32(const char* dicom,
                                        bool littleEndian)
  {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

    if (littleEndian)
    {
      return (static_cast<uint32_t>(p[0]) |
              (static_cast<uint32_t>(p[1]) << 8) |
              (static_cast<uint32_t>(p[2]) << 16) |
              (static_cast<uint32_t>(p[3]) << 24));
    }
    else
    {
      return (static_cast<uint32_t>(p[3]) |
              (static_cast<uint32_t>(p[2]) << 8) |
              (static_cast<uint32_t>(p[1]) << 16) |
              (static_cast<uint32_t>(p[0]) << 24));
    }
  }


  static DicomTag ReadTag(const char* dicom,
                          bool littleEndian)
  {
    return DicomTag(ReadUnsignedInteger16(dicom, littleEndian),
                    ReadUnsignedInteger16(dicom + 2, littleEndian));
  }


  static bool IsShortExplicitTag(ValueRepresentation vr)
  {
    /**
     * Are we in the case of Table 7.1-2? "Data Element with
     * Explicit VR of AE, AS, AT, CS, DA, DS, DT, FL, FD, IS, LO,
     * LT, PN, SH, SL, SS, ST, TM, UI, UL and US"
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_7.html#sect_7.1.2
     **/
    return (vr == ValueRepresentation_ApplicationEntity   /* AE */ ||
            vr == ValueRepresentation_AgeString           /* AS */ ||
            vr == ValueRepresentation_AttributeTag        /* AT */ ||
            vr == ValueRepresentation_CodeString          /* CS */ ||
            vr == ValueRepresentation_Date                /* DA */ ||
            vr == ValueRepresentation_DecimalString       /* DS */ ||
            vr == ValueRepresentation_DateTime            /* DT */ ||
            vr == ValueRepresentation_FloatingPointSingle /* FL */ ||
            vr == ValueRepresentation_FloatingPointDouble /* FD */ ||
            vr == ValueRepresentation_IntegerString       /* IS */ ||
            vr == ValueRepresentation_LongString          /* LO */ ||
            vr == ValueRepresentation_LongText            /* LT */ ||
            vr == ValueRepresentation_PersonName          /* PN */ ||
            vr == ValueRepresentation_ShortString         /* SH */ ||
            vr == ValueRepresentation_SignedLong          /* SL */ ||
            vr == ValueRepresentation_SignedShort         /* SS */ ||
            vr == ValueRepresentation_ShortText           /* ST */ ||
            vr == ValueRepresentation_Time                /* TM */ ||
            vr == ValueRepresentation_UniqueIdentifier    /* UI */ ||
            vr == ValueRepresentation_UnsignedLong        /* UL */ ||
            vr == ValueRepresentation_UnsignedShort       /* US */);
  }


  bool DicomStreamReader::IsLittleEndian() const
  {
    return (transferSyntax_ != DicomTransferSyntax_BigEndianExplicit);
  }


  void DicomStreamReader::HandlePreamble(IVisitor& visitor,
                                         const std::string& block)
  {
    assert(block.size() == 144u);
    assert(reader_.GetProcessedBytes() == 144u);

    /**
     * The "DICOM file meta information" is always encoded using
     * "Explicit VR Little Endian Transfer Syntax"
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part10/chapter_7.html
     **/
    if (block[128] != 'D' ||
        block[129] != 'I' ||
        block[130] != 'C' ||
        block[131] != 'M' ||
        ReadTag(block.c_str() + 132, true) != DicomTag(0x0002, 0x0000) ||
        block[136] != 'U' ||
        block[137] != 'L' ||
        ReadUnsignedInteger16(block.c_str() + 138, true) != 4)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    uint32_t length = ReadUnsignedInteger32(block.c_str() + 140, true);

    reader_.Schedule(length);
    state_ = State_MetaHeader;
  }


  void DicomStreamReader::HandleMetaHeader(IVisitor& visitor,
                                           const std::string& block)
  {
    size_t pos = 0;
    const char* p = block.c_str();

    bool hasTransferSyntax = false;

    while (pos + 8 <= block.size())
    {
      DicomTag tag = ReadTag(p + pos, true);
        
      ValueRepresentation vr = StringToValueRepresentation(std::string(p + pos + 4, 2), true);

      if (IsShortExplicitTag(vr))
      {
        uint16_t length = ReadUnsignedInteger16(p + pos + 6, true);

        std::string value;
        value.assign(p + pos + 8, length);
        NormalizeValue(value, vr);

        if (tag.GetGroup() == 0x0002)
        {
          visitor.VisitMetaHeaderTag(tag, vr, value);
        }                  

        if (tag == DICOM_TAG_TRANSFER_SYNTAX_UID)
        {
          if (LookupTransferSyntax(transferSyntax_, value))
          {
            hasTransferSyntax = true;
          }
          else
          {
            throw OrthancException(ErrorCode_NotImplemented, "Unsupported transfer syntax: " + value);
          }
        }
          
        pos += length + 8;
      }
      else if (pos + 12 <= block.size())
      {
        uint16_t reserved = ReadUnsignedInteger16(p + pos + 6, true);
        if (reserved != 0)
        {
          break;
        }
          
        uint32_t length = ReadUnsignedInteger32(p + pos + 8, true);

        if (tag.GetGroup() == 0x0002)
        {
          std::string value;
          value.assign(p + pos + 12, length);
          NormalizeValue(value, vr);
          visitor.VisitMetaHeaderTag(tag, vr, value);
        }                  
          
        pos += length + 12;
      }
    }

    if (pos != block.size())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    if (!hasTransferSyntax)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "DICOM file meta-header without transfer syntax UID");
    }

    visitor.VisitTransferSyntax(transferSyntax_);

    reader_.Schedule(8);
    state_ = State_DatasetTag;
  }
    

  void DicomStreamReader::HandleDatasetTag(const std::string& block,
                                           const DicomTag& untilTag)
  {
    static const DicomTag DICOM_TAG_SEQUENCE_ITEM(0xfffe, 0xe000);
    static const DicomTag DICOM_TAG_SEQUENCE_DELIMITATION_ITEM(0xfffe, 0xe00d);
    static const DicomTag DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE(0xfffe, 0xe0dd);

    assert(block.size() == 8u);

    const bool littleEndian = IsLittleEndian();
    DicomTag tag = ReadTag(block.c_str(), littleEndian);

    if (sequenceDepth_ == 0 &&
        tag >= untilTag)
    {
      state_ = State_Done;
      return;
    }
      
    if (tag == DICOM_TAG_SEQUENCE_ITEM ||
        tag == DICOM_TAG_SEQUENCE_DELIMITATION_ITEM ||
        tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
    {
      // The special sequence items are encoded like "Implicit VR"
      uint32_t length = ReadUnsignedInteger32(block.c_str() + 4, littleEndian);

      if (tag == DICOM_TAG_SEQUENCE_ITEM)
      {
        if (length == 0xffffffffu)
        {
          // Undefined length: Need to loop over the tags of the nested dataset
          reader_.Schedule(8);
          state_ = State_DatasetTag;
        }
        else
        {
          // Explicit length: Can skip the full sequence at once
          reader_.Schedule(length);
          state_ = State_DatasetValue;
        }
      }
      else if (tag == DICOM_TAG_SEQUENCE_DELIMITATION_ITEM ||
               tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
      {
        if (length != 0 ||
            sequenceDepth_ == 0)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }

        if (tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
        {
          sequenceDepth_ --;
        }

        reader_.Schedule(8);
        state_ = State_DatasetTag;          
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      assert(reader_.GetProcessedBytes() >= block.size());
      const uint64_t tagOffset = reader_.GetProcessedBytes() - block.size();
        
      ValueRepresentation vr = ValueRepresentation_Unknown;
        
      if (transferSyntax_ == DicomTransferSyntax_LittleEndianImplicit)
      {
        if (sequenceDepth_ == 0)
        {
          danglingTag_ = tag;
          danglingVR_ = vr;
          danglingOffset_ = tagOffset;
        }

        uint32_t length = ReadUnsignedInteger32(block.c_str() + 4, true /* little endian */);
        HandleDatasetExplicitLength(length);
      }
      else
      {
        // This in an explicit transfer syntax

        vr = StringToValueRepresentation(
          std::string(block.c_str() + 4, 2), false /* ignore unknown VR */);

        if (vr == ValueRepresentation_Sequence)
        {
          sequenceDepth_ ++;
          reader_.Schedule(4);
          state_ = State_SequenceExplicitLength;
        }
        else if (IsShortExplicitTag(vr))
        {
          uint16_t length = ReadUnsignedInteger16(block.c_str() + 6, littleEndian);

          reader_.Schedule(length);
          state_ = State_DatasetValue;
        }
        else
        {
          uint16_t reserved = ReadUnsignedInteger16(block.c_str() + 6, littleEndian);
          if (reserved != 0)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }

          reader_.Schedule(4);
          state_ = State_DatasetExplicitLength;
        }

        if (sequenceDepth_ == 0)
        {
          danglingTag_ = tag;
          danglingVR_ = vr;
          danglingOffset_ = tagOffset;
        }
      }
    }
  }


  void DicomStreamReader::HandleDatasetExplicitLength(uint32_t length)
  {
    if (length == 0xffffffffu)
    {
      /**
       * This is the case of pixel data with compressed transfer
       * syntaxes. Schedule the reading of the first tag of the
       * nested dataset.
       * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_7.5.html
       **/
      state_ = State_DatasetTag;
      reader_.Schedule(8);
      sequenceDepth_ ++;
    }
    else
    {
      reader_.Schedule(length);
      state_ = State_DatasetValue;
    }
  }    

    
  void DicomStreamReader::HandleDatasetExplicitLength(IVisitor& visitor,
                                                      const std::string& block)
  {
    assert(block.size() == 4);

    uint32_t length = ReadUnsignedInteger32(block.c_str(), IsLittleEndian());
    HandleDatasetExplicitLength(length);

    std::string empty;
    if (!visitor.VisitDatasetTag(danglingTag_, danglingVR_, empty, IsLittleEndian(), danglingOffset_))
    {
      state_ = State_Done;
    }
  }
    

  void DicomStreamReader::HandleSequenceExplicitLength(const std::string& block)
  {
    assert(block.size() == 4);

    uint32_t length = ReadUnsignedInteger32(block.c_str(), IsLittleEndian());
    if (length == 0xffffffffu)
    {
      state_ = State_DatasetTag;
      reader_.Schedule(8);
    }
    else
    {
      reader_.Schedule(length);
      state_ = State_SequenceExplicitValue;
    }
  }

    
  void DicomStreamReader::HandleSequenceExplicitValue()
  {
    if (sequenceDepth_ == 0)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    sequenceDepth_ --;

    state_ = State_DatasetTag;
    reader_.Schedule(8);
  }


  void DicomStreamReader::HandleDatasetValue(IVisitor& visitor,
                                             const std::string& block)
  {
    if (sequenceDepth_ == 0)
    {
      bool c;

      if (IsNormalizationNeeded(block, danglingVR_))
      {
        std::string s(block.begin(), block.end() - 1);
        c = visitor.VisitDatasetTag(danglingTag_, danglingVR_, s, IsLittleEndian(), danglingOffset_);
      }
      else
      {
        c = visitor.VisitDatasetTag(danglingTag_, danglingVR_, block, IsLittleEndian(), danglingOffset_);
      }
      
      if (!c)
      {
        state_ = State_Done;
        return;
      }
    }

    reader_.Schedule(8);
    state_ = State_DatasetTag;
  }
    
    
  DicomStreamReader::DicomStreamReader(std::istream& stream) :
    reader_(stream),
    state_(State_Preamble),
    transferSyntax_(DicomTransferSyntax_LittleEndianImplicit),  // Dummy
    danglingTag_(0x0000, 0x0000),  // Dummy
    danglingVR_(ValueRepresentation_Unknown),  // Dummy
    danglingOffset_(0),  // Dummy
    sequenceDepth_(0)
  {
    reader_.Schedule(128 /* empty header */ +
                     4 /* "DICM" magic value */ +
                     4 /* (0x0002, 0x0000) tag */ +
                     2 /* value representation of (0x0002, 0x0000) == "UL" */ +
                     2 /* length of "UL" value == 4 */ +
                     4 /* actual length of the meta-header */);
  }

  
  void DicomStreamReader::Consume(IVisitor& visitor,
                                  const DicomTag& untilTag)
  {
    while (state_ != State_Done)
    {
      std::string block;
      if (reader_.Read(block))
      {
        switch (state_)
        {
          case State_Preamble:
            HandlePreamble(visitor, block);
            break;

          case State_MetaHeader:
            HandleMetaHeader(visitor, block);
            break;

          case State_DatasetTag:
            HandleDatasetTag(block, untilTag);
            break;

          case State_DatasetExplicitLength:
            HandleDatasetExplicitLength(visitor, block);
            break;

          case State_SequenceExplicitLength:
            HandleSequenceExplicitLength(block);
            break;

          case State_SequenceExplicitValue:
            HandleSequenceExplicitValue();
            break;

          case State_DatasetValue:
            HandleDatasetValue(visitor, block);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        return;  // No more data in the stream
      }
    }
  }


  void DicomStreamReader::Consume(IVisitor& visitor)
  {
    DicomTag untilTag(0xffff, 0xffff);
    Consume(visitor, untilTag);
  }


  bool DicomStreamReader::IsDone() const
  {
    return (state_ == State_Done);
  }

  
  uint64_t DicomStreamReader::GetProcessedBytes() const
  {
    return reader_.GetProcessedBytes();
  }


  class DicomStreamReader::PixelDataVisitor : public DicomStreamReader::IVisitor
  {
  private:
    bool      hasPixelData_;
    uint64_t  pixelDataOffset_;
    
  public:
    PixelDataVisitor() :
      hasPixelData_(false),
      pixelDataOffset_(0)
    {
    }
    
    virtual void VisitMetaHeaderTag(const DicomTag& tag,
                                    const ValueRepresentation& vr,
                                    const std::string& value) ORTHANC_OVERRIDE
    {
    }

    virtual void VisitTransferSyntax(DicomTransferSyntax transferSyntax) ORTHANC_OVERRIDE
    {
    }
    
    virtual bool VisitDatasetTag(const DicomTag& tag,
                                 const ValueRepresentation& vr,
                                 const std::string& value,
                                 bool isLittleEndian,
                                 uint64_t fileOffset) ORTHANC_OVERRIDE
    {
      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        hasPixelData_ = true;
        pixelDataOffset_ = fileOffset;
      }

      // Stop processing once pixel data has been passed
      return (tag < DICOM_TAG_PIXEL_DATA);
    }

    bool HasPixelData() const
    {
      return hasPixelData_;
    }

    uint64_t GetPixelDataOffset() const
    {
      return pixelDataOffset_;
    }

    static bool LookupPixelDataOffset(uint64_t& offset,
                                      std::istream& stream)
    {
      PixelDataVisitor visitor;
      bool isLittleEndian;

      {
        DicomStreamReader reader(stream);

        try
        {
          reader.Consume(visitor);
          isLittleEndian = reader.IsLittleEndian();
        }
        catch (OrthancException& e)
        {
          // Invalid DICOM file
          return false;
        }
      }

      if (visitor.HasPixelData())
      {
        // Sanity check if we face an unsupported DICOM file: Make
        // sure that we can read DICOM_TAG_PIXEL_DATA at the reported
        // position in the stream
        stream.seekg(visitor.GetPixelDataOffset(), stream.beg);
        
        std::string s;
        s.resize(4);
        stream.read(&s[0], s.size());

        if (!isLittleEndian)
        {
          // Byte swapping if reading a file whose transfer syntax is
          // 1.2.840.10008.1.2.2 (big endian explicit)
          std::swap(s[0], s[1]);
          std::swap(s[2], s[3]);          
        }
        
        if (stream.gcount() == static_cast<std::streamsize>(s.size()) &&
            s[0] == char(0xe0) &&
            s[1] == char(0x7f) &&
            s[2] == char(0x10) &&
            s[3] == char(0x00))
        {
          offset = visitor.GetPixelDataOffset();
          return true;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
  };

  
  bool DicomStreamReader::LookupPixelDataOffset(uint64_t& offset,
                                                const std::string& dicom)
  {
    std::stringstream stream(dicom);
    return PixelDataVisitor::LookupPixelDataOffset(offset, stream);
  }
  

  bool DicomStreamReader::LookupPixelDataOffset(uint64_t& offset,
                                                const void* buffer,
                                                size_t size)
  {
    boost::iostreams::array_source source(reinterpret_cast<const char*>(buffer), size);
    boost::iostreams::stream<boost::iostreams::array_source> stream(source);
    return PixelDataVisitor::LookupPixelDataOffset(offset, stream);
  }
}

