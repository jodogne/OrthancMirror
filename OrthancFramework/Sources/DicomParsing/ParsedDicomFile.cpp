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
#include "Internals/DicomFrameIndex.h"
#include "Internals/DicomImageDecoder.h"
#include "ToDcmtkBridge.h"

#include "../Images/Image.h"
#include "../Images/ImageProcessing.h"
#include "../Images/PamReader.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "../Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

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
#include <dcmtk/dcmdata/dcswap.h>

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
    std::unique_ptr<DcmFileFormat> file_;
    std::unique_ptr<DicomFrameIndex>  frameIndex_;
  };


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
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
        ORTHANC_OVERRIDE
      {
        // No support for compression
        return HttpCompression_None;
      }

      virtual bool HasContentFilename(std::string& filename) ORTHANC_OVERRIDE
      {
        return false;
      }

      virtual std::string GetContentType() ORTHANC_OVERRIDE
      {
        return EnumerationToString(MimeType_Binary);
      }

      virtual uint64_t  GetContentLength() ORTHANC_OVERRIDE
      {
        return length_;
      }
 
      virtual bool ReadNextChunk() ORTHANC_OVERRIDE
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
            throw OrthancException(ErrorCode_InternalError,
                                   "Error while sending a DICOM field: " +
                                   std::string(cond.text()));
          }

          return true;
        }
      }
 
      virtual const char *GetChunkContent() ORTHANC_OVERRIDE
      {
        return chunk_.c_str();
      }
 
      virtual size_t GetChunkSize() ORTHANC_OVERRIDE
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
                output.AnswerBuffer(NULL, 0, MimeType_Binary);
                return true;
              }

              Uint8* buffer = NULL;
              if (pixelItem->getUint8Array(buffer).good() && buffer)
              {
                output.AnswerBuffer(buffer, pixelItem->getLength(), MimeType_Binary);
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

  
#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void ParsedDicomFile::SendPathValue(RestApiOutput& output,
                                      const UriComponents& uri) const
  {
    DcmItem* dicom = GetDcmtkObjectConst().getDataset();
    E_TransferSyntax transferSyntax = GetDcmtkObjectConst().getDataset()->getCurrentXfer();

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
    RemovePath(DicomPath(tag));
  }


  void ParsedDicomFile::Clear(const DicomTag& tag,
                              bool onlyIfExists)
  {
    ClearPath(DicomPath(tag), onlyIfExists);
  }


  void ParsedDicomFile::RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep)
  {
    InvalidateCache();

    DcmDataset& dataset = *GetDcmtkObject().getDataset();

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
                               bool decodeDataUriScheme,
                               const std::string& privateCreator)
  {
    if (tag.GetElement() == 0x0000)
    {
      // Prevent manually modifying generic group length tags: This is
      // handled by DCMTK serialization
      return;
    }

    if (GetDcmtkObject().getDataset()->tagExists(ToDcmtkBridge::Convert(tag)))
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

    bool hasCodeExtensions;
    Encoding encoding = DetectEncoding(hasCodeExtensions);
    std::unique_ptr<DcmElement> element(FromDcmtkBridge::FromJson(tag, value, decodeDataUriScheme, encoding, privateCreator));
    InsertInternal(*GetDcmtkObject().getDataset(), element.release());
  }


  void ParsedDicomFile::ReplacePlainString(const DicomTag& tag,
                                           const std::string& utf8Value)
  {
    if (tag.IsPrivate())
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot apply this function to private tags: " + tag.Format());
    }
    else
    {
      Replace(tag, utf8Value, false, DicomReplaceMode_InsertIfAbsent,
              "" /* not a private tag, so no private creator */);
    }
  }


  void ParsedDicomFile::SetIfAbsent(const DicomTag& tag,
                                    const std::string& utf8Value)
  {
    std::string currentValue;
    if (!GetTagValue(currentValue, tag))
    {
      ReplacePlainString(tag, utf8Value);
    }
  }

  void ParsedDicomFile::RemovePrivateTags()
  {
    RemovePrivateTagsInternal(NULL);
  }

  void ParsedDicomFile::RemovePrivateTags(const std::set<DicomTag> &toKeep)
  {
    RemovePrivateTagsInternal(&toKeep);
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
          throw OrthancException(ErrorCode_InexistentItem, "Cannot replace inexistent tag: " +
                                 FromDcmtkBridge::GetTagName(DicomTag(tag.getGroup(), tag.getElement()), ""));

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
        boost::starts_with(utf8Value, URI_SCHEME_PREFIX_BINARY))
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
      bool hasCodeExtensions;
      Encoding encoding = DetectEncoding(hasCodeExtensions);
      if (encoding != Encoding_Utf8)
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
                                DicomReplaceMode mode,
                                const std::string& privateCreator)
  {
    if (tag.GetElement() == 0x0000)
    {
      // Prevent manually modifying generic group length tags: This is
      // handled by DCMTK serialization
      return;
    }
    else
    {
      InvalidateCache();

      DcmDataset& dicom = *GetDcmtkObject().getDataset();
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

        std::unique_ptr<DcmElement> element(FromDcmtkBridge::CreateElementForTag(tag, privateCreator));

        if (!utf8Value.empty())
        {
          bool hasCodeExtensions;
          Encoding encoding = DetectEncoding(hasCodeExtensions);
          FromDcmtkBridge::FillElementWithString(*element, utf8Value, decodeDataUriScheme, encoding);
        }

        InsertInternal(dicom, element.release());

        if (tag == DICOM_TAG_SOP_CLASS_UID ||
            tag == DICOM_TAG_SOP_INSTANCE_UID)
        {
          if (decodeDataUriScheme &&
              boost::starts_with(utf8Value, URI_SCHEME_PREFIX_BINARY))
          {
            std::string mime, decoded;
            if (!Toolbox::DecodeDataUriScheme(mime, decoded, utf8Value))
            {
              throw OrthancException(ErrorCode_BadFileFormat);
            }
            else
            {
              UpdateStorageUid(tag, decoded, false);
            }
          }
          else
          {
            UpdateStorageUid(tag, utf8Value, false);
          }
        }
      }
    }
  }

    
  void ParsedDicomFile::Replace(const DicomTag& tag,
                                const Json::Value& value,
                                bool decodeDataUriScheme,
                                DicomReplaceMode mode,
                                const std::string& privateCreator)
  {
    if (tag.GetElement() == 0x0000)
    {
      // Prevent manually modifying generic group length tags: This is
      // handled by DCMTK serialization
      return;
    }
    else if (value.type() == Json::stringValue)
    {
      Replace(tag, value.asString(), decodeDataUriScheme, mode, privateCreator);
    }
    else
    {
      if (tag == DICOM_TAG_SOP_CLASS_UID ||
          tag == DICOM_TAG_SOP_INSTANCE_UID)
      {
        // Must be a string
        throw OrthancException(ErrorCode_BadParameterType);
      }

      InvalidateCache();

      DcmDataset& dicom = *GetDcmtkObject().getDataset();
      if (CanReplaceProceed(dicom, ToDcmtkBridge::Convert(tag), mode))
      {
        // Either the tag was previously existing (and now removed), or
        // the replace mode was set to "InsertIfAbsent"

        bool hasCodeExtensions;
        Encoding encoding = DetectEncoding(hasCodeExtensions);
        InsertInternal(dicom, FromDcmtkBridge::FromJson(tag, value, decodeDataUriScheme, encoding, privateCreator));
      }
    }
  }

    
