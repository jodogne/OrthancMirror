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


#include "../PrecompiledHeaders.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ParsedDicomFile.h"

#include "FromDcmtkBridge.h"
#include "ToDcmtkBridge.h"
#include "Internals/DicomFrameIndex.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"
#include "../SystemToolbox.h"

#if ORTHANC_ENABLE_JPEG == 1
#  include "../Images/JpegReader.h"
#endif

#if ORTHANC_ENABLE_PNG == 1
#  include "../Images/PngReader.h"
#endif

#include <list>
#include <limits>

#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcchrstr.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcdeftag.h>

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
#include <boost/algorithm/string/predicate.hpp>


#if DCMTK_VERSION_NUMBER <= 360
#  define EXS_JPEGProcess1      EXS_JPEGProcess1TransferSyntax
#endif



namespace Orthanc
{
  struct ParsedDicomFile::PImpl
  {
    std::auto_ptr<DcmFileFormat> file_;
    std::auto_ptr<DicomFrameIndex>  frameIndex_;
  };


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  static const char* CONTENT_TYPE_OCTET_STREAM = "application/octet-stream";

  static void ParseTagAndGroup(DcmTagKey& key,
                               const std::string& tag)
  {
    DicomTag t = FromDcmtkBridge::ParseTag(tag);
    key = DcmTagKey(t.GetGroup(), t.GetElement());
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


  namespace
  {
    class DicomFieldStream : public IHttpStreamAnswer
    {
    private:
      DcmElement&  element_;
      uint32_t     length_;
      uint32_t     offset_;
      std::string  chunk_;
      size_t       chunkSize_;
      
    public:
      DicomFieldStream(DcmElement& element,
                       E_TransferSyntax transferSyntax) :
        element_(element),
        length_(element.getLength(transferSyntax)),
        offset_(0),
        chunkSize_(0)
      {
        static const size_t CHUNK_SIZE = 64 * 1024;  // Use chunks of max 64KB
        chunk_.resize(CHUNK_SIZE);
      }

      virtual HttpCompression SetupHttpCompression(bool /*gzipAllowed*/,
                                                   bool /*deflateAllowed*/)
      {
        // No support for compression
        return HttpCompression_None;
      }

      virtual bool HasContentFilename(std::string& filename)
      {
        return false;
      }

      virtual std::string GetContentType()
      {
        return "";
      }

      virtual uint64_t  GetContentLength()
      {
        return length_;
      }
 
      virtual bool ReadNextChunk()
      {
        assert(offset_ <= length_);

        if (offset_ == length_)
        {
          return false;
        }
        else
        {
          if (length_ - offset_ < chunk_.size())
          {
            chunkSize_ = length_ - offset_;
          }
          else
          {
            chunkSize_ = chunk_.size();
          }

          OFCondition cond = element_.getPartialValue(&chunk_[0], offset_, chunkSize_);

          offset_ += chunkSize_;

          if (!cond.good())
          {
            LOG(ERROR) << "Error while sending a DICOM field: " << cond.text();
            throw OrthancException(ErrorCode_InternalError);
          }

          return true;
        }
      }
 
      virtual const char *GetChunkContent()
      {
        return chunk_.c_str();
      }
 
      virtual size_t GetChunkSize()
      {
        return chunkSize_;
      }
    };
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
        // The user asks how many blocks are present in this pixel data
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
          DicomFieldStream stream(*element, transferSyntax);
          output.AnswerStream(stream);
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
      DicomFieldStream stream(*element, transferSyntax);
      output.AnswerStream(stream);
    }
  }
#endif

  
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


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
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
#endif
  

  void ParsedDicomFile::Remove(const DicomTag& tag)
  {
    InvalidateCache();

    DcmTagKey key(tag.GetGroup(), tag.GetElement());
    DcmElement* element = pimpl_->file_->getDataset()->remove(key);
    if (element != NULL)
    {
      delete element;
    }
  }


