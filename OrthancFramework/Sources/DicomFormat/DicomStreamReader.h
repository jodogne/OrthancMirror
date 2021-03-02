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

#include "DicomTag.h"
#include "StreamBlockReader.h"

namespace Orthanc
{
  /**
   * This class parses a stream containing a DICOM instance, using a
   * state machine.
   *
   * It does *not* support the visit of sequences (it only works at
   * the first level of the hierarchy), and as a consequence, it
   * doesn't give access to the pixel data of compressed transfer
   * syntaxes.
   **/
  class ORTHANC_PUBLIC DicomStreamReader : public boost::noncopyable
  {
  public:
    class IVisitor : public boost::noncopyable
    {
    public:
      virtual ~IVisitor()
      {
      }

      // The data from this function will always be Little Endian (as
      // specified by the DICOM standard)
      virtual void VisitMetaHeaderTag(const DicomTag& tag,
                                      const ValueRepresentation& vr,
                                      const std::string& value) = 0;

      virtual void VisitTransferSyntax(DicomTransferSyntax transferSyntax) = 0;

      // Return "false" to stop processing
      virtual bool VisitDatasetTag(const DicomTag& tag,
                                   const ValueRepresentation& vr,
                                   const std::string& value,
                                   bool isLittleEndian,
                                   uint64_t fileOffset) = 0;
    };
    
  private:
    class PixelDataVisitor;
    
    enum State
    {
      State_Preamble,
      State_MetaHeader,
      State_DatasetTag,
      State_SequenceExplicitLength,
      State_SequenceExplicitValue,
      State_DatasetExplicitLength,
      State_DatasetValue,
      State_Done
    };

    StreamBlockReader    reader_;
    State                state_;
    DicomTransferSyntax  transferSyntax_;
    DicomTag             danglingTag_;  // Current root-level tag
    ValueRepresentation  danglingVR_;
    uint64_t             danglingOffset_;
    unsigned int         sequenceDepth_;
    
    bool IsLittleEndian() const;
    
    void HandlePreamble(IVisitor& visitor,
                        const std::string& block);
    
    void HandleMetaHeader(IVisitor& visitor,
                          const std::string& block);

    void HandleDatasetTag(const std::string& block,
                          const DicomTag& untilTag);

    void HandleDatasetExplicitLength(uint32_t length);
    
    void HandleDatasetExplicitLength(IVisitor& visitor,
                                     const std::string& block);

    void HandleSequenceExplicitLength(const std::string& block);

    void HandleSequenceExplicitValue();
    
    void HandleDatasetValue(IVisitor& visitor,
                            const std::string& block);
    
  public:
    explicit DicomStreamReader(std::istream& stream);

    /**
     * Consume all the available bytes from the input stream, until
     * end-of-stream is reached or the current tag is ">= untilTag".
     * This method can be invoked several times, as more bytes are
     * available from the input stream. To check if the DICOM stream
     * is fully parsed until the goal tag, call "IsDone()".
     **/
    void Consume(IVisitor& visitor,
                 const DicomTag& untilTag);

    void Consume(IVisitor& visitor);

    bool IsDone() const;

    uint64_t GetProcessedBytes() const;

    static bool LookupPixelDataOffset(uint64_t& offset,
                                      const std::string& dicom);

    static bool LookupPixelDataOffset(uint64_t& offset,
                                      const void* buffer,
                                      size_t size);
  };
}