#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void ParsedDicomFile::Answer(RestApiOutput& output) const
  {
    std::string serialized;
    if (FromDcmtkBridge::SaveToMemoryBuffer(serialized, *GetDcmtkObjectConst().getDataset()))
    {
      output.AnswerBuffer(serialized, MimeType_Dicom);
    }
  }
#endif


  bool ParsedDicomFile::GetTagValue(std::string& value,
                                    const DicomTag& tag) const
  {
    DcmTagKey k(tag.GetGroup(), tag.GetElement());
    DcmDataset& dataset = *GetDcmtkObjectConst().getDataset();

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

      bool hasCodeExtensions;
      Encoding encoding = DetectEncoding(hasCodeExtensions);
      
      std::set<DicomTag> tmp;
      std::unique_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement
                                    (*element, DicomToJsonFlags_Default, 
                                     0, encoding, hasCodeExtensions, tmp));
      
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


  DicomInstanceHasher ParsedDicomFile::GetHasher() const
  {
    std::string patientId, studyUid, seriesUid, instanceUid;

    if (!GetTagValue(patientId, DICOM_TAG_PATIENT_ID))
    {
      /**
       * If "PatientID" is absent, be tolerant by considering it
       * equals the empty string, then proceed. In Orthanc <= 1.5.6,
       * an exception "Bad file format" was generated.
       * https://groups.google.com/d/msg/orthanc-users/aphG_h1AHVg/rfOTtTPTAgAJ
       * https://hg.orthanc-server.com/orthanc/rev/4c45e018bd3de3cfa21d6efc6734673aaaee4435
       **/
      patientId.clear();
    }        
    
    if (!GetTagValue(studyUid, DICOM_TAG_STUDY_INSTANCE_UID) ||
        !GetTagValue(seriesUid, DICOM_TAG_SERIES_INSTANCE_UID) ||
        !GetTagValue(instanceUid, DICOM_TAG_SOP_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadFileFormat, "missing StudyInstanceUID, SeriesInstanceUID or SOPInstanceUID");
    }

    return DicomInstanceHasher(patientId, studyUid, seriesUid, instanceUid);
  }


  void ParsedDicomFile::SaveToMemoryBuffer(std::string& buffer)
  {
    if (!FromDcmtkBridge::SaveToMemoryBuffer(buffer, *GetDcmtkObject().getDataset()))
    {
      throw OrthancException(ErrorCode_InternalError, "Cannot write DICOM file to memory");
    }
  }


#if ORTHANC_SANDBOXED == 0
  void ParsedDicomFile::SaveToFile(const std::string& path)
  {
    // TODO Avoid using a temporary memory buffer, write directly on disk
    std::string content;
    SaveToMemoryBuffer(content);
    SystemToolbox::WriteFile(content, path);
  }