  void ParsedDicomFile::Clear(const DicomTag& tag,
                              bool onlyIfExists)
  {
    InvalidateCache();

    DcmItem* dicom = pimpl_->file_->getDataset();
    DcmTagKey key(tag.GetGroup(), tag.GetElement());

    if (onlyIfExists &&
        !dicom->tagExists(key))
    {
      // The tag is non-existing, do not clear it
    }
    else
    {
      if (!dicom->insertEmptyElement(key, OFTrue /* replace old value */).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  void ParsedDicomFile::RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep)
  {
    InvalidateCache();

    DcmDataset& dataset = *pimpl_->file_->getDataset();

    // Loop over the dataset to detect its private tags
    typedef std::list<DcmElement*> Tags;
    Tags privateTags;

    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      DcmTag tag(element->getTag());

      // Is this a private tag?
      if (tag.isPrivate())
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


  static void InsertInternal(DcmDataset& dicom,
                             DcmElement* element)
  {
    OFCondition cond = dicom.insert(element, false, false);
    if (!cond.good())
    {
      // This field already exists
      delete element;
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  void ParsedDicomFile::Insert(const DicomTag& tag,
                               const Json::Value& value,
                               bool decodeDataUriScheme)
  {
    if (pimpl_->file_->getDataset()->tagExists(ToDcmtkBridge::Convert(tag)))
    {
      throw OrthancException(ErrorCode_AlreadyExistingTag);
    }

    if (decodeDataUriScheme &&
        value.type() == Json::stringValue &&
        (tag == DICOM_TAG_ENCAPSULATED_DOCUMENT ||
         tag == DICOM_TAG_PIXEL_DATA))
    {
      if (EmbedContentInternal(value.asString()))
      {
        return;
      }
    }

    InvalidateCache();
    std::auto_ptr<DcmElement> element(FromDcmtkBridge::FromJson(tag, value, decodeDataUriScheme, GetEncoding()));
    InsertInternal(*pimpl_->file_->getDataset(), element.release());
  }


  static bool CanReplaceProceed(DcmDataset& dicom,
                                const DcmTagKey& tag,
                                DicomReplaceMode mode)
  {
    if (dicom.findAndDeleteElement(tag).good())
    {
      // This tag was existing, it has been deleted
      return true;
    }
    else
    {
      // This tag was absent, act wrt. the specified "mode"
      switch (mode)
      {
        case DicomReplaceMode_InsertIfAbsent:
          return true;

        case DicomReplaceMode_ThrowIfAbsent:
          throw OrthancException(ErrorCode_InexistentItem);

        case DicomReplaceMode_IgnoreIfAbsent:
          return false;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }


  void ParsedDicomFile::UpdateStorageUid(const DicomTag& tag,
                                         const std::string& utf8Value,
                                         bool decodeDataUriScheme)
  {
    if (tag != DICOM_TAG_SOP_CLASS_UID &&
        tag != DICOM_TAG_SOP_INSTANCE_UID)
    {
      return;
    }

    std::string binary;
    const std::string* decoded = &utf8Value;

    if (decodeDataUriScheme &&
        boost::starts_with(utf8Value, "data:application/octet-stream;base64,"))
    {
      std::string mime;
      if (!Toolbox::DecodeDataUriScheme(mime, binary, utf8Value))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      decoded = &binary;
    }
    else
    {
      Encoding encoding = GetEncoding();
      if (GetEncoding() != Encoding_Utf8)
      {
        binary = Toolbox::ConvertFromUtf8(utf8Value, encoding);
        decoded = &binary;
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
      ReplacePlainString(DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID, *decoded);
    }

    if (tag == DICOM_TAG_SOP_INSTANCE_UID)
    {
      ReplacePlainString(DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID, *decoded);
    }    
  }


  void ParsedDicomFile::Replace(const DicomTag& tag,
                                const std::string& utf8Value,
                                bool decodeDataUriScheme,
                                DicomReplaceMode mode)
  {
    InvalidateCache();

    DcmDataset& dicom = *pimpl_->file_->getDataset();
    if (CanReplaceProceed(dicom, ToDcmtkBridge::Convert(tag), mode))
    {
      // Either the tag was previously existing (and now removed), or
      // the replace mode was set to "InsertIfAbsent"

      if (decodeDataUriScheme &&
          (tag == DICOM_TAG_ENCAPSULATED_DOCUMENT ||
           tag == DICOM_TAG_PIXEL_DATA))
      {
        if (EmbedContentInternal(utf8Value))
        {
          return;
        }
      }

      std::auto_ptr<DcmElement> element(FromDcmtkBridge::CreateElementForTag(tag));
      FromDcmtkBridge::FillElementWithString(*element, tag, utf8Value, decodeDataUriScheme, GetEncoding());

      InsertInternal(dicom, element.release());
      UpdateStorageUid(tag, utf8Value, false);
    }
  }

    
  void ParsedDicomFile::Replace(const DicomTag& tag,
                                const Json::Value& value,
                                bool decodeDataUriScheme,
                                DicomReplaceMode mode)
  {
    InvalidateCache();

    DcmDataset& dicom = *pimpl_->file_->getDataset();
    if (CanReplaceProceed(dicom, ToDcmtkBridge::Convert(tag), mode))
    {
      // Either the tag was previously existing (and now removed), or
      // the replace mode was set to "InsertIfAbsent"

      if (decodeDataUriScheme &&
          value.type() == Json::stringValue &&
          (tag == DICOM_TAG_ENCAPSULATED_DOCUMENT ||
           tag == DICOM_TAG_PIXEL_DATA))
      {
        if (EmbedContentInternal(value.asString()))
        {
          return;
        }
      }

      InsertInternal(dicom, FromDcmtkBridge::FromJson(tag, value, decodeDataUriScheme, GetEncoding()));

      if (tag == DICOM_TAG_SOP_CLASS_UID ||
          tag == DICOM_TAG_SOP_INSTANCE_UID)
      {
        if (value.type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadParameterType);
        }

        UpdateStorageUid(tag, value.asString(), decodeDataUriScheme);
      }
    }
  }

    
#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void ParsedDicomFile::Answer(RestApiOutput& output)
  {
    std::string serialized;
    if (FromDcmtkBridge::SaveToMemoryBuffer(serialized, *pimpl_->file_->getDataset()))
    {
      output.AnswerBuffer(serialized, CONTENT_TYPE_OCTET_STREAM);
    }
  }
#endif


  bool ParsedDicomFile::GetTagValue(std::string& value,
                                    const DicomTag& tag)
  {
    DcmTagKey k(tag.GetGroup(), tag.GetElement());
    DcmDataset& dataset = *pimpl_->file_->getDataset();

    if (tag.IsPrivate() ||
        FromDcmtkBridge::IsUnknownTag(tag) ||
        tag == DICOM_TAG_PIXEL_DATA ||
        tag == DICOM_TAG_ENCAPSULATED_DOCUMENT)
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

      std::set<DicomTag> tmp;
      std::auto_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement
                                  (*element, DicomToJsonFlags_Default, 
                                   0, GetEncoding(), tmp));
      
      if (v.get() == NULL ||
          v->IsNull())
      {
        value = "";
      }
      else
      {
        // TODO v->IsBinary()
        value = v->GetContent();
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


  void ParsedDicomFile::SaveToMemoryBuffer(std::string& buffer)
  {
    FromDcmtkBridge::SaveToMemoryBuffer(buffer, *pimpl_->file_->getDataset());
  }


  void ParsedDicomFile::SaveToFile(const std::string& path)
  {
    // TODO Avoid using a temporary memory buffer, write directly on disk
    std::string content;
    SaveToMemoryBuffer(content);
    SystemToolbox::WriteFile(content, path);
  }


  ParsedDicomFile::ParsedDicomFile(bool createIdentifiers) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat);

    if (createIdentifiers)
    {
      ReplacePlainString(DICOM_TAG_PATIENT_ID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Patient));
      ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study));
      ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series));
      ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));
    }
  }


  void ParsedDicomFile::CreateFromDicomMap(const DicomMap& source,
                                           Encoding defaultEncoding)
  {
    try
    {
      pimpl_->file_.reset(new DcmFileFormat);

      const DicomValue* tmp = source.TestAndGetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET);

      if (tmp == NULL)
      {
        SetEncoding(defaultEncoding);
      }
      else if (tmp->IsBinary())
      {
        LOG(ERROR) << "Invalid binary string in the SpecificCharacterSet (0008,0005) tag";
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else if (tmp->IsNull() ||
               tmp->GetContent().empty())
      {
        SetEncoding(defaultEncoding);
      }
      else
      {
        Encoding encoding;

        if (GetDicomEncoding(encoding, tmp->GetContent().c_str()))
        {
          SetEncoding(encoding);
        }
        else
        {
          LOG(ERROR) << "Unsupported value for the SpecificCharacterSet (0008,0005) tag: \""
                     << tmp->GetContent() << "\"";        
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }

      for (DicomMap::Map::const_iterator 
             it = source.map_.begin(); it != source.map_.end(); ++it)
      {
        if (it->first != DICOM_TAG_SPECIFIC_CHARACTER_SET &&
            !it->second->IsNull())
        {
          ReplacePlainString(it->first, it->second->GetContent());
        }
      }
    }
    catch (OrthancException&)
    {
      // Manually delete the PImpl to avoid a memory leak due to
      // throwing the exception in the constructor
      delete pimpl_;
      pimpl_ = NULL;
      throw;
    }
  }


  ParsedDicomFile::ParsedDicomFile(const DicomMap& map,
                                   Encoding defaultEncoding) :
    pimpl_(new PImpl)
  {
    CreateFromDicomMap(map, defaultEncoding);
  }


  ParsedDicomFile::ParsedDicomFile(const DicomMap& map) : 
    pimpl_(new PImpl)
  {
    CreateFromDicomMap(map, GetDefaultDicomEncoding());
  }


  ParsedDicomFile::ParsedDicomFile(const void* content, 
                                   size_t size) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(FromDcmtkBridge::LoadFromMemoryBuffer(content, size));
  }

  ParsedDicomFile::ParsedDicomFile(const std::string& content) : pimpl_(new PImpl)
  {
    if (content.size() == 0)
    {
      pimpl_->file_.reset(FromDcmtkBridge::LoadFromMemoryBuffer(NULL, 0));
    }
    else
    {
      pimpl_->file_.reset(FromDcmtkBridge::LoadFromMemoryBuffer(&content[0], content.size()));
    }
  }


  ParsedDicomFile::ParsedDicomFile(ParsedDicomFile& other) : 
    pimpl_(new PImpl)
  {
    pimpl_->file_.reset(dynamic_cast<DcmFileFormat*>(other.pimpl_->file_->clone()));

    // Create a new instance-level identifier
    ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));
  }


  ParsedDicomFile::ParsedDicomFile(DcmDataset& dicom) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat(&dicom));
  }


  ParsedDicomFile::ParsedDicomFile(DcmFileFormat& dicom) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat(dicom));
  }


  ParsedDicomFile::~ParsedDicomFile()
  {
    delete pimpl_;
  }


  DcmFileFormat& ParsedDicomFile::GetDcmtkObject() const
  {
    return *pimpl_->file_.get();
  }


  ParsedDicomFile* ParsedDicomFile::Clone()
  {
    return new ParsedDicomFile(*this);
  }


  bool ParsedDicomFile::EmbedContentInternal(const std::string& dataUriScheme)
  {
    std::string mime, content;
    if (!Toolbox::DecodeDataUriScheme(mime, content, dataUriScheme))
    {
      return false;
    }

    Toolbox::ToLowerCase(mime);

    if (mime == "image/png")
    {
#if ORTHANC_ENABLE_PNG == 1
      EmbedImage(mime, content);
#else
      LOG(ERROR) << "Orthanc was compiled without support of PNG";
      throw OrthancException(ErrorCode_NotImplemented);
#endif
    }
    else if (mime == "image/jpeg")
    {
#if ORTHANC_ENABLE_JPEG == 1
      EmbedImage(mime, content);
#else
      LOG(ERROR) << "Orthanc was compiled without support of JPEG";
      throw OrthancException(ErrorCode_NotImplemented);
#endif
    }
    else if (mime == "application/pdf")
    {
      EmbedPdf(content);
    }
    else
    {
      LOG(ERROR) << "Unsupported MIME type for the content of a new DICOM file: " << mime;
      throw OrthancException(ErrorCode_NotImplemented);
    }

    return true;
  }


  void ParsedDicomFile::EmbedContent(const std::string& dataUriScheme)
  {
    if (!EmbedContentInternal(dataUriScheme))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


#if (ORTHANC_ENABLE_JPEG == 1 &&                \
     ORTHANC_ENABLE_PNG == 1)
  void ParsedDicomFile::EmbedImage(const std::string& mime,
                                   const std::string& content)
  {
    if (mime == "image/png")
    {
      PngReader reader;
      reader.ReadFromMemory(content);
      EmbedImage(reader);
    }
    else if (mime == "image/jpeg")
    {
      JpegReader reader;
      reader.ReadFromMemory(content);
      EmbedImage(reader);
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }
#endif


  void ParsedDicomFile::EmbedImage(const ImageAccessor& accessor)
  {
    if (accessor.GetFormat() != PixelFormat_Grayscale8 &&
        accessor.GetFormat() != PixelFormat_Grayscale16 &&
        accessor.GetFormat() != PixelFormat_SignedGrayscale16 &&
        accessor.GetFormat() != PixelFormat_RGB24 &&
        accessor.GetFormat() != PixelFormat_RGBA32)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    InvalidateCache();

    if (accessor.GetFormat() == PixelFormat_RGBA32)
    {
      LOG(WARNING) << "Getting rid of the alpha channel when embedding a RGBA image inside DICOM";
    }

    // http://dicomiseasy.blogspot.be/2012/08/chapter-12-pixel-data.html

    Remove(DICOM_TAG_PIXEL_DATA);
    ReplacePlainString(DICOM_TAG_COLUMNS, boost::lexical_cast<std::string>(accessor.GetWidth()));
    ReplacePlainString(DICOM_TAG_ROWS, boost::lexical_cast<std::string>(accessor.GetHeight()));
    ReplacePlainString(DICOM_TAG_SAMPLES_PER_PIXEL, "1");
    ReplacePlainString(DICOM_TAG_NUMBER_OF_FRAMES, "1");

    if (accessor.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ReplacePlainString(DICOM_TAG_PIXEL_REPRESENTATION, "1");
    }
    else
    {
      ReplacePlainString(DICOM_TAG_PIXEL_REPRESENTATION, "0");  // Unsigned pixels
    }

    ReplacePlainString(DICOM_TAG_PLANAR_CONFIGURATION, "0");  // Color channels are interleaved
    ReplacePlainString(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2");

    unsigned int bytesPerPixel = 0;

    switch (accessor.GetFormat())
    {
      case PixelFormat_Grayscale8:
        ReplacePlainString(DICOM_TAG_BITS_ALLOCATED, "8");
        ReplacePlainString(DICOM_TAG_BITS_STORED, "8");
        ReplacePlainString(DICOM_TAG_HIGH_BIT, "7");
        bytesPerPixel = 1;
        break;

      case PixelFormat_RGB24:
      case PixelFormat_RGBA32:
        ReplacePlainString(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "RGB");
        ReplacePlainString(DICOM_TAG_SAMPLES_PER_PIXEL, "3");
        ReplacePlainString(DICOM_TAG_BITS_ALLOCATED, "8");
        ReplacePlainString(DICOM_TAG_BITS_STORED, "8");
        ReplacePlainString(DICOM_TAG_HIGH_BIT, "7");
        bytesPerPixel = 3;
        break;

      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
        ReplacePlainString(DICOM_TAG_BITS_ALLOCATED, "16");
        ReplacePlainString(DICOM_TAG_BITS_STORED, "16");
        ReplacePlainString(DICOM_TAG_HIGH_BIT, "15");
        bytesPerPixel = 2;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    assert(bytesPerPixel != 0);

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

  
  Encoding ParsedDicomFile::GetEncoding() const
  {
    return FromDcmtkBridge::DetectEncoding(*pimpl_->file_->getDataset(),
                                           GetDefaultDicomEncoding());
  }


  void ParsedDicomFile::SetEncoding(Encoding encoding)
  {
    if (encoding == Encoding_Windows1251)
    {
      // This Cyrillic codepage is not officially supported by the
      // DICOM standard. Do not set the SpecificCharacterSet tag.
      return;
    }

    std::string s = GetDicomSpecificCharacterSet(encoding);
    ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, s);
  }

  void ParsedDicomFile::DatasetToJson(Json::Value& target, 
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength)
  {
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomAsJson(target, *pimpl_->file_->getDataset(),
                                        format, flags, maxStringLength,
                                        GetDefaultDicomEncoding(), ignoreTagLength);
  }


  void ParsedDicomFile::DatasetToJson(Json::Value& target, 
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    FromDcmtkBridge::ExtractDicomAsJson(target, *pimpl_->file_->getDataset(),
                                        format, flags, maxStringLength,
                                        GetDefaultDicomEncoding(), ignoreTagLength);
  }


  void ParsedDicomFile::DatasetToJson(Json::Value& target,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    FromDcmtkBridge::ExtractDicomAsJson(target, *pimpl_->file_->getDataset(), ignoreTagLength);
  }


  void ParsedDicomFile::DatasetToJson(Json::Value& target)
  {
    const std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomAsJson(target, *pimpl_->file_->getDataset(), ignoreTagLength);
  }


  void ParsedDicomFile::HeaderToJson(Json::Value& target, 
                                     DicomToJsonFormat format)
  {
    FromDcmtkBridge::ExtractHeaderAsJson(target, *pimpl_->file_->getMetaInfo(), format, DicomToJsonFlags_None, 0);
  }


  bool ParsedDicomFile::HasTag(const DicomTag& tag) const
  {
    DcmTag key(tag.GetGroup(), tag.GetElement());
    return pimpl_->file_->getDataset()->tagExists(key);
  }


  void ParsedDicomFile::EmbedPdf(const std::string& pdf)
  {
    if (pdf.size() < 5 ||  // (*)
        strncmp("%PDF-", pdf.c_str(), 5) != 0)
    {
      LOG(ERROR) << "Not a PDF file";
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    InvalidateCache();

    ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, UID_EncapsulatedPDFStorage);
    ReplacePlainString(FromDcmtkBridge::Convert(DCM_Modality), "OT");
    ReplacePlainString(FromDcmtkBridge::Convert(DCM_ConversionType), "WSD");
    ReplacePlainString(FromDcmtkBridge::Convert(DCM_MIMETypeOfEncapsulatedDocument), "application/pdf");
    //ReplacePlainString(FromDcmtkBridge::Convert(DCM_SeriesNumber), "1");

    std::auto_ptr<DcmPolymorphOBOW> element(new DcmPolymorphOBOW(DCM_EncapsulatedDocument));

    size_t s = pdf.size();
    if (s & 1)
    {
      // The size of the buffer must be even
      s += 1;
    }

    Uint8* bytes = NULL;
    OFCondition result = element->createUint8Array(s, bytes);
    if (!result.good() || bytes == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    // Blank pad byte (no access violation, as "pdf.size() >= 5" because of (*) )
    bytes[s - 1] = 0;

    memcpy(bytes, pdf.c_str(), pdf.size());
      
    DcmPolymorphOBOW* obj = element.release();
    result = pimpl_->file_->getDataset()->insert(obj);

    if (!result.good())
    {
      delete obj;
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
  }


  bool ParsedDicomFile::ExtractPdf(std::string& pdf)
  {
    std::string sop, mime;
    
    if (!GetTagValue(sop, DICOM_TAG_SOP_CLASS_UID) ||
        !GetTagValue(mime, FromDcmtkBridge::Convert(DCM_MIMETypeOfEncapsulatedDocument)) ||
        sop != UID_EncapsulatedPDFStorage ||
        mime != "application/pdf")
    {
      return false;
    }

    if (!GetTagValue(pdf, DICOM_TAG_ENCAPSULATED_DOCUMENT))
    {
      return false;
    }

    // Strip the possible pad byte at the end of file, because the
    // encapsulated documents must always have an even length. The PDF
    // format expects files to end with %%EOF followed by CR/LF. If
    // the last character of the file is not a CR or LF, we assume it
    // is a pad byte and remove it.
    if (pdf.size() > 0)
    {
      char last = *pdf.rbegin();

      if (last != 10 && last != 13)
      {
        pdf.resize(pdf.size() - 1);
      }
    }

    return true;
  }


  ParsedDicomFile* ParsedDicomFile::CreateFromJson(const Json::Value& json,
                                                   DicomFromJsonFlags flags)
  {
    const bool generateIdentifiers = (flags & DicomFromJsonFlags_GenerateIdentifiers) ? true : false;
    const bool decodeDataUriScheme = (flags & DicomFromJsonFlags_DecodeDataUriScheme) ? true : false;

    std::auto_ptr<ParsedDicomFile> result(new ParsedDicomFile(generateIdentifiers));
    result->SetEncoding(FromDcmtkBridge::ExtractEncoding(json, GetDefaultDicomEncoding()));

    const Json::Value::Members tags = json.getMemberNames();
    
    for (size_t i = 0; i < tags.size(); i++)
    {
      DicomTag tag = FromDcmtkBridge::ParseTag(tags[i]);
      const Json::Value& value = json[tags[i]];

      if (tag == DICOM_TAG_PIXEL_DATA ||
          tag == DICOM_TAG_ENCAPSULATED_DOCUMENT)
      {
        if (value.type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadRequest);
        }
        else
        {
          result->EmbedContent(value.asString());
        }
      }
      else if (tag != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        result->Replace(tag, value, decodeDataUriScheme, DicomReplaceMode_InsertIfAbsent);
      }
    }

    return result.release();
  }


  void ParsedDicomFile::GetRawFrame(std::string& target,
                                    std::string& mime,
                                    unsigned int frameId)
  {
    if (pimpl_->frameIndex_.get() == NULL)
    {
      pimpl_->frameIndex_.reset(new DicomFrameIndex(*pimpl_->file_));
    }

    pimpl_->frameIndex_->GetRawFrame(target, frameId);

    E_TransferSyntax transferSyntax = pimpl_->file_->getDataset()->getOriginalXfer();
    switch (transferSyntax)
    {
      case EXS_JPEGProcess1:
        mime = "image/jpeg";
        break;
       
      case EXS_JPEG2000LosslessOnly:
      case EXS_JPEG2000:
        mime = "image/jp2";
        break;

      default:
        mime = "application/octet-stream";
        break;
    }
  }


  void ParsedDicomFile::InvalidateCache()
  {
    pimpl_->frameIndex_.reset(NULL);
  }


  unsigned int ParsedDicomFile::GetFramesCount() const
  {
    return DicomFrameIndex::GetFramesCount(*pimpl_->file_);
  }


  void ParsedDicomFile::ChangeEncoding(Encoding target)
  {
    Encoding source = GetEncoding();

    if (source != target)  // Avoid unnecessary conversion
    {
      ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, GetDicomSpecificCharacterSet(target));
      FromDcmtkBridge::ChangeStringEncoding(*pimpl_->file_->getDataset(), source, target);
    }
  }


  void ParsedDicomFile::ExtractDicomSummary(DicomMap& target) const
  {
    FromDcmtkBridge::ExtractDicomSummary(target, *pimpl_->file_->getDataset());
  }


  bool ParsedDicomFile::LookupTransferSyntax(std::string& result)
  {
    return FromDcmtkBridge::LookupTransferSyntax(result, *pimpl_->file_);
  }


  bool ParsedDicomFile::LookupPhotometricInterpretation(PhotometricInterpretation& result) const
  {
    DcmTagKey k(DICOM_TAG_PHOTOMETRIC_INTERPRETATION.GetGroup(),
                DICOM_TAG_PHOTOMETRIC_INTERPRETATION.GetElement());

    DcmDataset& dataset = *pimpl_->file_->getDataset();

    const char *c = NULL;
    if (dataset.findAndGetString(k, c).good() &&
        c != NULL)
    {
      result = StringToPhotometricInterpretation(c);
      return true;
    }
    else
    {
      return false;
    }
  }
}
