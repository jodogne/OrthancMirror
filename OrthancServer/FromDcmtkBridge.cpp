/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "FromDcmtkBridge.h"

#include "ToDcmtkBridge.h"
#include "../Core/Toolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/PngWriter.h"
#include "../Core/Uuid.h"
#include "../Core/DicomFormat/DicomString.h"
#include "../Core/DicomFormat/DicomNullValue.h"
#include "../Core/DicomFormat/DicomIntegerPixelAccessor.h"

#include <list>
#include <limits>

#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcchrstr.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcuid.h>

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

#include <boost/math/special_functions/round.hpp>
#include <glog/logging.h>
#include <dcmtk/dcmdata/dcostrmb.h>

namespace Orthanc
{
  void ParsedDicomFile::Setup(const char* buffer, size_t size)
  {
    DcmInputBufferStream is;
    if (size > 0)
    {
      is.setBuffer(buffer, size);
    }
    is.setEos();

    file_.reset(new DcmFileFormat);
    file_->transferInit();
    if (!file_->read(is).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    file_->loadAllDataIntoMemory();
    file_->transferEnd();
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

  static void AnswerDicomField(RestApiOutput& output,
                               DcmElement& element)
  {
    // This element is not a sequence
    std::string buffer;
    buffer.resize(65536);
    Uint32 length = element.getLength();
    Uint32 offset = 0;

    output.GetLowLevelOutput().SendOkHeader("application/octet-stream", true, length, NULL);

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

      if (element.getPartialValue(&buffer[0], offset, nbytes).good())
      {
        output.GetLowLevelOutput().Send(&buffer[0], nbytes);
        offset += nbytes;
      }
      else
      {
        LOG(ERROR) << "Error while sending a DICOM field";
        return;
      }
    }

    output.MarkLowLevelOutputDone();
  }

  static void SendPathValueForLeaf(RestApiOutput& output,
                                   const std::string& tag,
                                   DcmItem& dicom)
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
        element->getVR() != EVR_UNKNOWN &&
        element->getVR() != EVR_SQ)
    {
      AnswerDicomField(output, *element);
    }
  }

  void ParsedDicomFile::SendPathValue(RestApiOutput& output,
                                      const UriComponents& uri)
  {
    DcmItem* dicom = file_->getDataset();

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
      SendPathValueForLeaf(output, uri.back(), *dicom);
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
    DcmElement* element = file_->getDataset()->remove(key);
    if (element != NULL)
    {
      delete element;
    }
  }



  void ParsedDicomFile::RemovePrivateTags()
  {
    typedef std::list<DcmElement*> Tags;

    Tags privateTags;

    DcmDataset& dataset = *file_->getDataset();
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      DcmTag tag(element->getTag());
      if (!strcmp("PrivateCreator", tag.getTagName()) ||  // TODO - This may change with future versions of DCMTK
          tag.getPrivateCreator() != NULL)
      {
        privateTags.push_back(element);
      }
    }

    for (Tags::iterator it = privateTags.begin(); 
         it != privateTags.end(); it++)
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
    std::auto_ptr<DcmElement> element(CreateElementForTag(tag));
    FillElementWithString(*element, tag, value);

    if (!file_->getDataset()->insert(element.release(), false, false).good())
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

    if (!file_->getDataset()->findAndGetElement(key, element).good() ||
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
      FillElementWithString(*element, tag, value);
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
      Replace(DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID, value, DicomReplaceMode_InsertIfAbsent);

    if (tag == DICOM_TAG_SOP_INSTANCE_UID)
      Replace(DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID, value, DicomReplaceMode_InsertIfAbsent);
  }

    
  void ParsedDicomFile::Answer(RestApiOutput& output)
  {
    std::string serialized;
    if (FromDcmtkBridge::SaveToMemoryBuffer(serialized, file_->getDataset()))
    {
      output.AnswerBuffer(serialized, "application/octet-stream");
    }
  }



  bool ParsedDicomFile::GetTagValue(std::string& value,
                                    const DicomTag& tag)
  {
    DcmTagKey k(tag.GetGroup(), tag.GetElement());
    DcmDataset& dataset = *file_->getDataset();
    DcmElement* element = NULL;
    if (!dataset.findAndGetElement(k, element).good() ||
        element == NULL)
    {
      return false;
    }

    std::auto_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement(*element));

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


  void FromDcmtkBridge::Convert(DicomMap& target, DcmDataset& dataset)
  {
    target.Clear();
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element && element->isLeaf())
      {
        target.SetValue(element->getTag().getGTag(),
                        element->getTag().getETag(),
                        ConvertLeafElement(*element));
      }
    }
  }


  DicomTag FromDcmtkBridge::GetTag(const DcmElement& element)
  {
    return DicomTag(element.getGTag(), element.getETag());
  }


  DicomValue* FromDcmtkBridge::ConvertLeafElement(DcmElement& element)
  {
    if (!element.isLeaf())
    {
      throw OrthancException("Only applicable to leaf elements");
    }

    if (element.isaString())
    {
      char *c;
      if (element.getString(c).good() &&
          c != NULL)
      {
        std::string s(c);
        std::string utf8 = Toolbox::ConvertToUtf8(s, "ISO-8859-1"); // TODO Parameter?
        return new DicomString(utf8);
      }
      else
      {
        return new DicomNullValue;
      }
    }

    try
    {
      // http://support.dcmtk.org/docs/dcvr_8h-source.html
      switch (element.getVR())
      {

        /**
         * TODO.
         **/

        case EVR_OB:  // other byte
        case EVR_OF:  // other float
        case EVR_OW:  // other word
        case EVR_AT:  // attribute tag
        case EVR_UN:  // unknown value representation
          return new DicomNullValue();
    
          /**
           * String types, should never happen at this point because of
           * "element.isaString()".
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
          return new DicomNullValue();


          /**
           * Numerical types
           **/ 
      
        case EVR_SL:  // signed long
        {
          Sint32 f;
          if (dynamic_cast<DcmSignedLong&>(element).getSint32(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }

        case EVR_SS:  // signed short
        {
          Sint16 f;
          if (dynamic_cast<DcmSignedShort&>(element).getSint16(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }

        case EVR_UL:  // unsigned long
        {
          Uint32 f;
          if (dynamic_cast<DcmUnsignedLong&>(element).getUint32(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }

        case EVR_US:  // unsigned short
        {
          Uint16 f;
          if (dynamic_cast<DcmUnsignedShort&>(element).getUint16(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }

        case EVR_FL:  // float single-precision
        {
          Float32 f;
          if (dynamic_cast<DcmFloatingPointSingle&>(element).getFloat32(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }

        case EVR_FD:  // float double-precision
        {
          Float64 f;
          if (dynamic_cast<DcmFloatingPointDouble&>(element).getFloat64(f).good())
            return new DicomString(boost::lexical_cast<std::string>(f));
          else
            return new DicomNullValue();
        }


        /**
         * Sequence types, should never occur at this point because of
         * "element.isLeaf()".
         **/

        case EVR_SQ:  // sequence of items
          return new DicomNullValue;


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
          return new DicomNullValue;


          /**
           * Default case.
           **/ 

        default:
          return new DicomNullValue;
      }
    }
    catch (boost::bad_lexical_cast)
    {
      return new DicomNullValue;
    }
    catch (std::bad_cast)
    {
      return new DicomNullValue;
    }
  }


  static void StoreElement(Json::Value& target,
                           DcmElement& element,
                           unsigned int maxStringLength);

  static void StoreItem(Json::Value& target,
                        DcmItem& item,
                        unsigned int maxStringLength)
  {
    target = Json::Value(Json::objectValue);

    for (unsigned long i = 0; i < item.card(); i++)
    {
      DcmElement* element = item.getElement(i);
      StoreElement(target, *element, maxStringLength);
    }
  }


  static void StoreElement(Json::Value& target,
                           DcmElement& element,
                           unsigned int maxStringLength)
  {
    assert(target.type() == Json::objectValue);

    DicomTag tag(FromDcmtkBridge::GetTag(element));
    const std::string formattedTag = tag.Format();

#if 0
    const std::string tagName = FromDcmtkBridge::GetName(tag);
#else
    // This version of the code gives access to the name of the private tags
    DcmTag tagbis(element.getTag());
    const std::string tagName(tagbis.getTagName());      
#endif

    if (element.isLeaf())
    {
      Json::Value value(Json::objectValue);
      value["Name"] = tagName;

      if (tagbis.getPrivateCreator() != NULL)
      {
        value["PrivateCreator"] = tagbis.getPrivateCreator();
      }

      std::auto_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement(element));
      if (v->IsNull())
      {
        value["Type"] = "Null";
        value["Value"] = Json::nullValue;
      }
      else
      {
        std::string s = v->AsString();
        if (maxStringLength == 0 ||
            s.size() <= maxStringLength)
        {
          value["Type"] = "String";
          value["Value"] = s;
        }
        else
        {
          value["Type"] = "TooLong";
          value["Value"] = Json::nullValue;
        }
      }

      target[formattedTag] = value;
    }
    else
    {
      Json::Value children(Json::arrayValue);

      // "All subclasses of DcmElement except for DcmSequenceOfItems
      // are leaf nodes, while DcmSequenceOfItems, DcmItem, DcmDataset
      // etc. are not." The following cast is thus OK.
      DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(element);

      for (unsigned long i = 0; i < sequence.card(); i++)
      {
        DcmItem* child = sequence.getItem(i);
        Json::Value& v = children.append(Json::objectValue);
        StoreItem(v, *child, maxStringLength);
      }  

      target[formattedTag]["Name"] = tagName;
      target[formattedTag]["Type"] = "Sequence";
      target[formattedTag]["Value"] = children;
    }
  }


  void FromDcmtkBridge::ToJson(Json::Value& root, 
                               DcmDataset& dataset,
                               unsigned int maxStringLength)
  {
    StoreItem(root, dataset, maxStringLength);
  }



  void FromDcmtkBridge::ToJson(Json::Value& target, 
                               const std::string& path,
                               unsigned int maxStringLength)
  {
    DcmFileFormat dicom;
    if (!dicom.loadFile(path.c_str()).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else
    {
      FromDcmtkBridge::ToJson(target, *dicom.getDataset(), maxStringLength);
    }
  }


  static void ExtractPngImagePreview(std::string& result,
                                     DicomIntegerPixelAccessor& accessor)
  {
    PngWriter w;

    int32_t min, max;
    accessor.GetExtremeValues(min, max);

    std::vector<uint8_t> image(accessor.GetWidth() * accessor.GetHeight(), 0);
    if (min != max)
    {
      uint8_t* pixel = &image[0];
      for (unsigned int y = 0; y < accessor.GetHeight(); y++)
      {
        for (unsigned int x = 0; x < accessor.GetWidth(); x++, pixel++)
        {
          int32_t v = accessor.GetValue(x, y);
          *pixel = static_cast<uint8_t>(
            boost::math::lround(static_cast<float>(v - min) / 
                                static_cast<float>(max - min) * 255.0f));
        }
      }
    }

    w.WriteToMemory(result, accessor.GetWidth(), accessor.GetHeight(),
                    accessor.GetWidth(), PixelFormat_Grayscale8, &image[0]);
  }


  template <typename T>
  static void ExtractPngImageTruncate(std::string& result,
                                      DicomIntegerPixelAccessor& accessor,
                                      PixelFormat format)
  {
    PngWriter w;

    std::vector<T> image(accessor.GetWidth() * accessor.GetHeight(), 0);
    T* pixel = &image[0];
    for (unsigned int y = 0; y < accessor.GetHeight(); y++)
    {
      for (unsigned int x = 0; x < accessor.GetWidth(); x++, pixel++)
      {
        int32_t v = accessor.GetValue(x, y);
        if (v < std::numeric_limits<T>::min())
          *pixel = std::numeric_limits<T>::min();
        else if (v > std::numeric_limits<T>::max())
          *pixel = std::numeric_limits<T>::max();
        else
          *pixel = static_cast<T>(v);
      }
    }

    w.WriteToMemory(result, accessor.GetWidth(), accessor.GetHeight(),
                    accessor.GetWidth() * sizeof(T), format, &image[0]);
  }


  void FromDcmtkBridge::ExtractPngImage(std::string& result,
                                        DcmDataset& dataset,
                                        unsigned int frame,
                                        ImageExtractionMode mode)
  {
    // See also: http://support.dcmtk.org/wiki/dcmtk/howto/accessing-compressed-data

    std::auto_ptr<DicomIntegerPixelAccessor> accessor;

    DicomMap m;
    FromDcmtkBridge::Convert(m, dataset);

    DcmElement* e;
    if (dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_PIXEL_DATA), e).good() &&
        e != NULL)
    {
      Uint8* pixData = NULL;
      if (e->getUint8Array(pixData) == EC_Normal)
      {    
        accessor.reset(new DicomIntegerPixelAccessor(m, pixData, e->getLength()));
        accessor->SetCurrentFrame(frame);
      }
    }

    PixelFormat format;
    switch (mode)
    {
      case ImageExtractionMode_Preview:
      case ImageExtractionMode_UInt8:
        format = PixelFormat_Grayscale8;
        break;

      case ImageExtractionMode_UInt16:
        format = PixelFormat_Grayscale16;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    if (accessor.get() == NULL ||
        accessor->GetWidth() == 0 ||
        accessor->GetHeight() == 0)
    {
      PngWriter w;
      w.WriteToMemory(result, 0, 0, 0, format, NULL);
    }
    else
    {
      switch (mode)
      {
        case ImageExtractionMode_Preview:
          ExtractPngImagePreview(result, *accessor);
          break;

        case ImageExtractionMode_UInt8:
          ExtractPngImageTruncate<uint8_t>(result, *accessor, format);
          break;

        case ImageExtractionMode_UInt16:
          ExtractPngImageTruncate<uint16_t>(result, *accessor, format);
          break;

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }
  }


  void FromDcmtkBridge::ExtractPngImage(std::string& result,
                                        const std::string& dicomContent,
                                        unsigned int frame,
                                        ImageExtractionMode mode)
  {
    DcmInputBufferStream is;
    if (dicomContent.size() > 0)
    {
      is.setBuffer(&dicomContent[0], dicomContent.size());
    }
    is.setEos();

    DcmFileFormat dicom;
    if (dicom.read(is).good())
    {
      ExtractPngImage(result, *dicom.getDataset(), frame, mode);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }



  std::string FromDcmtkBridge::GetName(const DicomTag& t)
  {
    // Some patches for important tags because of different DICOM
    // dictionaries between DCMTK versions
    std::string n = t.GetMainTagsName();
    if (n.size() != 0)
    {
      return n;
    }
    // End of patches

#if 0
    DcmTagKey tag(t.GetGroup(), t.GetElement());
    const DcmDataDictionary& dict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = dict.findEntry(tag, NULL);

    std::string s(DcmTag_ERROR_TagName);
    if (entry != NULL)
    {
      s = std::string(entry->getTagName());
    }

    dcmDataDict.unlock();
    return s;
#else
    DcmTag tag(t.GetGroup(), t.GetElement());
    const char* name = tag.getTagName();
    if (name == NULL)
    {
      return DcmTag_ERROR_TagName;
    }
    else
    {
      return std::string(name);
    }
#endif
  }


  DicomTag FromDcmtkBridge::ParseTag(const char* name)
  {
    if (strlen(name) == 9 &&
        isxdigit(name[0]) &&
        isxdigit(name[1]) &&
        isxdigit(name[2]) &&
        isxdigit(name[3]) &&
        name[4] == '-' &&
        isxdigit(name[5]) &&
        isxdigit(name[6]) &&
        isxdigit(name[7]) &&
        isxdigit(name[8]))        
    {
      uint16_t group = GetTagValue(name);
      uint16_t element = GetTagValue(name + 5);
      return DicomTag(group, element);
    }

#if 0
    const DcmDataDictionary& dict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = dict.findEntry(name);

    if (entry == NULL)
    {
      dcmDataDict.unlock();
      throw OrthancException("Unknown DICOM tag");
    }
    else
    {
      DcmTagKey key = entry->getKey();
      DicomTag tag(key.getGroup(), key.getElement());
      dcmDataDict.unlock();
      return tag;
    }
#else
    DcmTag tag;
    if (DcmTag::findTagFromName(name, tag).good())
    {
      return DicomTag(tag.getGTag(), tag.getETag());
    }
    else
    {
      throw OrthancException("Unknown DICOM tag");
    }
#endif
  }


  void FromDcmtkBridge::Print(FILE* fp, const DicomMap& m)
  {
    for (DicomMap::Map::const_iterator 
           it = m.map_.begin(); it != m.map_.end(); it++)
    {
      DicomTag t = it->first;
      std::string s = it->second->AsString();
      printf("0x%04x 0x%04x (%s) [%s]\n", t.GetGroup(), t.GetElement(), GetName(t).c_str(), s.c_str());
    }
  }


  void FromDcmtkBridge::ToJson(Json::Value& result,
                               const DicomMap& values)
  {
    if (result.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    result.clear();

    for (DicomMap::Map::const_iterator 
           it = values.map_.begin(); it != values.map_.end(); it++)
    {
      result[GetName(it->first)] = it->second->AsString();
    }
  }


  std::string FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel level)
  {
    char uid[100];

    switch (level)
    {
      case DicomRootLevel_Patient:
        // The "PatientID" field is of type LO (Long String), 64
        // Bytes Maximum. An UUID is of length 36, thus it can be used
        // as a random PatientID.
        return Toolbox::GenerateUuid();

      case DicomRootLevel_Instance:
        return dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);

      case DicomRootLevel_Series:
        return dcmGenerateUniqueIdentifier(uid, SITE_SERIES_UID_ROOT);

      case DicomRootLevel_Study:
        return dcmGenerateUniqueIdentifier(uid, SITE_STUDY_UID_ROOT);

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  bool FromDcmtkBridge::SaveToMemoryBuffer(std::string& buffer,
                                           DcmDataset* dataSet)
  {
    // Determine the transfer syntax which shall be used to write the
    // information to the file. We always switch to the Little Endian
    // syntax, with explicit length.

    // http://support.dcmtk.org/docs/dcxfer_8h-source.html
    E_TransferSyntax xfer = EXS_LittleEndianExplicit;
    E_EncodingType encodingType = /*opt_sequenceType*/ EET_ExplicitLength;

    uint32_t s = dataSet->getLength(xfer, encodingType);

    buffer.resize(s);
    DcmOutputBufferStream ob(&buffer[0], s);

    dataSet->transferInit();

#if DCMTK_VERSION_NUMBER >= 360
    OFCondition c = dataSet->write(ob, xfer, encodingType, NULL,
                                   /*opt_groupLength*/ EGL_recalcGL,
                                   /*opt_paddingType*/ EPD_withoutPadding);
#else
    OFCondition c = dataSet->write(ob, xfer, encodingType, NULL);
#endif

    dataSet->transferEnd();
    if (c.good())
    {
      return true;
    }
    else
    {
      buffer.clear();
      return false;
    }

#if 0
    OFCondition cond = cbdata->dcmff->saveFile(fileName.c_str(), xfer, 
                                               encodingType, 
                                               /*opt_groupLength*/ EGL_recalcGL,
                                               /*opt_paddingType*/ EPD_withoutPadding,
                                               OFstatic_cast(Uint32, /*opt_filepad*/ 0), 
                                               OFstatic_cast(Uint32, /*opt_itempad*/ 0),
                                               (opt_useMetaheader) ? EWM_fileformat : EWM_dataset);
#endif
  }
}
