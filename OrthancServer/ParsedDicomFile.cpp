/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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



/*=========================================================================

  This file is based on portions of the following project:

  Program: GDCM (Grassroots DICOM). A DICOM library
  Module:  http://gdcm.sourceforge.net/Copyright.html

Copyright (c) 2006-2011 Mathieu Malaterre
Copyright (c) 1993-2005 CREATIS
(CREATIS = Centre de Recherche et d'Applications en Traitement de l'Image)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither name of Mathieu Malaterre, or CREATIS, nor the names of any
   contributors (CNRS, INSERM, UCB, Universite Lyon I), may be used to
   endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/


#include "PrecompiledHeadersServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ParsedDicomFile.h"

#include "ServerToolbox.h"
#include "FromDcmtkBridge.h"
#include "ToDcmtkBridge.h"
#include "Internals/DicomImageDecoder.h"
#include "../Core/Logging.h"
#include "../Core/Toolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/ImageFormats/ImageBuffer.h"
#include "../Core/ImageFormats/PngWriter.h"
#include "../Core/Uuid.h"
#include "../Core/DicomFormat/DicomString.h"
#include "../Core/DicomFormat/DicomNullValue.h"
#include "../Core/DicomFormat/DicomIntegerPixelAccessor.h"
#include "../Core/ImageFormats/PngReader.h"

#include <list>
#include <limits>

#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcchrstr.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcmetinf.h>

#include <dcmtk/dcmdata/dcvrae.h>
#include <dcmtk/dcmdata/dcvras.h>
#include <dcmtk/dcmdata/dcvrcs.h>
#include <dcmtk/dcmdata/dcvrda.h>
#include <dcmtk/dcmdata/dcvrds.h>
#include <dcmtk/dcmdata/dcvrdt.h>
#include <dcmtk/dcmdata/dcvrfd.h>
#include <dcmtk/dcmdata/dcvrfl.h>
#include <dcmtk/dcmdata/dcvris.h>
#include <dcmtk/dcmdata/dcvrlo.h>
#include <dcmtk/dcmdata/dcvrlt.h>
#include <dcmtk/dcmdata/dcvrpn.h>
#include <dcmtk/dcmdata/dcvrsh.h>
#include <dcmtk/dcmdata/dcvrsl.h>
#include <dcmtk/dcmdata/dcvrss.h>
#include <dcmtk/dcmdata/dcvrst.h>
#include <dcmtk/dcmdata/dcvrtm.h>
#include <dcmtk/dcmdata/dcvrui.h>
#include <dcmtk/dcmdata/dcvrul.h>
#include <dcmtk/dcmdata/dcvrus.h>
#include <dcmtk/dcmdata/dcvrut.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>


#include <boost/math/special_functions/round.hpp>
#include <dcmtk/dcmdata/dcostrmb.h>


static const char* CONTENT_TYPE_OCTET_STREAM = "application/octet-stream";



namespace Orthanc
{
  struct ParsedDicomFile::PImpl
  {
    std::auto_ptr<DcmFileFormat> file_;
    Encoding encoding_;
  };


  // This method can only be called from the constructors!
  void ParsedDicomFile::Setup(const char* buffer, size_t size)
  {
    DcmInputBufferStream is;
    if (size > 0)
    {
      is.setBuffer(buffer, size);
    }
    is.setEos();

    pimpl_->file_.reset(new DcmFileFormat);
    pimpl_->file_->transferInit();
    if (!pimpl_->file_->read(is).good())
    {
      delete pimpl_;  // Avoid a memory leak due to exception
                      // throwing, as we are in the constructor

      throw OrthancException(ErrorCode_BadFileFormat);
    }
    pimpl_->file_->loadAllDataIntoMemory();
    pimpl_->file_->transferEnd();

    pimpl_->encoding_ = FromDcmtkBridge::DetectEncoding(*pimpl_->file_->getDataset());
  }


  static void SendPathValueForDictionary(RestApiOutput& output,
                                         DcmItem& dicom)
  {
    Json::Value v = Json::arrayValue;

    for (unsigned long i = 0; i < dicom.card(); i++)
    {
      DcmElement* element = dicom.getElement(i);
      if (element)
      {
        char buf[16];
        sprintf(buf, "%04x-%04x", element->getTag().getGTag(), element->getTag().getETag());
        v.append(buf);
      }
    }

    output.AnswerJson(v);
  }

  static inline uint16_t GetCharValue(char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    else if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    else
      return 0;
  }

  static inline uint16_t GetTagValue(const char* c)
  {
    return ((GetCharValue(c[0]) << 12) + 
            (GetCharValue(c[1]) << 8) + 
            (GetCharValue(c[2]) << 4) + 
            GetCharValue(c[3]));
  }

  static void ParseTagAndGroup(DcmTagKey& key,
                               const std::string& tag)
  {
    DicomTag t = FromDcmtkBridge::ParseTag(tag);
    key = DcmTagKey(t.GetGroup(), t.GetElement());
  }


  static void SendSequence(RestApiOutput& output,
                           DcmSequenceOfItems& sequence)
  {
    // This element is a sequence
    Json::Value v = Json::arrayValue;

    for (unsigned long i = 0; i < sequence.card(); i++)
    {
      v.append(boost::lexical_cast<std::string>(i));
    }

    output.AnswerJson(v);
  }


  static unsigned int GetPixelDataBlockCount(DcmPixelData& pixelData,
                                             E_TransferSyntax transferSyntax)
  {
    DcmPixelSequence* pixelSequence = NULL;
    if (pixelData.getEncapsulatedRepresentation
        (transferSyntax, NULL, pixelSequence).good() && pixelSequence)
    {
      return pixelSequence->card();
    }
    else
    {
      return 1;
    }
  }


  static void AnswerDicomField(RestApiOutput& output,
                               DcmElement& element,
                               E_TransferSyntax transferSyntax)
  {
    // This element is nor a sequence, neither a pixel-data
    std::string buffer;
    buffer.resize(65536);
    Uint32 length = element.getLength(transferSyntax);
    Uint32 offset = 0;

    output.GetLowLevelOutput().SetContentType(CONTENT_TYPE_OCTET_STREAM);
    output.GetLowLevelOutput().SetContentLength(length);

    while (offset < length)
    {
      Uint32 nbytes;
      if (length - offset < buffer.size())
      {
        nbytes = length - offset;
      }
      else
      {
        nbytes = buffer.size();
      }

      OFCondition cond = element.getPartialValue(&buffer[0], offset, nbytes);

      if (cond.good())
      {
        output.GetLowLevelOutput().SendBody(&buffer[0], nbytes);
        offset += nbytes;
      }
      else
      {
        LOG(ERROR) << "Error while sending a DICOM field: " << cond.text();
        return;
      }
    }

    output.MarkLowLevelOutputDone();
  }


  static bool AnswerPixelData(RestApiOutput& output,
                              DcmItem& dicom,
                              E_TransferSyntax transferSyntax,
                              const std::string* blockUri)
  {
    DcmTag k(DICOM_TAG_PIXEL_DATA.GetGroup(),
             DICOM_TAG_PIXEL_DATA.GetElement());

    DcmElement *element = NULL;
    if (!dicom.findAndGetElement(k, element).good() ||
        element == NULL)
    {
      return false;
    }

    try
    {
      DcmPixelData& pixelData = dynamic_cast<DcmPixelData&>(*element);
      if (blockUri == NULL)
      {
        // The user asks how many blocks are presents in this pixel data
        unsigned int blocks = GetPixelDataBlockCount(pixelData, transferSyntax);

        Json::Value result(Json::arrayValue);
        for (unsigned int i = 0; i < blocks; i++)
        {
          result.append(boost::lexical_cast<std::string>(i));
        }
        
        output.AnswerJson(result);
        return true;
      }


      unsigned int block = boost::lexical_cast<unsigned int>(*blockUri);

      if (block < GetPixelDataBlockCount(pixelData, transferSyntax))
      {
        DcmPixelSequence* pixelSequence = NULL;
        if (pixelData.getEncapsulatedRepresentation
            (transferSyntax, NULL, pixelSequence).good() && pixelSequence)
        {
          // This is the case for JPEG transfer syntaxes
          if (block < pixelSequence->card())
          {
            DcmPixelItem* pixelItem = NULL;
            if (pixelSequence->getItem(pixelItem, block).good() && pixelItem)
            {
              if (pixelItem->getLength() == 0)
              {
                output.AnswerBuffer(NULL, 0, CONTENT_TYPE_OCTET_STREAM);
                return true;
              }

              Uint8* buffer = NULL;
              if (pixelItem->getUint8Array(buffer).good() && buffer)
              {
                output.AnswerBuffer(buffer, pixelItem->getLength(), CONTENT_TYPE_OCTET_STREAM);
                return true;
              }
            }
          }
        }
        else
        {
          // This is the case for raw, uncompressed image buffers
          assert(*blockUri == "0");
          AnswerDicomField(output, *element, transferSyntax);
        }
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      // The URI entered by the user is not a number
    }
    catch (std::bad_cast&)
    {
      // This should never happen
    }

    return false;
  }



  static void SendPathValueForLeaf(RestApiOutput& output,
                                   const std::string& tag,
                                   DcmItem& dicom,
                                   E_TransferSyntax transferSyntax)
  {
    DcmTagKey k;
    ParseTagAndGroup(k, tag);

    DcmSequenceOfItems* sequence = NULL;
    if (dicom.findAndGetSequence(k, sequence).good() && 
        sequence != NULL &&
        sequence->getVR() == EVR_SQ)
    {
      SendSequence(output, *sequence);
      return;
    }

    DcmElement* element = NULL;
    if (dicom.findAndGetElement(k, element).good() && 
        element != NULL &&
        //element->getVR() != EVR_UNKNOWN &&  // This would forbid private tags
        element->getVR() != EVR_SQ)
    {
      AnswerDicomField(output, *element, transferSyntax);
    }
  }

  void ParsedDicomFile::SendPathValue(RestApiOutput& output,
                                      const UriComponents& uri)
  {
    DcmItem* dicom = pimpl_->file_->getDataset();
    E_TransferSyntax transferSyntax = pimpl_->file_->getDataset()->getOriginalXfer();

    // Special case: Accessing the pixel data
    if (uri.size() == 1 || 
        uri.size() == 2)
    {
      DcmTagKey tag;
      ParseTagAndGroup(tag, uri[0]);

      if (tag.getGroup() == DICOM_TAG_PIXEL_DATA.GetGroup() &&
          tag.getElement() == DICOM_TAG_PIXEL_DATA.GetElement())
      {
        AnswerPixelData(output, *dicom, transferSyntax, uri.size() == 1 ? NULL : &uri[1]);
        return;
      }
    }        

    // Go down in the tag hierarchy according to the URI
    for (size_t pos = 0; pos < uri.size() / 2; pos++)
    {
      size_t index;
      try
      {
        index = boost::lexical_cast<size_t>(uri[2 * pos + 1]);
      }
      catch (boost::bad_lexical_cast&)
      {
        return;
      }

      DcmTagKey k;
      DcmItem *child = NULL;
      ParseTagAndGroup(k, uri[2 * pos]);
      if (!dicom->findAndGetSequenceItem(k, child, index).good() ||
          child == NULL)
      {
        return;
      }

      dicom = child;
    }

    // We have reached the end of the URI
    if (uri.size() % 2 == 0)
    {
      SendPathValueForDictionary(output, *dicom);
    }
    else
    {
      SendPathValueForLeaf(output, uri.back(), *dicom, transferSyntax);
    }
  }


  


  static DcmElement* CreateElementForTag(const DicomTag& tag)
  {
    DcmTag key(tag.GetGroup(), tag.GetElement());

    switch (key.getEVR())
    {
      // http://support.dcmtk.org/docs/dcvr_8h-source.html

      /**
       * TODO.
       **/
    
      case EVR_OB:  // other byte
      case EVR_OF:  // other float
      case EVR_OW:  // other word
      case EVR_AT:  // attribute tag
        throw OrthancException(ErrorCode_NotImplemented);

      case EVR_UN:  // unknown value representation
        throw OrthancException(ErrorCode_ParameterOutOfRange);


      /**
       * String types.
       * http://support.dcmtk.org/docs/classDcmByteString.html
       **/
      
      case EVR_AS:  // age string
        return new DcmAgeString(key);

      case EVR_AE:  // application entity title
        return new DcmApplicationEntity(key);

      case EVR_CS:  // code string
        return new DcmCodeString(key);        

      case EVR_DA:  // date string
        return new DcmDate(key);
        
      case EVR_DT:  // date time string
        return new DcmDateTime(key);

      case EVR_DS:  // decimal string
        return new DcmDecimalString(key);

      case EVR_IS:  // integer string
        return new DcmIntegerString(key);

      case EVR_TM:  // time string
        return new DcmTime(key);

      case EVR_UI:  // unique identifier
        return new DcmUniqueIdentifier(key);

      case EVR_ST:  // short text
        return new DcmShortText(key);

      case EVR_LO:  // long string
        return new DcmLongString(key);

      case EVR_LT:  // long text
        return new DcmLongText(key);

      case EVR_UT:  // unlimited text
        return new DcmUnlimitedText(key);

      case EVR_SH:  // short string
        return new DcmShortString(key);

      case EVR_PN:  // person name
        return new DcmPersonName(key);

        
      /**
       * Numerical types
       **/ 
      
      case EVR_SL:  // signed long
        return new DcmSignedLong(key);

      case EVR_SS:  // signed short
        return new DcmSignedShort(key);

      case EVR_UL:  // unsigned long
        return new DcmUnsignedLong(key);

      case EVR_US:  // unsigned short
        return new DcmUnsignedShort(key);

      case EVR_FL:  // float single-precision
        return new DcmFloatingPointSingle(key);

      case EVR_FD:  // float double-precision
        return new DcmFloatingPointDouble(key);


      /**
       * Sequence types, should never occur at this point.
       **/

      case EVR_SQ:  // sequence of items
        throw OrthancException(ErrorCode_ParameterOutOfRange);


      /**
       * Internal to DCMTK.
       **/ 

      case EVR_ox:  // OB or OW depending on context
      case EVR_xs:  // SS or US depending on context
      case EVR_lt:  // US, SS or OW depending on context, used for LUT Data (thus the name)
      case EVR_na:  // na="not applicable", for data which has no VR
      case EVR_up:  // up="unsigned pointer", used internally for DICOMDIR suppor
      case EVR_item:  // used internally for items
      case EVR_metainfo:  // used internally for meta info datasets
      case EVR_dataset:  // used internally for datasets
      case EVR_fileFormat:  // used internally for DICOM files
      case EVR_dicomDir:  // used internally for DICOMDIR objects
      case EVR_dirRecord:  // used internally for DICOMDIR records
      case EVR_pixelSQ:  // used internally for pixel sequences in a compressed image
      case EVR_pixelItem:  // used internally for pixel items in a compressed image
      case EVR_UNKNOWN: // used internally for elements with unknown VR (encoded with 4-byte length field in explicit VR)
      case EVR_PixelData:  // used internally for uncompressed pixeld data
      case EVR_OverlayData:  // used internally for overlay data
      case EVR_UNKNOWN2B:  // used internally for elements with unknown VR with 2-byte length field in explicit VR
      default:
        break;
    }

    throw OrthancException(ErrorCode_InternalError);          
  }



  static void FillElementWithString(DcmElement& element,
                                    const DicomTag& tag,
                                    const std::string& value)
  {
    DcmTag key(tag.GetGroup(), tag.GetElement());
    bool ok = false;
    
    try
    {
      switch (key.getEVR())
      {
        // http://support.dcmtk.org/docs/dcvr_8h-source.html

        /**
         * TODO.
         **/

        case EVR_OB:  // other byte
        case EVR_OF:  // other float
        case EVR_OW:  // other word
        case EVR_AT:  // attribute tag
          throw OrthancException(ErrorCode_NotImplemented);
    
        case EVR_UN:  // unknown value representation
          throw OrthancException(ErrorCode_ParameterOutOfRange);


        /**
         * String types.
         **/
      
        case EVR_DS:  // decimal string
        case EVR_IS:  // integer string
        case EVR_AS:  // age string
        case EVR_DA:  // date string
        case EVR_DT:  // date time string
        case EVR_TM:  // time string
        case EVR_AE:  // application entity title
        case EVR_CS:  // code string
        case EVR_SH:  // short string
        case EVR_LO:  // long string
        case EVR_ST:  // short text
        case EVR_LT:  // long text
        case EVR_UT:  // unlimited text
        case EVR_PN:  // person name
        case EVR_UI:  // unique identifier
        {
          ok = element.putString(value.c_str()).good();
          break;
        }

        
        /**
         * Numerical types
         **/ 
      
        case EVR_SL:  // signed long
        {
          ok = element.putSint32(boost::lexical_cast<Sint32>(value)).good();
          break;
        }

        case EVR_SS:  // signed short
        {
          ok = element.putSint16(boost::lexical_cast<Sint16>(value)).good();
          break;
        }

        case EVR_UL:  // unsigned long
        {
          ok = element.putUint32(boost::lexical_cast<Uint32>(value)).good();
          break;
        }

        case EVR_US:  // unsigned short
        {
          ok = element.putUint16(boost::lexical_cast<Uint16>(value)).good();
          break;
        }

        case EVR_FL:  // float single-precision
        {
          ok = element.putFloat32(boost::lexical_cast<float>(value)).good();
          break;
        }

        case EVR_FD:  // float double-precision
        {
          ok = element.putFloat64(boost::lexical_cast<double>(value)).good();
          break;
        }


        /**
         * Sequence types, should never occur at this point.
         **/

        case EVR_SQ:  // sequence of items
        {
          ok = false;
          break;
        }


        /**
         * Internal to DCMTK.
         **/ 

        case EVR_ox:  // OB or OW depending on context
        case EVR_xs:  // SS or US depending on context
        case EVR_lt:  // US, SS or OW depending on context, used for LUT Data (thus the name)
        case EVR_na:  // na="not applicable", for data which has no VR
        case EVR_up:  // up="unsigned pointer", used internally for DICOMDIR suppor
        case EVR_item:  // used internally for items
        case EVR_metainfo:  // used internally for meta info datasets
        case EVR_dataset:  // used internally for datasets
        case EVR_fileFormat:  // used internally for DICOM files
        case EVR_dicomDir:  // used internally for DICOMDIR objects
        case EVR_dirRecord:  // used internally for DICOMDIR records
        case EVR_pixelSQ:  // used internally for pixel sequences in a compressed image
        case EVR_pixelItem:  // used internally for pixel items in a compressed image
        case EVR_UNKNOWN: // used internally for elements with unknown VR (encoded with 4-byte length field in explicit VR)
        case EVR_PixelData:  // used internally for uncompressed pixeld data
        case EVR_OverlayData:  // used internally for overlay data
        case EVR_UNKNOWN2B:  // used internally for elements with unknown VR with 2-byte length field in explicit VR
        default:
          break;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      ok = false;
    }

    if (!ok)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  void ParsedDicomFile::Remove(const DicomTag& tag)
  {
    DcmTagKey key(tag.GetGroup(), tag.GetElement());
    DcmElement* element = pimpl_->file_->getDataset()->remove(key);
    if (element != NULL)
    {
      delete element;
    }
  }



  void ParsedDicomFile::RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep)
  {
    DcmDataset& dataset = *pimpl_->file_->getDataset();

    // Loop over the dataset to detect its private tags
    typedef std::list<DcmElement*> Tags;
    Tags privateTags;

    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      DcmTag tag(element->getTag());

      // Is this a private tag?
      if (FromDcmtkBridge::IsPrivateTag(tag))
      {
        bool remove = true;

        // Check whether this private tag is to be kept
        if (toKeep != NULL)
        {
          DicomTag tmp = FromDcmtkBridge::Convert(tag);
          if (toKeep->find(tmp) != toKeep->end())
          {
            remove = false;  // Keep it
          }
        }
            
        if (remove)
        {
          privateTags.push_back(element);
        }
      }
    }

    // Loop over the detected private tags to remove them
    for (Tags::iterator it = privateTags.begin(); 
         it != privateTags.end(); ++it)
    {
      DcmElement* tmp = dataset.remove(*it);
      if (tmp != NULL)
      {
        delete tmp;
      }
    }
  }




  void ParsedDicomFile::Insert(const DicomTag& tag,
                               const std::string& value)
  {
    OFCondition cond;

    if (FromDcmtkBridge::IsPrivateTag(tag))
    {
      // This is a private tag
      // http://support.dcmtk.org/redmine/projects/dcmtk/wiki/howto_addprivatedata

      DcmTag key(tag.GetGroup(), tag.GetElement(), EVR_OB);
      cond = pimpl_->file_->getDataset()->putAndInsertUint8Array
        (key, (const Uint8*) value.c_str(), value.size(), false);
    }
    else
    {
      std::auto_ptr<DcmElement> element(CreateElementForTag(tag));
      FillElementWithString(*element, tag, value);

      cond = pimpl_->file_->getDataset()->insert(element.release(), false, false);
    }

    if (!cond.good())
    {
      // This field already exists
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  void ParsedDicomFile::Replace(const DicomTag& tag,
                                const std::string& value,
                                DicomReplaceMode mode)
  {
    DcmTagKey key(tag.GetGroup(), tag.GetElement());
    DcmElement* element = NULL;

    if (!pimpl_->file_->getDataset()->findAndGetElement(key, element).good() ||
        element == NULL)
    {
      // This field does not exist, act wrt. the specified "mode"
      switch (mode)
      {
        case DicomReplaceMode_InsertIfAbsent:
          Insert(tag, value);
          break;

        case DicomReplaceMode_ThrowIfAbsent:
          throw OrthancException(ErrorCode_InexistentItem);

        case DicomReplaceMode_IgnoreIfAbsent:
          return;
      }
    }
    else
    {
      if (FromDcmtkBridge::IsPrivateTag(tag))
      {
        if (!element->putUint8Array((const Uint8*) value.c_str(), value.size()).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        FillElementWithString(*element, tag, value);
      }
    }


    /**
     * dcmodify will automatically correct 'Media Storage SOP Class
     * UID' and 'Media Storage SOP Instance UID' in the metaheader, if
     * you make changes to the related tags in the dataset ('SOP Class
     * UID' and 'SOP Instance UID') via insert or modify mode
     * options. You can disable this behaviour by using the -nmu
     * option.
     **/

    if (tag == DICOM_TAG_SOP_CLASS_UID)
    {
      Replace(DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID, value, DicomReplaceMode_InsertIfAbsent);
    }

    if (tag == DICOM_TAG_SOP_INSTANCE_UID)
    {
      Replace(DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID, value, DicomReplaceMode_InsertIfAbsent);
    }
  }

    
  void ParsedDicomFile::Answer(RestApiOutput& output)
  {
    std::string serialized;
    if (FromDcmtkBridge::SaveToMemoryBuffer(serialized, *pimpl_->file_->getDataset()))
    {
      output.AnswerBuffer(serialized, CONTENT_TYPE_OCTET_STREAM);
    }
  }



  bool ParsedDicomFile::GetTagValue(std::string& value,
                                    const DicomTag& tag)
  {
    DcmTagKey k(tag.GetGroup(), tag.GetElement());
    DcmDataset& dataset = *pimpl_->file_->getDataset();

    if (FromDcmtkBridge::IsPrivateTag(tag))
    {
      const Uint8* data = NULL;   // This is freed in the destructor of the dataset
      long unsigned int count = 0;

      if (dataset.findAndGetUint8Array(k, data, &count).good())
      {
        if (count > 0)
        {
          assert(data != NULL);
          value.assign(reinterpret_cast<const char*>(data), count);
        }
        else
        {
          value.clear();
        }

        return true;
      }
      else
      {
        return false;
      }
    }
    else
    {
      DcmElement* element = NULL;
      if (!dataset.findAndGetElement(k, element).good() ||
          element == NULL)
      {
        return false;
      }

      std::auto_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement(*element, pimpl_->encoding_));

      if (v.get() == NULL)
      {
        value = "";
      }
      else
      {
        value = v->AsString();
      }

      return true;
    }
  }


  DicomInstanceHasher ParsedDicomFile::GetHasher()
  {
    std::string patientId, studyUid, seriesUid, instanceUid;

    if (!GetTagValue(patientId, DICOM_TAG_PATIENT_ID) ||
        !GetTagValue(studyUid, DICOM_TAG_STUDY_INSTANCE_UID) ||
        !GetTagValue(seriesUid, DICOM_TAG_SERIES_INSTANCE_UID) ||
        !GetTagValue(instanceUid, DICOM_TAG_SOP_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    return DicomInstanceHasher(patientId, studyUid, seriesUid, instanceUid);
  }


  template <typename T>
  static void ExtractPngImageTruncate(std::string& result,
                                      DicomIntegerPixelAccessor& accessor,
                                      PixelFormat format)
  {
    assert(accessor.GetInformation().GetChannelCount() == 1);

    PngWriter w;

    std::vector<T> image(accessor.GetInformation().GetWidth() * accessor.GetInformation().GetHeight(), 0);
    T* pixel = &image[0];
    for (unsigned int y = 0; y < accessor.GetInformation().GetHeight(); y++)
    {
      for (unsigned int x = 0; x < accessor.GetInformation().GetWidth(); x++, pixel++)
      {
        int32_t v = accessor.GetValue(x, y);
        if (v < static_cast<int32_t>(std::numeric_limits<T>::min()))
          *pixel = std::numeric_limits<T>::min();
        else if (v > static_cast<int32_t>(std::numeric_limits<T>::max()))
          *pixel = std::numeric_limits<T>::max();
        else
          *pixel = static_cast<T>(v);
      }
    }

    w.WriteToMemory(result, accessor.GetInformation().GetWidth(), accessor.GetInformation().GetHeight(),
                    accessor.GetInformation().GetWidth() * sizeof(T), format, &image[0]);
  }


  void ParsedDicomFile::SaveToMemoryBuffer(std::string& buffer)
  {
    FromDcmtkBridge::SaveToMemoryBuffer(buffer, *pimpl_->file_->getDataset());
  }


  void ParsedDicomFile::SaveToFile(const std::string& path)
  {
    // TODO Avoid using a temporary memory buffer, write directly on disk
    std::string content;
    SaveToMemoryBuffer(content);
    Toolbox::WriteFile(content, path);
  }


  ParsedDicomFile::ParsedDicomFile() : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat);
    pimpl_->encoding_ = Encoding_Ascii;
    Replace(DICOM_TAG_PATIENT_ID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Patient));
    Replace(DICOM_TAG_STUDY_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study));
    Replace(DICOM_TAG_SERIES_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series));
    Replace(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));
  }


  ParsedDicomFile::ParsedDicomFile(const char* content, size_t size) : pimpl_(new PImpl)
  {
    Setup(content, size);
  }

  ParsedDicomFile::ParsedDicomFile(const std::string& content) : pimpl_(new PImpl)
  {
    if (content.size() == 0)
    {
      Setup(NULL, 0);
    }
    else
    {
      Setup(&content[0], content.size());
    }
  }


  ParsedDicomFile::ParsedDicomFile(ParsedDicomFile& other) : 
    pimpl_(new PImpl)
  {
    pimpl_->file_.reset(dynamic_cast<DcmFileFormat*>(other.pimpl_->file_->clone()));

    pimpl_->encoding_ = other.pimpl_->encoding_;
  }


  ParsedDicomFile::~ParsedDicomFile()
  {
    delete pimpl_;
  }


  void* ParsedDicomFile::GetDcmtkObject()
  {
    return pimpl_->file_.get();
  }


  ParsedDicomFile* ParsedDicomFile::Clone()
  {
    return new ParsedDicomFile(*this);
  }


  void ParsedDicomFile::EmbedImage(const std::string& dataUriScheme)
  {
    std::string mime, content;
    Toolbox::DecodeDataUriScheme(mime, content, dataUriScheme);

    std::string decoded;
    Toolbox::DecodeBase64(decoded, content);

    if (mime == "image/png")
    {
      PngReader reader;
      reader.ReadFromMemory(decoded);
      EmbedImage(reader);
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ParsedDicomFile::EmbedImage(const ImageAccessor& accessor)
  {
    if (accessor.GetFormat() != PixelFormat_Grayscale8 &&
        accessor.GetFormat() != PixelFormat_Grayscale16 &&
        accessor.GetFormat() != PixelFormat_RGB24 &&
        accessor.GetFormat() != PixelFormat_RGBA32)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (accessor.GetFormat() == PixelFormat_RGBA32)
    {
      LOG(WARNING) << "Getting rid of the alpha channel when embedding a RGBA image inside DICOM";
    }

    // http://dicomiseasy.blogspot.be/2012/08/chapter-12-pixel-data.html

    Remove(DICOM_TAG_PIXEL_DATA);
    Replace(DICOM_TAG_COLUMNS, boost::lexical_cast<std::string>(accessor.GetWidth()));
    Replace(DICOM_TAG_ROWS, boost::lexical_cast<std::string>(accessor.GetHeight()));
    Replace(DICOM_TAG_SAMPLES_PER_PIXEL, "1");
    Replace(DICOM_TAG_NUMBER_OF_FRAMES, "1");
    Replace(DICOM_TAG_PIXEL_REPRESENTATION, "0");  // Unsigned pixels
    Replace(DICOM_TAG_PLANAR_CONFIGURATION, "0");  // Color channels are interleaved
    Replace(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2");
    Replace(DICOM_TAG_BITS_ALLOCATED, "8");
    Replace(DICOM_TAG_BITS_STORED, "8");
    Replace(DICOM_TAG_HIGH_BIT, "7");

    unsigned int bytesPerPixel = 1;

    switch (accessor.GetFormat())
    {
      case PixelFormat_RGB24:
      case PixelFormat_RGBA32:
        Replace(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "RGB");
        Replace(DICOM_TAG_SAMPLES_PER_PIXEL, "3");
        bytesPerPixel = 3;
        break;

      case PixelFormat_Grayscale8:
        break;

      case PixelFormat_Grayscale16:
        Replace(DICOM_TAG_BITS_ALLOCATED, "16");
        Replace(DICOM_TAG_BITS_STORED, "16");
        Replace(DICOM_TAG_HIGH_BIT, "15");
        bytesPerPixel = 2;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    DcmTag key(DICOM_TAG_PIXEL_DATA.GetGroup(), 
               DICOM_TAG_PIXEL_DATA.GetElement());

    std::auto_ptr<DcmPixelData> pixels(new DcmPixelData(key));

    unsigned int pitch = accessor.GetWidth() * bytesPerPixel;
    Uint8* target = NULL;
    pixels->createUint8Array(accessor.GetHeight() * pitch, target);

    for (unsigned int y = 0; y < accessor.GetHeight(); y++)
    {
      switch (accessor.GetFormat())
      {
        case PixelFormat_RGB24:
        case PixelFormat_Grayscale8:
        case PixelFormat_Grayscale16:
        case PixelFormat_SignedGrayscale16:
        {
          memcpy(target, reinterpret_cast<const Uint8*>(accessor.GetConstRow(y)), pitch);
          target += pitch;
          break;
        }

        case PixelFormat_RGBA32:
        {
          // The alpha channel is not supported by the DICOM standard
          const Uint8* source = reinterpret_cast<const Uint8*>(accessor.GetConstRow(y));
          for (unsigned int x = 0; x < accessor.GetWidth(); x++, target += 3, source += 4)
          {
            target[0] = source[0];
            target[1] = source[1];
            target[2] = source[2];
          }

          break;
        }
          
        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }

    if (!pimpl_->file_->getDataset()->insert(pixels.release(), false, false).good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }    
  }

  
  void ParsedDicomFile::ExtractImage(ImageBuffer& result,
                                     unsigned int frame)
  {
    DcmDataset& dataset = *pimpl_->file_->getDataset();

    if (!DicomImageDecoder::Decode(result, dataset, frame))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  void ParsedDicomFile::ExtractImage(ImageBuffer& result,
                                     unsigned int frame,
                                     ImageExtractionMode mode)
  {
    DcmDataset& dataset = *pimpl_->file_->getDataset();

    bool ok = false;

    switch (mode)
    {
      case ImageExtractionMode_UInt8:
        ok = DicomImageDecoder::DecodeAndTruncate(result, dataset, frame, PixelFormat_Grayscale8, false);
        break;

      case ImageExtractionMode_UInt16:
        ok = DicomImageDecoder::DecodeAndTruncate(result, dataset, frame, PixelFormat_Grayscale16, false);
        break;

      case ImageExtractionMode_Int16:
        ok = DicomImageDecoder::DecodeAndTruncate(result, dataset, frame, PixelFormat_SignedGrayscale16, false);
        break;

      case ImageExtractionMode_Preview:
        ok = DicomImageDecoder::DecodePreview(result, dataset, frame);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (!ok)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  void ParsedDicomFile::ExtractPngImage(std::string& result,
                                        unsigned int frame,
                                        ImageExtractionMode mode)
  {
    ImageBuffer buffer;
    ExtractImage(buffer, frame, mode);

    ImageAccessor accessor(buffer.GetConstAccessor());
    PngWriter writer;
    writer.WriteToMemory(result, accessor);
  }


  Encoding ParsedDicomFile::GetEncoding() const
  {
    return pimpl_->encoding_;
  }


  void ParsedDicomFile::SetEncoding(Encoding encoding)
  {
    std::string s;

    // http://www.dabsoft.ch/dicom/3/C.12.1.1.2/
    switch (encoding)
    {
      case Encoding_Utf8:
      case Encoding_Ascii:
        s = "ISO_IR 192";
        break;

      case Encoding_Windows1251:
        // This Cyrillic codepage is not officially supported by the
        // DICOM standard. Do not set the SpecificCharacterSet tag.
        return;

      case Encoding_Latin1:
        s = "ISO_IR 100";
        break;

      case Encoding_Latin2:
        s = "ISO_IR 101";
        break;

      case Encoding_Latin3:
        s = "ISO_IR 109";
        break;

      case Encoding_Latin4:
        s = "ISO_IR 110";
        break;

      case Encoding_Latin5:
        s = "ISO_IR 148";
        break;

      case Encoding_Cyrillic:
        s = "ISO_IR 144";
        break;

      case Encoding_Arabic:
        s = "ISO_IR 127";
        break;

      case Encoding_Greek:
        s = "ISO_IR 126";
        break;

      case Encoding_Hebrew:
        s = "ISO_IR 138";
        break;

      case Encoding_Japanese:
        s = "ISO_IR 13";
        break;

      case Encoding_Chinese:
        s = "GB18030";
        break;

      case Encoding_Thai:
        s = "ISO_IR 166";
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    Replace(DICOM_TAG_SPECIFIC_CHARACTER_SET, s, DicomReplaceMode_InsertIfAbsent);
  }

  void ParsedDicomFile::ToJson(Json::Value& target, bool simplify)
  {
    if (simplify)
    {
      Json::Value tmp;
      FromDcmtkBridge::ToJson(tmp, *pimpl_->file_->getDataset());
      SimplifyTags(target, tmp);
    }
    else
    {
      FromDcmtkBridge::ToJson(target, *pimpl_->file_->getDataset());
    }
  }
}