#endif


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
                                           Encoding defaultEncoding,
                                           bool permissive,
                                           const std::string& defaultPrivateCreator,
                                           const std::map<uint16_t, std::string>& privateCreators)
  {
    pimpl_->file_.reset(new DcmFileFormat);
    InvalidateCache();

    const DicomValue* tmp = source.TestAndGetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET);

    if (tmp == NULL)
    {
      SetEncoding(defaultEncoding);
    }
    else if (tmp->IsBinary())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Invalid binary string in the SpecificCharacterSet (0008,0005) tag");
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
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Unsupported value for the SpecificCharacterSet (0008,0005) tag: \"" +
                               tmp->GetContent() + "\"");
      }
    }

    for (DicomMap::Content::const_iterator 
           it = source.content_.begin(); it != source.content_.end(); ++it)
    {
      if (it->first != DICOM_TAG_SPECIFIC_CHARACTER_SET &&
          !it->second->IsNull())
      {
        try
        {
          // Same as "ReplacePlainString()", but with support for private creator
          const std::string& utf8Value = it->second->GetContent();

          std::map<uint16_t, std::string>::const_iterator found = privateCreators.find(it->first.GetGroup());
          
          if (it->first.IsPrivate() &&
              found != privateCreators.end())
          {
            Replace(it->first, utf8Value, false, DicomReplaceMode_InsertIfAbsent, found->second);
          }
          else
          {
            Replace(it->first, utf8Value, false, DicomReplaceMode_InsertIfAbsent, defaultPrivateCreator);
          }
        }
        catch (OrthancException&)
        {
          if (!permissive)
          {
            throw;
          }
        }
      }
    }
  }

  ParsedDicomFile::ParsedDicomFile(const DicomMap& map,
                                   Encoding defaultEncoding,
                                   bool permissive) :
    pimpl_(new PImpl)
  {
    std::map<uint16_t, std::string> noPrivateCreators;
    CreateFromDicomMap(map, defaultEncoding, permissive, "" /* no default private creator */, noPrivateCreators);
  }


  ParsedDicomFile::ParsedDicomFile(const DicomMap& map,
                                   Encoding defaultEncoding,
                                   bool permissive,
                                   const std::string& defaultPrivateCreator,
                                   const std::map<uint16_t, std::string>& privateCreators) :
    pimpl_(new PImpl)
  {
    CreateFromDicomMap(map, defaultEncoding, permissive, defaultPrivateCreator, privateCreators);
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


  ParsedDicomFile::ParsedDicomFile(const ParsedDicomFile& other,
                                   bool keepSopInstanceUid) : 
    pimpl_(new PImpl)
  {
    pimpl_->file_.reset(dynamic_cast<DcmFileFormat*>(other.GetDcmtkObjectConst().clone()));

    if (!keepSopInstanceUid)
    {
      // Create a new instance-level identifier
      ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));
    }
  }


  ParsedDicomFile::ParsedDicomFile(DcmDataset& dicom) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat(&dicom));
  }


  ParsedDicomFile::ParsedDicomFile(DcmFileFormat& dicom) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(new DcmFileFormat(dicom));
  }


  ParsedDicomFile::ParsedDicomFile(DcmFileFormat* dicom) : pimpl_(new PImpl)
  {
    pimpl_->file_.reset(dicom);  // No cloning
  }


  DcmFileFormat& ParsedDicomFile::GetDcmtkObjectConst() const
  {
    if (pimpl_->file_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "ReleaseDcmtkObject() was called");
    }
    else
    {
      return *pimpl_->file_;
    }
  }

  ParsedDicomFile *ParsedDicomFile::AcquireDcmtkObject(DcmFileFormat *dicom)  // No clone here
  {
    return new ParsedDicomFile(dicom);
  }

  DcmFileFormat &ParsedDicomFile::GetDcmtkObject()
  {
    return GetDcmtkObjectConst();
  }


  DcmFileFormat* ParsedDicomFile::ReleaseDcmtkObject()
  {
    if (pimpl_->file_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "ReleaseDcmtkObject() was called");
    }
    else
    {
      pimpl_->frameIndex_.reset(NULL);
      return pimpl_->file_.release();
    }
  }


  ParsedDicomFile* ParsedDicomFile::Clone(bool keepSopInstanceUid) const
  {
    return new ParsedDicomFile(*this, keepSopInstanceUid);
  }


  bool ParsedDicomFile::EmbedContentInternal(const std::string& dataUriScheme)
  {
    std::string mimeString, content;
    if (!Toolbox::DecodeDataUriScheme(mimeString, content, dataUriScheme))
    {
      return false;
    }

    Toolbox::ToLowerCase(mimeString);
    MimeType mime = StringToMimeType(mimeString);

    switch (mime)
    {
      case MimeType_Png:
#if ORTHANC_ENABLE_PNG == 1
        EmbedImage(mime, content);
        break;
#else
        throw OrthancException(ErrorCode_NotImplemented,
                               "Orthanc was compiled without support of PNG");
#endif

      case MimeType_Jpeg:
#if ORTHANC_ENABLE_JPEG == 1
        EmbedImage(mime, content);
        break;
#else
        throw OrthancException(ErrorCode_NotImplemented,
                               "Orthanc was compiled without support of JPEG");
#endif

      case MimeType_Pam:
        EmbedImage(mime, content);
        break;

      case MimeType_Binary:
        EmbedImage(mime, content);
        break;

      case MimeType_Pdf:
        EmbedPdf(content);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented,
                               "Unsupported MIME type for the content of a new DICOM file: " +
                               std::string(EnumerationToString(mime)));
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


  void ParsedDicomFile::EmbedImage(MimeType mime,
                                   const std::string& content)
  {
    switch (mime)
    {
    
#if ORTHANC_ENABLE_JPEG == 1
      case MimeType_Jpeg:
      {
        JpegReader reader;
        reader.ReadFromMemory(content);
        EmbedImage(reader);
        break;
      }
#endif
    
#if ORTHANC_ENABLE_PNG == 1
      case MimeType_Png:
      {
        PngReader reader;
        reader.ReadFromMemory(content);
        EmbedImage(reader);
        break;
      }
#endif

      case MimeType_Pam:
      {
        // "true" means "enforce memory alignment": This is slower,
        // but possibly avoids crash related to non-aligned memory access
        PamReader reader(true);
        reader.ReadFromMemory(content);
        EmbedImage(reader);
        break;
      }

      case MimeType_Binary:
        EmbedRawPixelData(content);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ParsedDicomFile::EmbedImage(const ImageAccessor& accessor)
  {
    if (accessor.GetFormat() != PixelFormat_Grayscale8 &&
        accessor.GetFormat() != PixelFormat_Grayscale16 &&
        accessor.GetFormat() != PixelFormat_SignedGrayscale16 &&
        accessor.GetFormat() != PixelFormat_RGB24 &&
        accessor.GetFormat() != PixelFormat_RGBA32 && 
        accessor.GetFormat() != PixelFormat_RGBA64)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    InvalidateCache();

    if (accessor.GetFormat() == PixelFormat_RGBA32 || 
        accessor.GetFormat() == PixelFormat_RGBA64)
    {
      LOG(WARNING) << "Getting rid of the alpha channel when embedding a RGBA image inside DICOM";
    }

    // http://dicomiseasy.blogspot.be/2012/08/chapter-12-pixel-data.html

    Remove(DICOM_TAG_PIXEL_DATA);
    ReplacePlainString(DICOM_TAG_COLUMNS, boost::lexical_cast<std::string>(accessor.GetWidth()));
    ReplacePlainString(DICOM_TAG_ROWS, boost::lexical_cast<std::string>(accessor.GetHeight()));
    ReplacePlainString(DICOM_TAG_SAMPLES_PER_PIXEL, "1");

    // The "Number of frames" must only be present in multi-frame images
    //ReplacePlainString(DICOM_TAG_NUMBER_OF_FRAMES, "1");

    if (accessor.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ReplacePlainString(DICOM_TAG_PIXEL_REPRESENTATION, "1");
    }
    else
    {
      ReplacePlainString(DICOM_TAG_PIXEL_REPRESENTATION, "0");  // Unsigned pixels
    }

    unsigned int bytesPerPixel = 0;

    switch (accessor.GetFormat())
    {
      case PixelFormat_Grayscale8:
        // By default, grayscale images are MONOCHROME2
        SetIfAbsent(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2");

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

        // "Planar configuration" must only present if "Samples per
        // Pixel" is greater than 1
        ReplacePlainString(DICOM_TAG_PLANAR_CONFIGURATION, "0");  // Color channels are interleaved

        break;
      
      case PixelFormat_RGBA64:
        ReplacePlainString(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "RGB");
        ReplacePlainString(DICOM_TAG_SAMPLES_PER_PIXEL, "3");
        ReplacePlainString(DICOM_TAG_BITS_ALLOCATED, "16");
        ReplacePlainString(DICOM_TAG_BITS_STORED, "16");
        ReplacePlainString(DICOM_TAG_HIGH_BIT, "15");
        bytesPerPixel = 6;

        // "Planar configuration" must only present if "Samples per
        // Pixel" is greater than 1
        ReplacePlainString(DICOM_TAG_PLANAR_CONFIGURATION, "0");  // Color channels are interleaved

        break;

      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
        // By default, grayscale images are MONOCHROME2
        SetIfAbsent(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2");

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

    std::unique_ptr<DcmPixelData> pixels(new DcmPixelData(key));

    unsigned int pitch = accessor.GetWidth() * bytesPerPixel;
    Uint8* target = NULL;
    pixels->createUint8Array(accessor.GetHeight() * pitch, target);

    const unsigned int height = accessor.GetHeight();
    const unsigned int width = accessor.GetWidth();

    {
      Uint8* q = target;
      for (unsigned int y = 0; y < height; y++)
      {
        switch (accessor.GetFormat())
        {
          case PixelFormat_RGB24:
          case PixelFormat_Grayscale8:
          case PixelFormat_Grayscale16:
          case PixelFormat_SignedGrayscale16:
          {
            memcpy(q, reinterpret_cast<const Uint8*>(accessor.GetConstRow(y)), pitch);
            q += pitch;
            break;
          }

          case PixelFormat_RGBA32:
          {
            // The alpha channel is not supported by the DICOM standard
            const Uint8* source = reinterpret_cast<const Uint8*>(accessor.GetConstRow(y));
            for (unsigned int x = 0; x < width; x++, q += 3, source += 4)
            {
              q[0] = source[0];
              q[1] = source[1];
              q[2] = source[2];
            }

            break;
          }

          case PixelFormat_RGBA64:
          {
            // The alpha channel is not supported by the DICOM standard
            const Uint8* source = reinterpret_cast<const Uint8*>(accessor.GetConstRow(y));
            for (unsigned int x = 0; x < width; x++, q += 6, source += 8)
            {
              q[0] = source[0];
              q[1] = source[1];
              q[2] = source[2];
              q[3] = source[3];
              q[4] = source[4];
              q[5] = source[5];
            }

            break;
          }
          
          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      }
    }

    static const Endianness ENDIANNESS = Toolbox::DetectEndianness();
    if (ENDIANNESS == Endianness_Big &&
        (accessor.GetFormat() == PixelFormat_Grayscale16 ||
         accessor.GetFormat() == PixelFormat_SignedGrayscale16))
    {
      // New in Orthanc 1.9.1
      assert(pitch % 2 == 0);
      swapBytes(target, accessor.GetHeight() * pitch, sizeof(uint16_t));
    }

    if (!GetDcmtkObject().getDataset()->insert(pixels.release(), false, false).good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }    
  }

  void ParsedDicomFile::EmbedRawPixelData(const std::string& content)
  {
    DcmTag key(DICOM_TAG_PIXEL_DATA.GetGroup(), 
               DICOM_TAG_PIXEL_DATA.GetElement());

    std::unique_ptr<DcmPixelData> pixels(new DcmPixelData(key));

    Uint8* target = NULL;
    pixels->createUint8Array(content.size(), target);
    memcpy(target, content.c_str(), content.size());

    if (!GetDcmtkObject().getDataset()->insert(pixels.release(), false, false).good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  Encoding ParsedDicomFile::DetectEncoding(bool& hasCodeExtensions) const
  {
    return FromDcmtkBridge::DetectEncoding(hasCodeExtensions,
                                           *GetDcmtkObjectConst().getDataset(),
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
                                      unsigned int maxStringLength) const
  {
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomAsJson(target, *GetDcmtkObjectConst().getDataset(),
                                        format, flags, maxStringLength, ignoreTagLength);
  }


  void ParsedDicomFile::DatasetToJson(Json::Value& target, 
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength,
                                      const std::set<DicomTag>& ignoreTagLength) const
  {
    FromDcmtkBridge::ExtractDicomAsJson(target, *GetDcmtkObjectConst().getDataset(),
                                        format, flags, maxStringLength, ignoreTagLength);
  }


  void ParsedDicomFile::HeaderToJson(Json::Value& target, 
                                     DicomToJsonFormat format) const
  {
    FromDcmtkBridge::ExtractHeaderAsJson(target, *GetDcmtkObjectConst().getMetaInfo(), format, DicomToJsonFlags_None, 0);
  }


  bool ParsedDicomFile::HasTag(const DicomTag& tag) const
  {
    DcmTag key(tag.GetGroup(), tag.GetElement());
    return GetDcmtkObjectConst().getDataset()->tagExists(key);
  }


  void ParsedDicomFile::EmbedPdf(const std::string& pdf)
  {
    if (pdf.size() < 5 ||  // (*)
        strncmp("%PDF-", pdf.c_str(), 5) != 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Not a PDF file");
    }

    InvalidateCache();

    // In Orthanc <= 1.9.7, the "Modality" would have always be overwritten as "OT"
    // https://groups.google.com/g/orthanc-users/c/eNSddNrQDtM/m/wc1HahimAAAJ
    
    ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, UID_EncapsulatedPDFStorage);
    SetIfAbsent(FromDcmtkBridge::Convert(DCM_Modality), "OT");
    SetIfAbsent(FromDcmtkBridge::Convert(DCM_ConversionType), "WSD");
    SetIfAbsent(FromDcmtkBridge::Convert(DCM_MIMETypeOfEncapsulatedDocument), MIME_PDF);
    //SetIfAbsent(FromDcmtkBridge::Convert(DCM_SeriesNumber), "1");

    std::unique_ptr<DcmPolymorphOBOW> element(new DcmPolymorphOBOW(DCM_EncapsulatedDocument));

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
    result = GetDcmtkObject().getDataset()->insert(obj);

    if (!result.good())
    {
      delete obj;
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
  }


  bool ParsedDicomFile::ExtractPdf(std::string& pdf) const
  {
    std::string sop, mime;
    
    if (!GetTagValue(sop, DICOM_TAG_SOP_CLASS_UID) ||
        !GetTagValue(mime, FromDcmtkBridge::Convert(DCM_MIMETypeOfEncapsulatedDocument)) ||
        sop != UID_EncapsulatedPDFStorage ||
        mime != MIME_PDF)
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
                                                   DicomFromJsonFlags flags,
                                                   const std::string& privateCreator)
  {
    const bool generateIdentifiers = (flags & DicomFromJsonFlags_GenerateIdentifiers) ? true : false;
    const bool decodeDataUriScheme = (flags & DicomFromJsonFlags_DecodeDataUriScheme) ? true : false;

    std::unique_ptr<ParsedDicomFile> result(new ParsedDicomFile(generateIdentifiers));
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
        result->Replace(tag, value, decodeDataUriScheme, DicomReplaceMode_InsertIfAbsent, privateCreator);
      }
    }

    return result.release();
  }


  void ParsedDicomFile::GetRawFrame(std::string& target,
                                    MimeType& mime,
                                    unsigned int frameId) const
  {
    if (pimpl_->frameIndex_.get() == NULL)
    {
      assert(pimpl_->file_ != NULL &&
             GetDcmtkObjectConst().getDataset() != NULL);
      pimpl_->frameIndex_.reset(new DicomFrameIndex(*GetDcmtkObjectConst().getDataset()));
    }

    pimpl_->frameIndex_->GetRawFrame(target, frameId);

    E_TransferSyntax transferSyntax = GetDcmtkObjectConst().getDataset()->getCurrentXfer();
    switch (transferSyntax)
    {
      case EXS_JPEGProcess1:
        mime = MimeType_Jpeg;
        break;
       
      case EXS_JPEG2000LosslessOnly:
      case EXS_JPEG2000:
        mime = MimeType_Jpeg2000;
        break;

      default:
        mime = MimeType_Binary;
        break;
    }
  }


  void ParsedDicomFile::InvalidateCache()
  {
    pimpl_->frameIndex_.reset(NULL);
  }


  unsigned int ParsedDicomFile::GetFramesCount() const
  {
    assert(pimpl_->file_ != NULL &&
           GetDcmtkObjectConst().getDataset() != NULL);
    return DicomFrameIndex::GetFramesCount(*GetDcmtkObjectConst().getDataset());
  }


  void ParsedDicomFile::ChangeEncoding(Encoding target)
  {
    bool hasCodeExtensions;
    Encoding source = DetectEncoding(hasCodeExtensions);

    if (source != target)  // Avoid unnecessary conversion
    {
      ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, GetDicomSpecificCharacterSet(target));
      FromDcmtkBridge::ChangeStringEncoding(*GetDcmtkObject().getDataset(), source, hasCodeExtensions, target);
    }
  }


  void ParsedDicomFile::ExtractDicomSummary(DicomMap& target,
                                            unsigned int maxTagLength) const
  {
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomSummary(target, *GetDcmtkObjectConst().getDataset(),
                                         maxTagLength, ignoreTagLength);
  }


  void ParsedDicomFile::ExtractDicomSummary(DicomMap& target,
                                            unsigned int maxTagLength,
                                            const std::set<DicomTag>& ignoreTagLength) const
  {
    FromDcmtkBridge::ExtractDicomSummary(target, *GetDcmtkObjectConst().getDataset(),
                                         maxTagLength, ignoreTagLength);
  }


  bool ParsedDicomFile::LookupTransferSyntax(DicomTransferSyntax& result) const
  {
    return FromDcmtkBridge::LookupOrthancTransferSyntax(result, GetDcmtkObjectConst());
  }


  bool ParsedDicomFile::LookupPhotometricInterpretation(PhotometricInterpretation& result) const
  {
    DcmTagKey k(DICOM_TAG_PHOTOMETRIC_INTERPRETATION.GetGroup(),
                DICOM_TAG_PHOTOMETRIC_INTERPRETATION.GetElement());

    DcmDataset& dataset = *GetDcmtkObjectConst().getDataset();

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


  void ParsedDicomFile::Apply(ITagVisitor& visitor) const
  {
    FromDcmtkBridge::Apply(*GetDcmtkObjectConst().getDataset(), visitor, GetDefaultDicomEncoding());
  }


  ImageAccessor* ParsedDicomFile::DecodeFrame(unsigned int frame) const
  {
    if (GetDcmtkObjectConst().getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      return DicomImageDecoder::Decode(*GetDcmtkObjectConst().getDataset(), frame);
    }
  }


  static bool HasGenericGroupLength(const DicomPath& path)
  {
    for (size_t i = 0; i < path.GetPrefixLength(); i++)
    {
      if (path.GetPrefixTag(i).GetElement() == 0x0000)
      {
        return true;
      }
    }
    
    return (path.GetFinalTag().GetElement() == 0x0000);
  }
  

  void ParsedDicomFile::ReplacePath(const DicomPath& path,
                                    const Json::Value& value,
                                    bool decodeDataUriScheme,
                                    DicomReplaceMode mode,
                                    const std::string& privateCreator)
  {
    if (HasGenericGroupLength(path))
    {
      // Prevent manually modifying generic group length tags: This is
      // handled by DCMTK serialization
      return;
    }
    else if (path.GetPrefixLength() == 0)
    {
      Replace(path.GetFinalTag(), value, decodeDataUriScheme, mode, privateCreator);
    }
    else
    {
      InvalidateCache();

      bool hasCodeExtensions;
      Encoding encoding = DetectEncoding(hasCodeExtensions);
      std::unique_ptr<DcmElement> element(
        FromDcmtkBridge::FromJson(path.GetFinalTag(), value, decodeDataUriScheme, encoding, privateCreator));

      FromDcmtkBridge::ReplacePath(*GetDcmtkObject().getDataset(), path, *element, mode);
    }
  }
  

  void ParsedDicomFile::RemovePath(const DicomPath& path)
  {
    InvalidateCache();
    FromDcmtkBridge::RemovePath(*GetDcmtkObject().getDataset(), path);
  }


  void ParsedDicomFile::ClearPath(const DicomPath& path,
                                  bool onlyIfExists)
  {
    if (HasGenericGroupLength(path))
    {
      // Prevent manually modifying generic group length tags: This is
      // handled by DCMTK serialization
      return;
    }
    else
    {
      InvalidateCache();
      FromDcmtkBridge::ClearPath(*GetDcmtkObject().getDataset(), path, onlyIfExists);
    }
  }


  bool ParsedDicomFile::LookupSequenceItem(DicomMap& target,
                                           const DicomPath& path,
                                           size_t sequenceIndex) const
  {
    DcmDataset& dataset = *const_cast<ParsedDicomFile&>(*this).GetDcmtkObject().getDataset();
    return FromDcmtkBridge::LookupSequenceItem(target, dataset, path, sequenceIndex);
  }
  

  void ParsedDicomFile::GetDefaultWindowing(double& windowCenter,
                                            double& windowWidth,
                                            unsigned int frame) const
  {
    DcmDataset& dataset = *const_cast<ParsedDicomFile&>(*this).GetDcmtkObject().getDataset();

    const char* wc = NULL;
    const char* ww = NULL;
    DcmItem *item1 = NULL;
    DcmItem *item2 = NULL;

    if (dataset.findAndGetString(DCM_WindowCenter, wc).good() &&
        dataset.findAndGetString(DCM_WindowWidth, ww).good() &&
        wc != NULL &&
        ww != NULL &&
        SerializationToolbox::ParseFirstDouble(windowCenter, wc) &&
        SerializationToolbox::ParseFirstDouble(windowWidth, ww))
    {
      return;  // OK
    }
    else if (dataset.findAndGetSequenceItem(DCM_PerFrameFunctionalGroupsSequence, item1, frame).good() &&
             item1 != NULL &&
             item1->findAndGetSequenceItem(DCM_FrameVOILUTSequence, item2, 0).good() &&
             item2 != NULL &&
             item2->findAndGetString(DCM_WindowCenter, wc).good() &&
             item2->findAndGetString(DCM_WindowWidth, ww).good() &&
             wc != NULL &&
             ww != NULL &&
             SerializationToolbox::ParseFirstDouble(windowCenter, wc) &&
             SerializationToolbox::ParseFirstDouble(windowWidth, ww))
    {
      // New in Orthanc 1.9.7, to deal with Philips multiframe images
      // (cf. private mail from Tomas Kenda on 2021-08-17)
      return;  // OK
    }
    else
    {
      Uint16 bitsStored = 0;
      if (!dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good() ||
          bitsStored == 0)
      {
        bitsStored = 8;  // Rough assumption
      }

      windowWidth = static_cast<double>(1 << bitsStored);
      windowCenter = windowWidth / 2.0f;
    }
  }

  
  void ParsedDicomFile::GetRescale(double& rescaleIntercept,
                                   double& rescaleSlope,
                                   unsigned int frame) const
  {
    DcmDataset& dataset = *const_cast<ParsedDicomFile&>(*this).GetDcmtkObject().getDataset();

    const char* sopClassUid = NULL;
    const char* intercept = NULL;
    const char* slope = NULL;
    DcmItem *item1 = NULL;
    DcmItem *item2 = NULL;

    if (dataset.findAndGetString(DCM_SOPClassUID, sopClassUid).good() &&
        sopClassUid != NULL &&
        std::string(sopClassUid) == std::string(UID_RTDoseStorage))
    {
      // We must not take the rescale value into account in the case of doses
      rescaleIntercept = 0;
      rescaleSlope = 1;
    }
    else if (dataset.findAndGetString(DCM_RescaleIntercept, intercept).good() &&
             dataset.findAndGetString(DCM_RescaleSlope, slope).good() &&
             intercept != NULL &&
             slope != NULL &&
             SerializationToolbox::ParseDouble(rescaleIntercept, intercept) &&
             SerializationToolbox::ParseDouble(rescaleSlope, slope))
    {
      return;  // OK
    }
    else if (dataset.findAndGetSequenceItem(DCM_PerFrameFunctionalGroupsSequence, item1, frame).good() &&
             item1 != NULL &&
             item1->findAndGetSequenceItem(DCM_PixelValueTransformationSequence, item2, 0).good() &&
             item2 != NULL &&
             item2->findAndGetString(DCM_RescaleIntercept, intercept).good() &&
             item2->findAndGetString(DCM_RescaleSlope, slope).good() &&
             intercept != NULL &&
             slope != NULL &&
             SerializationToolbox::ParseDouble(rescaleIntercept, intercept) &&
             SerializationToolbox::ParseDouble(rescaleSlope, slope))
    {
      // New in Orthanc 1.9.7, to deal with Philips multiframe images
      // (cf. private mail from Tomas Kenda on 2021-08-17)
      return;  // OK
    }
    else
    {
      rescaleIntercept = 0;
      rescaleSlope = 1;
    }
  }


  void ParsedDicomFile::ListOverlays(std::set<uint16_t>& groups) const
  {
    DcmDataset& dataset = *const_cast<ParsedDicomFile&>(*this).GetDcmtkObject().getDataset();

    // "Repeating Groups shall only be allowed in the even Groups (6000-601E,eeee)"
    // https://dicom.nema.org/medical/dicom/2021e/output/chtml/part05/sect_7.6.html

    for (uint16_t group = 0x6000; group <= 0x601e; group += 2)
    {
      if (dataset.tagExists(DcmTagKey(group, 0x0010)))
      {
        groups.insert(group);
      }
    }
  }


  static unsigned int Ceiling(unsigned int a,
                              unsigned int b)
  {
    if (a % b == 0)
    {
      return a / b;
    }
    else
    {
      return a / b + 1;
    }
  }
  

  ImageAccessor* ParsedDicomFile::DecodeOverlay(int& originX,
                                                int& originY,
                                                uint16_t group) const
  {
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.9.2.html

    DcmDataset& dataset = *const_cast<ParsedDicomFile&>(*this).GetDcmtkObject().getDataset();

    Uint16 rows, columns, bitsAllocated, bitPosition;
    const Sint16* origin = NULL;
    unsigned long originSize = 0;
    DcmElement* overlayElement = NULL;
    Uint8* overlayData = NULL;
    
    if (dataset.findAndGetUint16(DcmTagKey(group, 0x0010), rows).good() &&
        dataset.findAndGetUint16(DcmTagKey(group, 0x0011), columns).good() &&
        dataset.findAndGetSint16Array(DcmTagKey(group, 0x0050), origin, &originSize).good() &&
        origin != NULL &&
        originSize == 2 &&
        dataset.findAndGetUint16(DcmTagKey(group, 0x0100), bitsAllocated).good() &&
        bitsAllocated == 1 &&
        dataset.findAndGetUint16(DcmTagKey(group, 0x0102), bitPosition).good() &&
        bitPosition == 0 &&
        dataset.findAndGetElement(DcmTagKey(group, 0x3000), overlayElement).good() &&
        overlayElement != NULL &&
        overlayElement->getUint8Array(overlayData).good() &&
        overlayData != NULL)
    {
      /**
       * WARNING - It might seem easier to use
       * "dataset.findAndGetUint8Array()" that directly gives the size
       * of the overlay data (using the "count" parameter), instead of
       * "dataset.findAndGetElement()". Unfortunately, this does *not*
       * work with Emscripten/WebAssembly, that reports a "count" that
       * is half the number of bytes, presumably because of
       * discrepancies in the way sizeof are computed inside DCMTK.
       * The method "getLengthField()" reports the correct number of
       * bytes, even if targeting WebAssembly.
       **/

      unsigned int expectedSize = Ceiling(rows * columns, 8);
      if (overlayElement->getLengthField() < expectedSize)
      {
        throw OrthancException(ErrorCode_CorruptedFile, "Overlay doesn't have a valid number of bits");
      }
      
      originX = origin[1];
      originY = origin[0];

      std::unique_ptr<ImageAccessor> overlay(new Image(Orthanc::PixelFormat_Grayscale8, columns, rows, false));

      unsigned int posBit = 0;
      for (int y = 0; y < rows; y++)
      {
        uint8_t* target = reinterpret_cast<uint8_t*>(overlay->GetRow(y));
        
        for (int x = 0; x < columns; x++)
        {
          uint8_t source = overlayData[posBit / 8];
          uint8_t mask = 1 << (posBit % 8);

          *target = ((source & mask) ? 255 : 0);

          target++;
          posBit++;
        }
      }
      
      return overlay.release();
    }
    else
    {
      throw OrthancException(ErrorCode_CorruptedFile, "Invalid overlay");
    }
  }

  
  ImageAccessor* ParsedDicomFile::DecodeAllOverlays(int& originX,
                                                    int& originY) const
  {
    std::set<uint16_t> groups;
    ListOverlays(groups);

    if (groups.empty())
    {
      originX = 0;
      originY = 0;
      return new Image(PixelFormat_Grayscale8, 0, 0, false);
    }
    else
    {
      std::set<uint16_t>::const_iterator it = groups.begin();
      assert(it != groups.end());
      
      std::unique_ptr<ImageAccessor> result(DecodeOverlay(originX, originY, *it));
      assert(result.get() != NULL);
      ++it;

      int right = originX + static_cast<int>(result->GetWidth());
      int bottom = originY + static_cast<int>(result->GetHeight());

      while (it != groups.end())
      {
        int ox, oy;
        std::unique_ptr<ImageAccessor> overlay(DecodeOverlay(ox, oy, *it));
        assert(overlay.get() != NULL);

        int mergedX = std::min(originX, ox);
        int mergedY = std::min(originY, oy);
        right = std::max(right, ox + static_cast<int>(overlay->GetWidth()));
        bottom = std::max(bottom, oy + static_cast<int>(overlay->GetHeight()));

        assert(right >= mergedX && bottom >= mergedY);
        unsigned int width = static_cast<unsigned int>(right - mergedX);
        unsigned int height = static_cast<unsigned int>(bottom - mergedY);
        
        std::unique_ptr<ImageAccessor> merged(new Image(PixelFormat_Grayscale8, width, height, false));
        ImageProcessing::Set(*merged, 0);

        ImageAccessor a;
        merged->GetRegion(a, originX - mergedX, originY - mergedY, result->GetWidth(), result->GetHeight());
        ImageProcessing::Maximum(a, *result);

        merged->GetRegion(a, ox - mergedX, oy - mergedY, overlay->GetWidth(), overlay->GetHeight());
        ImageProcessing::Maximum(a, *overlay);

        originX = mergedX;
        originY = mergedY;
        result.reset(merged.release());
        
        ++it;
      }

      return result.release();
    }
  }


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
  void ParsedDicomFile::DatasetToJson(Json::Value& target,
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength)
  {
    return const_cast<const ParsedDicomFile&>(*this).DatasetToJson(target, format, flags, maxStringLength);
  }

  DcmFileFormat& ParsedDicomFile::GetDcmtkObject() const
  {
    return const_cast<ParsedDicomFile&>(*this).GetDcmtkObject();
  }

  void ParsedDicomFile::Apply(ITagVisitor& visitor)
  {
    const_cast<const ParsedDicomFile&>(*this).Apply(visitor);
  }

  ParsedDicomFile* ParsedDicomFile::Clone(bool keepSopInstanceUid)
  {
    return const_cast<const ParsedDicomFile&>(*this).Clone(keepSopInstanceUid);
  }
  
  bool ParsedDicomFile::LookupTransferSyntax(std::string& result)
  {
    return const_cast<const ParsedDicomFile&>(*this).LookupTransferSyntax(result);
  }
  
  bool ParsedDicomFile::LookupTransferSyntax(std::string& result) const
  {
    DicomTransferSyntax s;
    if (LookupTransferSyntax(s))
    {
      result = GetTransferSyntaxUid(s);
      return true;
    }
    else
    {
      return false;
    }
  }

  bool ParsedDicomFile::GetTagValue(std::string& value,
                                    const DicomTag& tag)
  {
    return const_cast<const ParsedDicomFile&>(*this).GetTagValue(value, tag);
  }
#endif
}
