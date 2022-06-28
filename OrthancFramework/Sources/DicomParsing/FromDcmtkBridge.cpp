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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if !defined(DCMTK_VERSION_NUMBER)
#  error The macro DCMTK_VERSION_NUMBER must be defined
#endif

#include "FromDcmtkBridge.h"
#include "ToDcmtkBridge.h"
#include "../Compatibility.h"
#include "../Logging.h"
#include "../Toolbox.h"
#include "../OrthancException.h"

#if ORTHANC_SANDBOXED == 0
#  include "../TemporaryFile.h"
#endif

#include <list>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/join.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>

#include <dcmtk/dcmdata/dcvrae.h>
#include <dcmtk/dcmdata/dcvras.h>
#include <dcmtk/dcmdata/dcvrat.h>
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

#if DCMTK_VERSION_NUMBER >= 361
#  include <dcmtk/dcmdata/dcvruc.h>
#  include <dcmtk/dcmdata/dcvrur.h>
#endif

#if DCMTK_USE_EMBEDDED_DICTIONARIES == 1
#  include <OrthancFrameworkResources.h>
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
#  include <dcmtk/dcmjpeg/djdecode.h>
#  if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
#    include <dcmtk/dcmjpeg/djencode.h>
#  endif
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
#  include <dcmtk/dcmjpls/djdecode.h>
#  if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
#    include <dcmtk/dcmjpls/djencode.h>
#  endif
#endif


#include <dcmtk/dcmdata/dcrledrg.h>
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
#  include <dcmtk/dcmdata/dcrleerg.h>
#  include <dcmtk/dcmimage/diregist.h>  // include to support color images
#endif


static bool hasExternalDictionaries_ = false;


namespace Orthanc
{
  static bool IsBinaryTag(const DcmTag& key)
  {
    return (key.isUnknownVR() ||
            key.getEVR() == EVR_OB ||
            key.getEVR() == EVR_OW ||
            key.getEVR() == EVR_UN ||
            key.getEVR() == EVR_ox);
  }


#if DCMTK_USE_EMBEDDED_DICTIONARIES == 1
  static void LoadEmbeddedDictionary(DcmDataDictionary& dictionary,
                                     FrameworkResources::FileResourceId resource)
  {
    std::string content;
    FrameworkResources::GetFileResource(content, resource);

#if ORTHANC_SANDBOXED == 0
    TemporaryFile tmp;
    tmp.Write(content);

    if (!dictionary.loadDictionary(tmp.GetPath().c_str()))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot read embedded dictionary. Under Windows, make sure that " 
                             "your TEMP directory does not contain special characters.");
    }
#else
    if (!dictionary.loadFromMemory(content))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Cannot read embedded dictionary. Under Windows, make sure that " 
                             "your TEMP directory does not contain special characters.");
    }
#endif
  }
#endif


  namespace
  {
    class DictionaryLocker : public boost::noncopyable
    {
    private:
      DcmDataDictionary& dictionary_;

    public:
      DictionaryLocker() : dictionary_(dcmDataDict.wrlock())
      {
      }

      ~DictionaryLocker()
      {
#if DCMTK_VERSION_NUMBER >= 364
        dcmDataDict.wrunlock();
#else
        dcmDataDict.unlock();
#endif
      }

      DcmDataDictionary& operator*()
      {
        return dictionary_;
      }

      DcmDataDictionary* operator->()
      {
        return &dictionary_;
      }
    };

    
    ORTHANC_FORCE_INLINE
    static std::string FloatToString(float v)
    {
      /**
       * From "boost::lexical_cast" documentation: "For more involved
       * conversions, such as where precision or formatting need tighter
       * control than is offered by the default behavior of
       * lexical_cast, the conventional stringstream approach is
       * recommended."
       * https://www.boost.org/doc/libs/1_65_0/doc/html/boost_lexical_cast.html
       * http://www.gotw.ca/publications/mill19.htm
       *
       * The precision of 17 corresponds to "defaultRealPrecision" in JsonCpp:
       * https://github.com/open-source-parsers/jsoncpp/blob/master/include/json/value.h
       **/

      //return boost::lexical_cast<std::string>(v);  // This was used in Orthanc <= 1.9.0
      
      std::ostringstream ss;
      ss << std::setprecision(17) << v;
      return ss.str();
    }


    ORTHANC_FORCE_INLINE
    static std::string DoubleToString(double v)
    {
      //return boost::lexical_cast<std::string>(v);  // This was used in Orthanc <= 1.9.0
      
      std::ostringstream ss;
      ss << std::setprecision(17) << v;
      return ss.str();
    }

    
#define DCMTK_TO_CTYPE_CONVERTER(converter, cType, dcmtkType, getter, toStringFunction) \
                                                                        \
    struct converter                                                    \
    {                                                                   \
      typedef cType CType;                                              \
                                                                        \
      ORTHANC_FORCE_INLINE                                              \
        static bool Apply(CType& result,                                \
                          DcmElement& element,                          \
                          size_t i)                                     \
      {                                                                 \
        return dynamic_cast<dcmtkType&>(element).getter(result, i).good(); \
      }                                                                 \
                                                                        \
      ORTHANC_FORCE_INLINE                                              \
        static std::string ToString(CType value)                        \
      {                                                                 \
        return toStringFunction(value);                                 \
      }                                                                 \
    };

    DCMTK_TO_CTYPE_CONVERTER(DcmtkToSint32Converter, Sint32, DcmSignedLong, getSint32, boost::lexical_cast<std::string>)
    DCMTK_TO_CTYPE_CONVERTER(DcmtkToSint16Converter, Sint16, DcmSignedShort, getSint16, boost::lexical_cast<std::string>)
    DCMTK_TO_CTYPE_CONVERTER(DcmtkToUint32Converter, Uint32, DcmUnsignedLong, getUint32, boost::lexical_cast<std::string>)
    DCMTK_TO_CTYPE_CONVERTER(DcmtkToUint16Converter, Uint16, DcmUnsignedShort, getUint16, boost::lexical_cast<std::string>)
    DCMTK_TO_CTYPE_CONVERTER(DcmtkToFloat32Converter, Float32, DcmFloatingPointSingle, getFloat32, FloatToString)
    DCMTK_TO_CTYPE_CONVERTER(DcmtkToFloat64Converter, Float64, DcmFloatingPointDouble, getFloat64, DoubleToString)


    template <typename F>
    static DicomValue* ApplyDcmtkToCTypeConverter(DcmElement& element)
    {
      F f;
      typename F::CType value;

      if (element.getLength() > sizeof(typename F::CType)
          && (element.getLength() % sizeof(typename F::CType)) == 0)
      {
        size_t count = element.getLength() / sizeof(typename F::CType);
        std::vector<std::string> strings;
        for (size_t i = 0; i < count; i++) {
          if (f.Apply(value, element, i)) {
            strings.push_back(F::ToString(value));
          }
        }
        return new DicomValue(boost::algorithm::join(strings, "\\"), false);
      }
      else if (f.Apply(value, element, 0)) {
        return new DicomValue(F::ToString(value), false);
      }
      else {
        return new DicomValue;
      }
    }
  }


  void FromDcmtkBridge::InitializeDictionary(bool loadPrivateDictionary)
  {
    CLOG(INFO, DICOM) << "Using DCTMK version: " << DCMTK_VERSION_NUMBER;
    
#if DCMTK_USE_EMBEDDED_DICTIONARIES == 1
    {
      DictionaryLocker locker;

      locker->clear();

      CLOG(INFO, DICOM) << "Loading the embedded dictionaries";
      /**
       * Do not load DICONDE dictionary, it breaks the other tags. The
       * command "strace storescu 2>&1 |grep dic" shows that DICONDE
       * dictionary is not loaded by storescu.
       **/
      //LoadEmbeddedDictionary(*locker, FrameworkResources::DICTIONARY_DICONDE);

      LoadEmbeddedDictionary(*locker, FrameworkResources::DICTIONARY_DICOM);

      if (loadPrivateDictionary)
      {
        CLOG(INFO, DICOM) << "Loading the embedded dictionary of private tags";
        LoadEmbeddedDictionary(*locker, FrameworkResources::DICTIONARY_PRIVATE);
      }
      else
      {
        CLOG(INFO, DICOM) << "The dictionary of private tags has not been loaded";
      }
    }
#else
    {
      std::vector<std::string> dictionaries;
      
      const char* env = std::getenv(DCM_DICT_ENVIRONMENT_VARIABLE);
      if (env != NULL)
      {
        // This mimics the behavior of DCMTK:
        // https://support.dcmtk.org/docs/file_envvars.html
#if defined(_WIN32)
        Toolbox::TokenizeString(dictionaries, std::string(env), ';');
#else
        Toolbox::TokenizeString(dictionaries, std::string(env), ':');
#endif
      }
      else
      {
        boost::filesystem::path base = DCMTK_DICTIONARY_DIR;
        dictionaries.push_back((base / "dicom.dic").string());

        if (loadPrivateDictionary)
        {
          dictionaries.push_back((base / "private.dic").string());
        }
      }

      LoadExternalDictionaries(dictionaries);
      hasExternalDictionaries_ = false;  // Fix the side-effect of "LoadExternalDictionaries()"
    }
#endif

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
      throw OrthancException(ErrorCode_InternalError,
                             "No DICOM dictionary loaded, check environment variable: " +
                             std::string(DCM_DICT_ENVIRONMENT_VARIABLE));
    }

    {
      // Test the dictionary with a simple DICOM tag
      DcmTag key(0x0010, 0x1030); // This is PatientWeight
      if (key.getEVR() != EVR_DS)
      {
        throw OrthancException(ErrorCode_InternalError,
                               "The DICOM dictionary has not been correctly read");
      }
    }
  }


  void FromDcmtkBridge::LoadExternalDictionaries(const std::vector<std::string>& dictionaries)
  {
    DictionaryLocker locker;

    CLOG(INFO, DICOM) << "Clearing the DICOM dictionary";
    locker->clear();

    for (size_t i = 0; i < dictionaries.size(); i++)
    {
      LOG(WARNING) << "Loading external DICOM dictionary: \"" << dictionaries[i] << "\"";
        
      if (!locker->loadDictionary(dictionaries[i].c_str()))
      {
        throw OrthancException(ErrorCode_InexistentFile);
      }
    }    

    hasExternalDictionaries_ = true;
  }


  void FromDcmtkBridge::RegisterDictionaryTag(const DicomTag& tag,
                                              ValueRepresentation vr,
                                              const std::string& name,
                                              unsigned int minMultiplicity,
                                              unsigned int maxMultiplicity,
                                              const std::string& privateCreator)
  {
    if (minMultiplicity < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    bool arbitrary = false;
    if (maxMultiplicity == 0)
    {
      maxMultiplicity = DcmVariableVM;
      arbitrary = true;
    }
    else if (maxMultiplicity < minMultiplicity)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    DcmEVR evr = ToDcmtkBridge::Convert(vr);

    CLOG(INFO, DICOM) << "Registering tag in dictionary: (" << tag.Format() << ") "
                      << (DcmVR(evr).getValidVRName()) << " " 
                      << name << " (multiplicity: " << minMultiplicity << "-" 
                      << (arbitrary ? "n" : boost::lexical_cast<std::string>(maxMultiplicity)) << ")";

    std::unique_ptr<DcmDictEntry>  entry;
    if (privateCreator.empty())
    {
      if (tag.GetGroup() % 2 == 1)
      {
        char buf[128];
        sprintf(buf, "Warning: You are registering a private tag (%04x,%04x), "
                "but no private creator was associated with it", 
                tag.GetGroup(), tag.GetElement());
        LOG(WARNING) << buf;
      }

      entry.reset(new DcmDictEntry(tag.GetGroup(),
                                   tag.GetElement(),
                                   evr, name.c_str(),
                                   static_cast<int>(minMultiplicity),
                                   static_cast<int>(maxMultiplicity),
                                   NULL    /* version */,
                                   OFTrue  /* doCopyString */,
                                   NULL    /* private creator */));
    }
    else
    {
      // "Private Data Elements have an odd Group Number that is not
      // (0001,eeee), (0003,eeee), (0005,eeee), (0007,eeee), or
      // (FFFF,eeee)."
      if (tag.GetGroup() % 2 == 0 /* even */ ||
          tag.GetGroup() == 0x0001 ||
          tag.GetGroup() == 0x0003 ||
          tag.GetGroup() == 0x0005 ||
          tag.GetGroup() == 0x0007 ||
          tag.GetGroup() == 0xffff)
      {
        char buf[128];
        sprintf(buf, "Trying to register private tag (%04x,%04x), but it must have an odd group >= 0x0009",
                tag.GetGroup(), tag.GetElement());
        throw OrthancException(ErrorCode_ParameterOutOfRange, std::string(buf));
      }

      entry.reset(new DcmDictEntry(tag.GetGroup(),
                                   tag.GetElement(),
                                   evr, name.c_str(),
                                   static_cast<int>(minMultiplicity),
                                   static_cast<int>(maxMultiplicity),
                                   "private" /* version */,
                                   OFTrue    /* doCopyString */,
                                   privateCreator.c_str()));
    }

    entry->setGroupRangeRestriction(DcmDictRange_Unspecified);
    entry->setElementRangeRestriction(DcmDictRange_Unspecified);

    {
      DictionaryLocker locker;

      if (locker->findEntry(DcmTagKey(tag.GetGroup(), tag.GetElement()),
                            privateCreator.empty() ? NULL : privateCreator.c_str()))
      {
        throw OrthancException(ErrorCode_AlreadyExistingTag,
                               "Cannot register twice the tag (" + tag.Format() +
                               "), whose symbolic name is \"" + name + "\"");
      }
      else
      {
        locker->addEntry(entry.release());
      }
    }
  }


  Encoding FromDcmtkBridge::DetectEncoding(bool& hasCodeExtensions,
                                           DcmItem& dataset,
                                           Encoding defaultEncoding)
  {
    // http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.12.html#sect_C.12.1.1.2

    OFString tmp;
    if (dataset.findAndGetOFStringArray(DCM_SpecificCharacterSet, tmp).good())
    {
      std::vector<std::string> tokens;
      Toolbox::TokenizeString(tokens, std::string(tmp.c_str()), '\\');

      hasCodeExtensions = (tokens.size() > 1);

      for (size_t i = 0; i < tokens.size(); i++)
      {
        std::string characterSet = Toolbox::StripSpaces(tokens[i]);

        if (!characterSet.empty())
        {
          Encoding encoding;
          
          if (GetDicomEncoding(encoding, characterSet.c_str()))
          {
            // The specific character set is supported by the Orthanc core
            return encoding;
          }
          else
          {
            LOG(WARNING) << "Value of Specific Character Set (0008,0005) is not supported: " << characterSet
                         << ", fallback to ASCII (remove all special characters)";
            return Encoding_Ascii;
          }
        }
      }
    }
    else
    {
      hasCodeExtensions = false;
    }
    
    // No specific character set tag: Use the default encoding
    return defaultEncoding;
  }


  Encoding FromDcmtkBridge::DetectEncoding(DcmItem &dataset,
                                           Encoding defaultEncoding)
  {
    bool hasCodeExtensions;  // ignored
    return DetectEncoding(hasCodeExtensions, dataset, defaultEncoding);
  }


  void FromDcmtkBridge::ExtractDicomSummary(DicomMap& target,
                                            DcmItem& dataset,
                                            unsigned int maxStringLength,
                                            const std::set<DicomTag>& ignoreTagLength)
  {
    const Encoding defaultEncoding = GetDefaultDicomEncoding();
    
    bool hasCodeExtensions;
    Encoding encoding = DetectEncoding(hasCodeExtensions, dataset, defaultEncoding);

    target.Clear();
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element && element->isLeaf())
      {
        target.SetValueInternal(element->getTag().getGTag(),
                                element->getTag().getETag(),
                                ConvertLeafElement(*element, DicomToJsonFlags_Default,
                                                   maxStringLength, encoding, hasCodeExtensions, ignoreTagLength));
      }
      else
      {
        DcmSequenceOfItems* sequence = dynamic_cast<DcmSequenceOfItems*>(element);
        
        if (sequence)
        {
          Json::Value jsonSequence = Json::arrayValue;
          for (unsigned long i = 0; i < sequence->card(); i++)
          {
            DcmItem* child = sequence->getItem(i);
            Json::Value& v = jsonSequence.append(Json::objectValue);
            DatasetToJson(v, *child, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 
                          maxStringLength, encoding, hasCodeExtensions,
                          ignoreTagLength, 1);
          }

          target.SetValue(DicomTag(element->getTag().getGTag(), element->getTag().getETag()),
                          jsonSequence);
        }
      }
    }
  }


  DicomTag FromDcmtkBridge::Convert(const DcmTag& tag)
  {
    return DicomTag(tag.getGTag(), tag.getETag());
  }


  DicomTag FromDcmtkBridge::GetTag(const DcmElement& element)
  {
    return DicomTag(element.getGTag(), element.getETag());
  }


  static DicomValue* CreateValueFromUtf8String(const DicomTag& tag,
                                               const std::string& utf8,
                                               unsigned int maxStringLength,
                                               const std::set<DicomTag>& ignoreTagLength)
  {
    if (maxStringLength != 0 &&
        utf8.size() > maxStringLength &&
        ignoreTagLength.find(tag) == ignoreTagLength.end())
    {
      return new DicomValue;  // Too long, create a NULL value
    }
    else
    {
      return new DicomValue(utf8, false);
    }
  }


  DicomValue* FromDcmtkBridge::ConvertLeafElement(DcmElement& element,
                                                  DicomToJsonFlags flags,
                                                  unsigned int maxStringLength,
                                                  Encoding encoding,
                                                  bool hasCodeExtensions,
                                                  const std::set<DicomTag>& ignoreTagLength)
  {
    if (!element.isLeaf())
    {
      // This function is only applicable to leaf elements
      throw OrthancException(ErrorCode_BadParameterType);
    }

    {
      char *c = NULL;
      if (element.isaString() &&
          element.getString(c).good())
      {
        if (c == NULL)  // This case corresponds to the empty string
        {
          return new DicomValue("", false);
        }
        else
        {
          const std::string s(c);
          const std::string utf8 = Toolbox::ConvertToUtf8(s, encoding, hasCodeExtensions);
          return CreateValueFromUtf8String(GetTag(element), utf8, maxStringLength, ignoreTagLength);
        }
      }
    }


    if (element.getVR() == EVR_UN)
    {
      /**
       * Unknown value representation: Lookup in the dictionary. This
       * is notably the case for private tags registered with the
       * "Dictionary" configuration option, or for public tags with
       * "EVR_UN" in the case of Little Endian Implicit transfer
       * syntax (cf. DICOM CP 246).
       * ftp://medical.nema.org/medical/dicom/final/cp246_ft.pdf
       **/
      DictionaryLocker locker;
      
      const DcmDictEntry* entry = locker->findEntry(element.getTag().getXTag(), 
                                                    element.getTag().getPrivateCreator());
      if (entry != NULL && 
          entry->getVR().isaString())
      {
        Uint8* data = NULL;

        if (element.getUint8Array(data) == EC_Normal)
        {
          Uint32 length = element.getLength();

          if (data == NULL ||
              length == 0)
          {
            return new DicomValue("", false);   // Empty string
          }

          // Remove the trailing padding, if any
          if (length > 0 &&
              length % 2 == 0 &&
              data[length - 1] == '\0')
          {
            length = length - 1;
          }

          if (element.getTag().isPrivate())
          {
            // For private tags, we do not try and convert to UTF-8,
            // as nothing ensures that the encoding of the private tag
            // is the same as that of the remaining of the DICOM
            // dataset. Only go for ASCII strings.
            if (Toolbox::IsAsciiString(data, length))
            {
              const std::string s(reinterpret_cast<const char*>(data), length);
              return CreateValueFromUtf8String(GetTag(element), s, maxStringLength, ignoreTagLength);
            }
            else
            {
              // Not a plain ASCII string: Consider it as a binary
              // value that is handled in the switch-case below
            }
          }
          else
          {
            // For public tags, convert to UTF-8 by using the
            // "SpecificCharacterSet" tag, if present. This branch is
            // new in Orthanc 1.9.1 (cf. DICOM CP 246).
            const std::string s(reinterpret_cast<const char*>(data), length);
            const std::string utf8 = Toolbox::ConvertToUtf8(s, encoding, hasCodeExtensions);
            return CreateValueFromUtf8String(GetTag(element), utf8, maxStringLength, ignoreTagLength);
          }
        }
      }
    }

    
    try
    {
      // http://support.dcmtk.org/docs/dcvr_8h-source.html
      switch (element.getVR())
      {

        /**
         * Deal with binary data (including PixelData).
         **/

        case EVR_OB:  // other byte
        case EVR_OF:  // other float
        case EVR_OW:  // other word
        case EVR_UN:  // unknown value representation
        case EVR_ox:  // OB or OW depending on context
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
        case EVR_UNKNOWN: // used internally for elements with unknown VR (encoded with 4-byte length field in explicit VR)
        case EVR_UNKNOWN2B:  // used internally for elements with unknown VR with 2-byte length field in explicit VR
        {
          if (!(flags & DicomToJsonFlags_ConvertBinaryToNull))
          {
            Uint8* data = NULL;
            Uint16* data16 = NULL;
            if (element.getUint8Array(data) == EC_Normal)
            {
              return new DicomValue(reinterpret_cast<const char*>(data), element.getLength(), true);
            }
            else if (element.getUint16Array(data16) == EC_Normal)
            {
              return new DicomValue(reinterpret_cast<const char*>(data16), element.getLength(), true);
            }
          }

          return new DicomValue;
        }
    
        /**
         * Numeric types
         **/ 
      
        case EVR_SL:  // signed long
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToSint32Converter>(element);
        }

        case EVR_SS:  // signed short
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToSint16Converter>(element);
        }

        case EVR_UL:  // unsigned long
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToUint32Converter>(element);
        }

        case EVR_US:  // unsigned short
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToUint16Converter>(element);
        }

        case EVR_FL:  // float single-precision
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToFloat32Converter>(element);
        }

        case EVR_FD:  // float double-precision
        {
          return ApplyDcmtkToCTypeConverter<DcmtkToFloat64Converter>(element);
        }


        /**
         * Attribute tag.
         **/

        case EVR_AT:
        {
          DcmTagKey tag;
          if (dynamic_cast<DcmAttributeTag&>(element).getTagVal(tag, 0).good())
          {
            DicomTag t(tag.getGroup(), tag.getElement());
            return new DicomValue(t.Format(), false);
          }
          else
          {
            return new DicomValue;
          }
        }


        /**
         * Sequence types, should never occur at this point because of
         * "element.isLeaf()".
         **/

        case EVR_SQ:  // sequence of items
          return new DicomValue;


          /**
           * Internal to DCMTK.
           **/ 

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
        case EVR_PixelData:  // used internally for uncompressed pixeld data
        case EVR_OverlayData:  // used internally for overlay data
          return new DicomValue;


          /**
           * Default case.
           **/ 

        default:
          return new DicomValue;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      return new DicomValue;
    }
    catch (std::bad_cast&)
    {
      return new DicomValue;
    }
  }


  static Json::Value& PrepareNode(Json::Value& parent,
                                  DcmElement& element,
                                  DicomToJsonFormat format)
  {
    assert(parent.type() == Json::objectValue);

    DicomTag tag(FromDcmtkBridge::GetTag(element));
    const std::string formattedTag = tag.Format();

    if (format == DicomToJsonFormat_Short)
    {
      parent[formattedTag] = Json::nullValue;
      return parent[formattedTag];
    }

    // This code gives access to the name of the private tags
    std::string tagName = FromDcmtkBridge::GetTagName(element);
    
    switch (format)
    {
      case DicomToJsonFormat_Human:
        parent[tagName] = Json::nullValue;
        return parent[tagName];

      case DicomToJsonFormat_Full:
      {
        parent[formattedTag] = Json::objectValue;
        Json::Value& node = parent[formattedTag];

        if (element.isLeaf())
        {
          node["Name"] = tagName;

          if (element.getTag().getPrivateCreator() != NULL)
          {
            node["PrivateCreator"] = element.getTag().getPrivateCreator();
          }

          return node;
        }
        else
        {
          node["Name"] = tagName;
          node["Type"] = "Sequence";
          node["Value"] = Json::nullValue;
          return node["Value"];
        }
      }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void LeafValueToJson(Json::Value& target,
                              const DicomValue& value,
                              DicomToJsonFormat format,
                              DicomToJsonFlags flags,
                              unsigned int maxStringLength)
  {
    Json::Value* targetValue = NULL;
    Json::Value* targetType = NULL;

    switch (format)
    {
      case DicomToJsonFormat_Short:
      case DicomToJsonFormat_Human:
      {
        assert(target.type() == Json::nullValue);
        targetValue = &target;
        break;
      }      

      case DicomToJsonFormat_Full:
      {
        assert(target.type() == Json::objectValue);
        target["Value"] = Json::nullValue;
        target["Type"] = Json::nullValue;
        targetType = &target["Type"];
        targetValue = &target["Value"];
        break;
      }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    assert(targetValue != NULL);
    assert(targetValue->type() == Json::nullValue);
    assert(targetType == NULL || targetType->type() == Json::nullValue);

    if (value.IsNull())
    {
      if (targetType != NULL)
      {
        *targetType = "Null";
      }
    }
    else if (value.IsBinary())
    {
      if (flags & DicomToJsonFlags_ConvertBinaryToAscii)
      {
        *targetValue = Toolbox::ConvertToAscii(value.GetContent());
      }
      else
      {
        std::string s;
        value.FormatDataUriScheme(s);
        *targetValue = s;
      }

      if (targetType != NULL)
      {
        *targetType = "Binary";
      }
    }
    else if (maxStringLength == 0 ||
             value.GetContent().size() <= maxStringLength)
    {
      *targetValue = value.GetContent();

      if (targetType != NULL)
      {
        *targetType = "String";
      }
    }
    else
    {
      if (targetType != NULL)
      {
        *targetType = "TooLong";
      }
    }
  }                              


  void FromDcmtkBridge::ElementToJson(Json::Value& parent,
                                      DcmElement& element,
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength,
                                      Encoding encoding,
                                      bool hasCodeExtensions,
                                      const std::set<DicomTag>& ignoreTagLength,
                                      unsigned int depth)
  {
    if (parent.type() == Json::nullValue)
    {
      parent = Json::objectValue;
    }

    assert(parent.type() == Json::objectValue);
    Json::Value& target = PrepareNode(parent, element, format);

    if (element.isLeaf())
    {
      // The "0" below lets "LeafValueToJson()" take care of "TooLong" values
      std::unique_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement
                                    (element, flags, 0, encoding, hasCodeExtensions, ignoreTagLength));

      if (ignoreTagLength.find(GetTag(element)) == ignoreTagLength.end())
      {
        LeafValueToJson(target, *v, format, flags, maxStringLength);
      }
      else
      {
        LeafValueToJson(target, *v, format, flags, 0);
      }
    }
    else
    {
      assert(target.type() == Json::nullValue);
      target = Json::arrayValue;

      // "All subclasses of DcmElement except for DcmSequenceOfItems
      // are leaf nodes, while DcmSequenceOfItems, DcmItem, DcmDataset
      // etc. are not." The following dynamic_cast is thus OK.
      DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(element);

      for (unsigned long i = 0; i < sequence.card(); i++)
      {
        DcmItem* child = sequence.getItem(i);
        Json::Value& v = target.append(Json::objectValue);
        DatasetToJson(v, *child, format, flags, maxStringLength, encoding, hasCodeExtensions,
                      ignoreTagLength, depth + 1);
      }
    }
  }


  void FromDcmtkBridge::DatasetToJson(Json::Value& parent,
                                      DcmItem& item,
                                      DicomToJsonFormat format,
                                      DicomToJsonFlags flags,
                                      unsigned int maxStringLength,
                                      Encoding encoding,
                                      bool hasCodeExtensions,
                                      const std::set<DicomTag>& ignoreTagLength,
                                      unsigned int depth)
  {
    assert(parent.type() == Json::objectValue);

    for (unsigned long i = 0; i < item.card(); i++)
    {
      DcmElement* element = item.getElement(i);
      if (element == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      DicomTag tag(FromDcmtkBridge::Convert(element->getTag()));

      // New flag in Orthanc 1.9.1
      if (depth == 0 &&
          (flags & DicomToJsonFlags_StopAfterPixelData) &&
          tag > DICOM_TAG_PIXEL_DATA)
      {
        continue;
      }

      // New flag in Orthanc 1.9.1
      if ((flags & DicomToJsonFlags_SkipGroupLengths) &&
          tag.GetElement() == 0x0000)
      {
        continue;
      }

      /*element->getTag().isPrivate()*/
      if (tag.IsPrivate() &&
          !(flags & DicomToJsonFlags_IncludePrivateTags))    
      {
        continue;
      }

      if (!(flags & DicomToJsonFlags_IncludeUnknownTags))
      {
        DictionaryLocker locker;
        if (locker->findEntry(element->getTag(), element->getTag().getPrivateCreator()) == NULL)
        {
          continue;
        }
      }

      if (IsBinaryTag(element->getTag()))
      {
        // This is a binary tag
        if ((tag == DICOM_TAG_PIXEL_DATA && !(flags & DicomToJsonFlags_IncludePixelData)) ||
            (tag != DICOM_TAG_PIXEL_DATA && !(flags & DicomToJsonFlags_IncludeBinary)))
        {
          continue;
        }
      }

      FromDcmtkBridge::ElementToJson(parent, *element, format, flags, maxStringLength, encoding,
                                     hasCodeExtensions, ignoreTagLength, depth);
    }
  }


  void FromDcmtkBridge::ExtractDicomAsJson(Json::Value& target, 
                                           DcmDataset& dataset,
                                           DicomToJsonFormat format,
                                           DicomToJsonFlags flags,
                                           unsigned int maxStringLength,
                                           const std::set<DicomTag>& ignoreTagLength)
  {
    const Encoding defaultEncoding = GetDefaultDicomEncoding();
    
    bool hasCodeExtensions;
    Encoding encoding = DetectEncoding(hasCodeExtensions, dataset, defaultEncoding);

    target = Json::objectValue;
    DatasetToJson(target, dataset, format, flags, maxStringLength, encoding, hasCodeExtensions, ignoreTagLength, 0);
  }


  void FromDcmtkBridge::ExtractHeaderAsJson(Json::Value& target, 
                                            DcmMetaInfo& dataset,
                                            DicomToJsonFormat format,
                                            DicomToJsonFlags flags,
                                            unsigned int maxStringLength)
  {
    std::set<DicomTag> ignoreTagLength;
    target = Json::objectValue;
    DatasetToJson(target, dataset, format, flags, maxStringLength, Encoding_Ascii, false, ignoreTagLength, 0);
  }


  static std::string GetTagNameInternal(DcmTag& tag)
  {
    if (!hasExternalDictionaries_)
    {
      /**
       * Some patches for important tags because of different DICOM
       * dictionaries between DCMTK versions. Since Orthanc 1.9.4, we
       * don't apply these patches if external dictionaries are
       * loaded, notably for compatibility with DICONDE. In Orthanc <=
       * 1.9.3, this was done by method "DicomTag::GetMainTagsName()".
       **/
      
      DicomTag tmp(tag.getGroup(), tag.getElement());

      if (tmp == DICOM_TAG_ACCESSION_NUMBER)
        return "AccessionNumber";

      if (tmp == DICOM_TAG_SOP_INSTANCE_UID)
        return "SOPInstanceUID";

      if (tmp == DICOM_TAG_PATIENT_ID)
        return "PatientID";

      if (tmp == DICOM_TAG_SERIES_INSTANCE_UID)
        return "SeriesInstanceUID";

      if (tmp == DICOM_TAG_STUDY_INSTANCE_UID)
        return "StudyInstanceUID"; 

      if (tmp == DICOM_TAG_PIXEL_DATA)
        return "PixelData";

      if (tmp == DICOM_TAG_IMAGE_INDEX)
        return "ImageIndex";

      if (tmp == DICOM_TAG_INSTANCE_NUMBER)
        return "InstanceNumber";

      if (tmp == DICOM_TAG_NUMBER_OF_SLICES)
        return "NumberOfSlices";

      if (tmp == DICOM_TAG_NUMBER_OF_FRAMES)
        return "NumberOfFrames";

      if (tmp == DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)
        return "CardiacNumberOfImages";

      if (tmp == DICOM_TAG_IMAGES_IN_ACQUISITION)
        return "ImagesInAcquisition";

      if (tmp == DICOM_TAG_PATIENT_NAME)
        return "PatientName";

      if (tmp == DICOM_TAG_IMAGE_POSITION_PATIENT)
        return "ImagePositionPatient";

      if (tmp == DICOM_TAG_IMAGE_ORIENTATION_PATIENT)
        return "ImageOrientationPatient";

      // New in Orthanc 1.6.0, as tagged as "RETIRED_" since DCMTK 3.6.4
      if (tmp == DICOM_TAG_OTHER_PATIENT_IDS)
        return "OtherPatientIDs";

      // End of patches
    }

#if 0
    // This version explicitly calls the dictionary
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


  std::string FromDcmtkBridge::GetTagName(const DicomTag& t,
                                          const std::string& privateCreator)
  {
    DcmTag tag(t.GetGroup(), t.GetElement());

    if (!privateCreator.empty())
    {
      tag.setPrivateCreator(privateCreator.c_str());
    }

    return GetTagNameInternal(tag);
  }


  std::string FromDcmtkBridge::GetTagName(const DcmElement& element)
  {
    // Copy the tag to ensure const-correctness of DcmElement. Note
    // that the private creator information is also copied.
    DcmTag tag(element.getTag());  

    return GetTagNameInternal(tag);
  }

  std::string FromDcmtkBridge::GetTagName(const DicomElement &element)
  {
    return GetTagName(element.GetTag(), "");
  }



  DicomTag FromDcmtkBridge::ParseTag(const char* name)
  {
    DicomTag parsed(0, 0);
    if (DicomTag::ParseHexadecimal(parsed, name))
    {
      return parsed;
    }

#if 0
    const DcmDataDictionary& dict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = dict.findEntry(name);

    if (entry == NULL)
    {
      dcmDataDict.unlock();
      throw OrthancException(ErrorCode_UnknownDicomTag);
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
      CLOG(INFO, DICOM) << "Unknown DICOM tag: \"" << name << "\"";
      throw OrthancException(ErrorCode_UnknownDicomTag, name, false);
    }
#endif
  }

  DicomTag FromDcmtkBridge::ParseTag(const std::string &name)
  {
    return ParseTag(name.c_str());
  }

  bool FromDcmtkBridge::HasTag(const DicomMap &fields, const std::string &tagName)
  {
    return fields.HasTag(ParseTag(tagName));
  }

  void FromDcmtkBridge::FormatListOfTags(std::string& output, const std::set<DicomTag>& tags)
  {
    std::set<std::string> values;
    for (std::set<DicomTag>::const_iterator it = tags.begin();
         it != tags.end(); ++it)
    {
      values.insert(it->Format());
    }

    Toolbox::JoinStrings(output, values, ";");
  }

  void FromDcmtkBridge::FormatListOfTags(Json::Value& output, const std::set<DicomTag>& tags)
  {
    output = Json::arrayValue;
    for (std::set<DicomTag>::const_iterator it = tags.begin();
         it != tags.end(); ++it)
    {
      output.append(it->Format());
    }
  }

  // parses a list like "0010,0010;PatientBirthDate;0020,0020"
  void FromDcmtkBridge::ParseListOfTags(std::set<DicomTag>& result, const std::string& source)
  {
    result.clear();

    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, source, ';');

    for (std::vector<std::string>::const_iterator it = tokens.begin();
         it != tokens.end(); ++it)
    {
      if (it->size() > 0)
      {
        DicomTag tag = FromDcmtkBridge::ParseTag(*it);
        result.insert(tag);
      }
    }
  }


  void FromDcmtkBridge::ParseListOfTags(std::set<DicomTag>& result, const Json::Value& source)
  {
    result.clear();

    if (!source.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest, "List of tags is not an array");
    }

    for (Json::ArrayIndex i = 0; i < source.size(); i++)
    {
      const std::string& value = source[i].asString();
      DicomTag tag = FromDcmtkBridge::ParseTag(value);
      result.insert(tag);
    }
  }

  const DicomValue &FromDcmtkBridge::GetValue(const DicomMap &fields,
                                              const std::string &tagName)
  {
    return fields.GetValue(ParseTag(tagName));
  }

  void FromDcmtkBridge::SetValue(DicomMap &target,
                                 const std::string &tagName,
                                 DicomValue *value)
  {
    const DicomTag tag = ParseTag(tagName);
    target.SetValueInternal(tag.GetGroup(), tag.GetElement(), value);
  }


  bool FromDcmtkBridge::IsUnknownTag(const DicomTag& tag)
  {
    DcmTag tmp(tag.GetGroup(), tag.GetElement());
    return tmp.isUnknownVR();
  }


  void FromDcmtkBridge::ToJson(Json::Value& result,
                               const DicomMap& values,
                               DicomToJsonFormat format)
  {
    if (result.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    result.clear();

    for (DicomMap::Content::const_iterator 
           it = values.content_.begin(); it != values.content_.end(); ++it)
    {
      switch (format)
      {
        case DicomToJsonFormat_Human:
        {
          // TODO Inject PrivateCreator if some is available in the DicomMap?
          const std::string tagName = GetTagName(it->first, "");

          if (it->second->IsNull())
          {
            result[tagName] = Json::nullValue;
          }
          else if (it->second->IsSequence())
          {
            result[tagName] = Json::arrayValue;
            const Json::Value& jsonSequence = it->second->GetSequenceContent();

            for (Json::Value::ArrayIndex i = 0; i < jsonSequence.size(); ++i)
            {
              Json::Value target = Json::objectValue;
              Toolbox::SimplifyDicomAsJson(target, jsonSequence[i], DicomToJsonFormat_Human);
              result[tagName].append(target);
            }
          }
          else
          {
            // TODO IsBinary
            result[tagName] = it->second->GetContent();
          }
          break;
        }

        case DicomToJsonFormat_Full:
        {
          // TODO Inject PrivateCreator if some is available in the DicomMap?
          const std::string tagName = GetTagName(it->first, "");

          Json::Value value = Json::objectValue;

          value["Name"] = tagName;

          if (it->second->IsNull())
          {
            value["Type"] = "Null";
            value["Value"] = Json::nullValue;
          }
          else if (it->second->IsSequence())
          {
            value["Type"] = "Sequence";
            value["Value"] = it->second->GetSequenceContent();
          }
          else
          {
            // TODO IsBinary
            value["Type"] = "String";
            value["Value"] = it->second->GetContent();
          }

          result[it->first.Format()] = value;
          break;
        }

        case DicomToJsonFormat_Short:
        {
          const std::string hex = it->first.Format();

          if (it->second->IsNull())
          {
            result[hex] = Json::nullValue;
          }
          else if (it->second->IsSequence())
          {
            result[hex] = Json::arrayValue;
            const Json::Value& jsonSequence = it->second->GetSequenceContent();

            for (Json::Value::ArrayIndex i = 0; i < jsonSequence.size(); ++i)
            {
              Json::Value target = Json::objectValue;
              Toolbox::SimplifyDicomAsJson(target, jsonSequence[i], DicomToJsonFormat_Short);
              result[hex].append(target);
            }
          }
          else
          {
            // TODO IsBinary
            result[hex] = it->second->GetContent();
          }

          break;
        }

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }


  std::string FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType level)
  {
    char uid[100];

    switch (level)
    {
      case ResourceType_Patient:
        // The "PatientID" field is of type LO (Long String), 64
        // Bytes Maximum. An UUID is of length 36, thus it can be used
        // as a random PatientID.
        return Toolbox::GenerateUuid();

      case ResourceType_Instance:
        return dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);

      case ResourceType_Series:
        return dcmGenerateUniqueIdentifier(uid, SITE_SERIES_UID_ROOT);

      case ResourceType_Study:
        return dcmGenerateUniqueIdentifier(uid, SITE_STUDY_UID_ROOT);

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  
  static bool SaveToMemoryBufferInternal(std::string& buffer,
                                         DcmFileFormat& dicom,
                                         E_TransferSyntax xfer)
  {
    E_EncodingType encodingType = /*opt_sequenceType*/ EET_ExplicitLength;

    // Create a memory buffer with the proper size
    {
      const uint32_t estimatedSize = dicom.calcElementLength(xfer, encodingType);  // (*)
      buffer.resize(estimatedSize);
    }

    DcmOutputBufferStream ob(&buffer[0], buffer.size());

    // Fill the memory buffer with the meta-header and the dataset
    dicom.transferInit();
    OFCondition c = dicom.write(ob, xfer, encodingType, NULL,
                                /*opt_groupLength*/ EGL_recalcGL,
                                /*opt_paddingType*/ EPD_noChange,
                                /*padlen*/ 0, /*subPadlen*/ 0, /*instanceLength*/ 0,
                                EWM_updateMeta /* creates new SOP instance UID on lossy */);
    dicom.transferEnd();

    if (c.good())
    {
      // The DICOM file is successfully written, truncate the target
      // buffer if its size was overestimated by (*)
      ob.flush();

      size_t effectiveSize = static_cast<size_t>(ob.tell());
      if (effectiveSize < buffer.size())
      {
        buffer.resize(effectiveSize);
      }

      return true;
    }
    else
    {
      // Error
      buffer.clear();
      return false;
    }
  }
  

  bool FromDcmtkBridge::SaveToMemoryBuffer(std::string& buffer,
                                           DcmDataset& dataSet)
  {
    // Determine the transfer syntax which shall be used to write the
    // information to the file. If not possible, switch to the Little
    // Endian syntax, with explicit length.

    // http://support.dcmtk.org/docs/dcxfer_8h-source.html


    /**
     * Note that up to Orthanc 0.7.1 (inclusive), the
     * "EXS_LittleEndianExplicit" was always used to save the DICOM
     * dataset into memory. We now keep the original transfer syntax
     * (if available).
     **/
    E_TransferSyntax xfer = dataSet.getCurrentXfer();
    if (xfer == EXS_Unknown)
    {
      // No information about the original transfer syntax: This is
      // most probably a DICOM dataset that was read from memory.
      xfer = EXS_LittleEndianExplicit;
    }

    // Create the meta-header information
    DcmFileFormat ff(&dataSet);
    ff.validateMetaInfo(xfer);
    ff.removeInvalidGroups();

    return SaveToMemoryBufferInternal(buffer, ff, xfer);
  }


  bool FromDcmtkBridge::Transcode(DcmFileFormat& dicom,
                                  DicomTransferSyntax syntax,
                                  const DcmRepresentationParameter* representation)
  {
    E_TransferSyntax xfer;
    if (!LookupDcmtkTransferSyntax(xfer, syntax))
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      DicomTransferSyntax sourceSyntax;
      bool known = LookupOrthancTransferSyntax(sourceSyntax, dicom);
      
      if (!dicom.chooseRepresentation(xfer, representation).good() ||
          !dicom.canWriteXfer(xfer) ||
          !dicom.validateMetaInfo(xfer, EWM_updateMeta).good())
      {
        return false;
      }
      else
      {
        dicom.removeInvalidGroups();

        if (known)
        {
          CLOG(INFO, DICOM) << "Transcoded an image from transfer syntax "
                            << GetTransferSyntaxUid(sourceSyntax) << " to "
                            << GetTransferSyntaxUid(syntax);
        }
        else
        {
          CLOG(INFO, DICOM) << "Transcoded an image from unknown transfer syntax to "
                            << GetTransferSyntaxUid(syntax);
        }
        
        return true;
      }
    }
  }


  ValueRepresentation FromDcmtkBridge::LookupValueRepresentation(const DicomTag& tag)
  {
    DcmTag t(tag.GetGroup(), tag.GetElement());
    return Convert(t.getEVR());
  }

  ValueRepresentation FromDcmtkBridge::Convert(const DcmEVR vr)
  {
    switch (vr)
    {
      case EVR_AE:
        return ValueRepresentation_ApplicationEntity;

      case EVR_AS:
        return ValueRepresentation_AgeString;

      case EVR_AT:
        return ValueRepresentation_AttributeTag;

      case EVR_CS:
        return ValueRepresentation_CodeString;

      case EVR_DA:
        return ValueRepresentation_Date;

      case EVR_DS:
        return ValueRepresentation_DecimalString;

      case EVR_DT:
        return ValueRepresentation_DateTime;

      case EVR_FL:
        return ValueRepresentation_FloatingPointSingle;

      case EVR_FD:
        return ValueRepresentation_FloatingPointDouble;

      case EVR_IS:
        return ValueRepresentation_IntegerString;

      case EVR_LO:
        return ValueRepresentation_LongString;

      case EVR_LT:
        return ValueRepresentation_LongText;

      case EVR_OB:
        return ValueRepresentation_OtherByte;

#if DCMTK_VERSION_NUMBER >= 361
      case EVR_OD:
        return ValueRepresentation_OtherDouble;
#endif

      case EVR_OF:
        return ValueRepresentation_OtherFloat;

#if DCMTK_VERSION_NUMBER >= 362
      case EVR_OL:
        return ValueRepresentation_OtherLong;
#endif

      case EVR_OW:
        return ValueRepresentation_OtherWord;

      case EVR_PN:
        return ValueRepresentation_PersonName;

      case EVR_SH:
        return ValueRepresentation_ShortString;

      case EVR_SL:
        return ValueRepresentation_SignedLong;

      case EVR_SQ:
        return ValueRepresentation_Sequence;

      case EVR_SS:
        return ValueRepresentation_SignedShort;

      case EVR_ST:
        return ValueRepresentation_ShortText;

      case EVR_TM:
        return ValueRepresentation_Time;

#if DCMTK_VERSION_NUMBER >= 361
      case EVR_UC:
        return ValueRepresentation_UnlimitedCharacters;
#endif

      case EVR_UI:
        return ValueRepresentation_UniqueIdentifier;

      case EVR_UL:
        return ValueRepresentation_UnsignedLong;

      case EVR_UN:
        return ValueRepresentation_Unknown;

#if DCMTK_VERSION_NUMBER >= 361
      case EVR_UR:
        return ValueRepresentation_UniversalResource;
#endif

      case EVR_US:
        return ValueRepresentation_UnsignedShort;

      case EVR_UT:
        return ValueRepresentation_UnlimitedText;

      default:
        return ValueRepresentation_NotSupported;
    }
  }


  DcmElement* FromDcmtkBridge::CreateElementForTag(const DicomTag& tag,
                                                   const std::string& privateCreator)
  {
    if (tag.IsPrivate() &&
        privateCreator.empty())
    {
      // This solves issue 140 (Modifying private tags with REST API
      // changes VR from LO to UN)
      // https://bugs.orthanc-server.com/show_bug.cgi?id=140
      LOG(WARNING) << "Private creator should not be empty while creating a private tag: " << tag.Format();
    }
    
#if DCMTK_VERSION_NUMBER >= 362
    DcmTag key(tag.GetGroup(), tag.GetElement());
    if (tag.IsPrivate())
    {
      return DcmItem::newDicomElement(key, privateCreator.c_str());
    }
    else
    {
      return DcmItem::newDicomElement(key, NULL);
    }
    
#else
    DcmTag key(tag.GetGroup(), tag.GetElement());
    if (tag.IsPrivate())
    {
      // https://forum.dcmtk.org/viewtopic.php?t=4527
      LOG(WARNING) << "You are using DCMTK <= 3.6.1: All the private tags "
        "are considered as having a binary value representation";
      key.setPrivateCreator(privateCreator.c_str());
      return new DcmOtherByteOtherWord(key);
    }
    else
    {
      return newDicomElement(key);
    }
#endif      
  }



  void FromDcmtkBridge::FillElementWithString(DcmElement& element,
                                              const std::string& utf8Value,
                                              bool decodeDataUriScheme,
                                              Encoding dicomEncoding)
  {
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
    else if (dicomEncoding != Encoding_Utf8)
    {
      binary = Toolbox::ConvertFromUtf8(utf8Value, dicomEncoding);
      decoded = &binary;
    }

    if (IsBinaryTag(element.getTag()))
    {
      bool ok;

      switch (element.getTag().getEVR())
      {
        case EVR_OW:
          if (decoded->size() % sizeof(Uint16) != 0)
          {
            LOG(ERROR) << "A tag with OW VR must have an even number of bytes";
            ok = false;
          }
          else
          {
            ok = element.putUint16Array((const Uint16*) decoded->c_str(), decoded->size() / sizeof(Uint16)).good();
          }
          
          break;
      
        default:
          ok = element.putUint8Array((const Uint8*) decoded->c_str(), decoded->size()).good();
          break;
      }
      
      if (ok)
      {
        return;
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    bool ok = false;
    
    try
    {
      switch (element.getTag().getEVR())
      {
        // http://support.dcmtk.org/docs/dcvr_8h-source.html

        /**
         * TODO.
         **/

        case EVR_OB:  // other byte
        case EVR_OW:  // other word
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
#if DCMTK_VERSION_NUMBER >= 361
        case EVR_UC:  // unlimited characters
        case EVR_UR:  // URI/URL
#endif
        {
          ok = element.putString(decoded->c_str()).good();
          break;
        }

        
        /**
         * Numerical types
         **/ 
      
        case EVR_SL:  // signed long
        {
          ok = element.putSint32(boost::lexical_cast<Sint32>(*decoded)).good();
          break;
        }

        case EVR_SS:  // signed short
        {
          ok = element.putSint16(boost::lexical_cast<Sint16>(*decoded)).good();
          break;
        }

        case EVR_UL:  // unsigned long
#if DCMTK_VERSION_NUMBER >= 362
        case EVR_OL:  // other long (requires byte-swapping)
#endif
        {
          ok = element.putUint32(boost::lexical_cast<Uint32>(*decoded)).good();
          break;
        }

        case EVR_xs: // unsigned short, signed short or multiple values
        {
          if (decoded->find('\\') != std::string::npos)
          {
            ok = element.putString(decoded->c_str()).good();
          }
          else if (decoded->find('-') != std::string::npos)
          {
            ok = element.putSint16(boost::lexical_cast<Sint16>(*decoded)).good();
          }
          else
          {
            ok = element.putUint16(boost::lexical_cast<Uint16>(*decoded)).good();  
          }
          break;
        }

        case EVR_US:  // unsigned short
        {
          ok = element.putUint16(boost::lexical_cast<Uint16>(*decoded)).good();
          break;
        }

        case EVR_FL:  // float single-precision
        case EVR_OF:  // other float (requires byte swapping)
        {
          ok = element.putFloat32(boost::lexical_cast<float>(*decoded)).good();
          break;
        }

        case EVR_FD:  // float double-precision
#if DCMTK_VERSION_NUMBER >= 361
        case EVR_OD:  // other double (requires byte-swapping)
#endif
        {
          ok = element.putFloat64(boost::lexical_cast<double>(*decoded)).good();
          break;
        }


        /**
         * Other types
         **/
        
        case EVR_AT:  // attribute tag, new in Orthanc 1.9.4
        {
          DicomTag value = ParseTag(utf8Value);
          ok = element.putTagVal(DcmTagKey(value.GetGroup(), value.GetElement())).good();
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
      DicomTag tag(element.getTag().getGroup(), element.getTag().getElement());
      throw OrthancException(ErrorCode_BadFileFormat,
                             "While creating a DICOM instance, tag (" + tag.Format() +
                             ") has out-of-range value: \"" + (*decoded) + "\"");
    }
  }


  DcmElement* FromDcmtkBridge::FromJson(const DicomTag& tag,
                                        const Json::Value& value,
                                        bool decodeDataUriScheme,
                                        Encoding dicomEncoding,
                                        const std::string& privateCreator)
  {
    std::unique_ptr<DcmElement> element;

    switch (value.type())
    {
      case Json::stringValue:
        element.reset(CreateElementForTag(tag, privateCreator));
        FillElementWithString(*element, value.asString(), decodeDataUriScheme, dicomEncoding);
        break;

      case Json::nullValue:
        element.reset(CreateElementForTag(tag, privateCreator));
        FillElementWithString(*element, "", decodeDataUriScheme, dicomEncoding);
        break;

      case Json::arrayValue:
      {
        const char* p = NULL;
        if (tag.IsPrivate() &&
            !privateCreator.empty())
        {
          p = privateCreator.c_str();
        }
        
        DcmTag key(tag.GetGroup(), tag.GetElement(), p);
        if (key.getEVR() != EVR_SQ)
        {
          throw OrthancException(ErrorCode_BadParameterType,
                                 "Bad Parameter type for tag " + tag.Format());
        }

        DcmSequenceOfItems* sequence = new DcmSequenceOfItems(key);
        element.reset(sequence);
        
        for (Json::Value::ArrayIndex i = 0; i < value.size(); i++)
        {
          std::unique_ptr<DcmItem> item(new DcmItem);

          switch (value[i].type())
          {
            case Json::objectValue:
            {
              Json::Value::Members members = value[i].getMemberNames();
              for (Json::Value::ArrayIndex j = 0; j < members.size(); j++)
              {
                item->insert(FromJson(ParseTag(members[j]), value[i][members[j]], decodeDataUriScheme, dicomEncoding, privateCreator));
              }
              break;
            }

            case Json::arrayValue:
            {
              // Lua cannot disambiguate between an empty dictionary
              // and an empty array
              if (value[i].size() != 0)
              {
                throw OrthancException(ErrorCode_BadParameterType);
              }
              break;
            }

            default:
              throw OrthancException(ErrorCode_BadParameterType);
          }

          sequence->append(item.release());
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_BadParameterType, "Bad Parameter type for tag " + tag.Format());
    }

    return element.release();
  }


  DcmPixelSequence* FromDcmtkBridge::GetPixelSequence(DcmDataset& dataset)
  {
    DcmElement *element = NULL;
    if (!dataset.findAndGetElement(DCM_PixelData, element).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    DcmPixelData& pixelData = dynamic_cast<DcmPixelData&>(*element);

    E_TransferSyntax repType;
    const DcmRepresentationParameter *repParam = NULL;
    pixelData.getCurrentRepresentationKey(repType, repParam);
    
    DcmPixelSequence* pixelSequence = NULL;
    if (!pixelData.getEncapsulatedRepresentation(repType, repParam, pixelSequence).good())
    {
      return NULL;
    }
    else
    {
      return pixelSequence;
    }
  }


  Encoding FromDcmtkBridge::ExtractEncoding(const Json::Value& json,
                                            Encoding defaultEncoding)
  {
    if (json.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    Encoding encoding = defaultEncoding;

    const Json::Value::Members tags = json.getMemberNames();
    
    // Look for SpecificCharacterSet (0008,0005) in the JSON file
    for (size_t i = 0; i < tags.size(); i++)
    {
      DicomTag tag = FromDcmtkBridge::ParseTag(tags[i]);
      if (tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        const Json::Value& value = json[tags[i]];
        if (value.type() != Json::stringValue ||
            (value.asString().length() != 0 &&
             !GetDicomEncoding(encoding, value.asCString())))
        {
          throw OrthancException(ErrorCode_BadRequest,
                                 "Unknown encoding while creating DICOM from JSON: " +
                                 value.toStyledString());
        }

        if (value.asString().length() == 0)
        {
          return defaultEncoding;
        }
      }
    }

    return encoding;
  } 


  static void SetString(DcmDataset& target,
                        const DcmTag& tag,
                        const std::string& value)
  {
    if (!target.putAndInsertString(tag, value.c_str()).good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  DcmDataset* FromDcmtkBridge::FromJson(const Json::Value& json,  // Encoded using UTF-8
                                        bool generateIdentifiers,
                                        bool decodeDataUriScheme,
                                        Encoding defaultEncoding,
                                        const std::string& privateCreator)
  {
    std::unique_ptr<DcmDataset> result(new DcmDataset);
    Encoding encoding = ExtractEncoding(json, defaultEncoding);

    SetString(*result, DCM_SpecificCharacterSet, GetDicomSpecificCharacterSet(encoding));

    const Json::Value::Members tags = json.getMemberNames();
    
    bool hasPatientId = false;
    bool hasStudyInstanceUid = false;
    bool hasSeriesInstanceUid = false;
    bool hasSopInstanceUid = false;

    for (size_t i = 0; i < tags.size(); i++)
    {
      DicomTag tag = FromDcmtkBridge::ParseTag(tags[i]);
      const Json::Value& value = json[tags[i]];

      if (tag == DICOM_TAG_PATIENT_ID)
      {
        hasPatientId = true;
      }
      else if (tag == DICOM_TAG_STUDY_INSTANCE_UID)
      {
        hasStudyInstanceUid = true;
      }
      else if (tag == DICOM_TAG_SERIES_INSTANCE_UID)
      {
        hasSeriesInstanceUid = true;
      }
      else if (tag == DICOM_TAG_SOP_INSTANCE_UID)
      {
        hasSopInstanceUid = true;
      }

      if (tag != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        std::unique_ptr<DcmElement> element(FromDcmtkBridge::FromJson(tag, value, decodeDataUriScheme, encoding, privateCreator));

        result->findAndDeleteElement(element->getTag());

        DcmElement* tmp = element.release();
        if (!result->insert(tmp, false, false).good())
        {
          delete tmp;
          throw OrthancException(ErrorCode_InternalError);
        }
      }
    }

    if (!hasPatientId &&
        generateIdentifiers)
    {
      SetString(*result, DCM_PatientID, GenerateUniqueIdentifier(ResourceType_Patient));
    }

    if (!hasStudyInstanceUid &&
        generateIdentifiers)
    {
      SetString(*result, DCM_StudyInstanceUID, GenerateUniqueIdentifier(ResourceType_Study));
    }

    if (!hasSeriesInstanceUid &&
        generateIdentifiers)
    {
      SetString(*result, DCM_SeriesInstanceUID, GenerateUniqueIdentifier(ResourceType_Series));
    }

    if (!hasSopInstanceUid &&
        generateIdentifiers)
    {
      SetString(*result, DCM_SOPInstanceUID, GenerateUniqueIdentifier(ResourceType_Instance));
    }

    return result.release();
  }


  DcmFileFormat* FromDcmtkBridge::LoadFromMemoryBuffer(const void* buffer,
                                                       size_t size)
  {
    DcmInputBufferStream is;
    if (size > 0)
    {
      is.setBuffer(buffer, size);
    }
    is.setEos();

    std::unique_ptr<DcmFileFormat> result(new DcmFileFormat);

    result->transferInit();

    /**
     * New in Orthanc 1.6.0: The "size" is given as an argument to the
     * "read()" method. This can avoid huge memory consumption if
     * parsing an invalid DICOM file, which can notably been observed
     * by executing the integration test "test_upload_compressed" on
     * valgrind running Orthanc.
     **/
    if (!result->read(is, EXS_Unknown, EGL_noChange, size).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Cannot parse an invalid DICOM file (size: " +
                             boost::lexical_cast<std::string>(size) + " bytes)");
    }

    result->loadAllDataIntoMemory();
    result->transferEnd();

    return result.release();
  }


  void FromDcmtkBridge::FromJson(DicomMap& target,
                                 const Json::Value& source,
                                 const char* fieldName)
  {
    if (source.type() != Json::objectValue)
    {
      if (fieldName != NULL) 
      {
        throw OrthancException(ErrorCode_BadFileFormat, std::string("Expecting an object in field '") + std::string(fieldName) + std::string("'"));
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Expecting an object");
      }
    }

    target.Clear();

    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& value = source[members[i]];

      if (value.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat, std::string("Expecting a string in field '") + members[i] + std::string("'"));
      }
      
      target.SetValue(ParseTag(members[i]), value.asString(), false);
    }
  }




  void FromDcmtkBridge::ChangeStringEncoding(DcmItem& dataset,
                                             Encoding source,
                                             bool hasSourceCodeExtensions,
                                             Encoding target)
  {
    // Recursive exploration of a dataset to change the encoding of
    // each string-like element

    if (source == target)
    {
      return;
    }

    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element)
      {
        if (element->isLeaf())
        {
          char *c = NULL;
          if (element->isaString() &&
              element->getString(c).good() && 
              c != NULL)
          {
            std::string a = Toolbox::ConvertToUtf8(c, source, hasSourceCodeExtensions);
            std::string b = Toolbox::ConvertFromUtf8(a, target);
            element->putString(b.c_str());
          }
        }
        else
        {
          // "All subclasses of DcmElement except for DcmSequenceOfItems
          // are leaf nodes, while DcmSequenceOfItems, DcmItem, DcmDataset
          // etc. are not." The following dynamic_cast is thus OK.
          DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(*element);

          for (unsigned long j = 0; j < sequence.card(); j++)
          {
            ChangeStringEncoding(*sequence.getItem(j), source, hasSourceCodeExtensions, target);
          }
        }
      }
    }
  }


  void FromDcmtkBridge::InitializeCodecs()
  {
#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    CLOG(INFO, DICOM) << "Registering JPEG Lossless codecs in DCMTK";
    DJLSDecoderRegistration::registerCodecs();
# if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DJLSEncoderRegistration::registerCodecs();
# endif
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    CLOG(INFO, DICOM) << "Registering JPEG codecs in DCMTK";
    DJDecoderRegistration::registerCodecs(); 
# if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DJEncoderRegistration::registerCodecs();
# endif
#endif

    CLOG(INFO, DICOM) << "Registering RLE codecs in DCMTK";
    DcmRLEDecoderRegistration::registerCodecs(); 
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DcmRLEEncoderRegistration::registerCodecs();
#endif
  }


  void FromDcmtkBridge::FinalizeCodecs()
  {
#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    // Unregister JPEG-LS codecs
    DJLSDecoderRegistration::cleanup();
# if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DJLSEncoderRegistration::cleanup();
# endif
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    // Unregister JPEG codecs
    DJDecoderRegistration::cleanup();
# if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DJEncoderRegistration::cleanup();
# endif
#endif

    DcmRLEDecoderRegistration::cleanup(); 
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DcmRLEEncoderRegistration::cleanup();
#endif
  }



  // Forward declaration
  static bool ApplyVisitorToElement(DcmElement& element,
                                    ITagVisitor& visitor,
                                    const std::vector<DicomTag>& parentTags,
                                    const std::vector<size_t>& parentIndexes,
                                    Encoding encoding,
                                    bool hasCodeExtensions);
 
  static void ApplyVisitorToDataset(DcmItem& dataset,
                                    ITagVisitor& visitor,
                                    const std::vector<DicomTag>& parentTags,
                                    const std::vector<size_t>& parentIndexes,
                                    Encoding encoding,
                                    bool hasCodeExtensions)
  {
    assert(parentTags.size() == parentIndexes.size());

    std::set<DcmTagKey> toRemove;
    
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        if (!ApplyVisitorToElement(*element, visitor, parentTags, parentIndexes, encoding, hasCodeExtensions))
        {
          toRemove.insert(element->getTag());
        }
      }      
    }

    // Remove all the tags that were planned for removal (cf. ITagVisitor::Action_Remove)
    for (std::set<DcmTagKey>::const_iterator
           it = toRemove.begin(); it != toRemove.end(); ++it)
    {
      std::unique_ptr<DcmElement> tmp(dataset.remove(*it));
    }
  }


  // Returns "true" iff the element must be kept. If "false" is
  // returned, the element will be removed.
  static bool ApplyVisitorToLeaf(DcmElement& element,
                                 ITagVisitor& visitor,
                                 const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 Encoding encoding,
                                 bool hasCodeExtensions)
  {
    // TODO - Merge this function, that is more recent, with ConvertLeafElement()

    assert(element.isLeaf());

    DcmEVR evr = element.getTag().getEVR();

    
    /**
     * Fix the EVR for types internal to DCMTK 
     **/

    if (evr == EVR_ox)  // OB or OW depending on context
    {
      evr = EVR_OB;
    }

    if (evr == EVR_UNKNOWN ||  // used internally for elements with unknown VR (encoded with 4-byte length field in explicit VR)
        evr == EVR_UNKNOWN2B)  // used internally for elements with unknown VR with 2-byte length field in explicit VR
    {
      evr = EVR_UN;
    }

    if (evr == EVR_UN)
    {
      // New in Orthanc 1.9.5
      DictionaryLocker locker;
      
      const DcmDictEntry* entry = locker->findEntry(element.getTag().getXTag(),
                                                    element.getTag().getPrivateCreator());

      if (entry != NULL)
      {
        evr = entry->getEVR();
      }
    }

    const ValueRepresentation vr = FromDcmtkBridge::Convert(evr);

    
    /**
     * Deal with binary data (including PixelData).
     **/

    if (evr == EVR_OB ||  // other byte
        evr == EVR_OW ||  // other word
        evr == EVR_UN)    // unknown value representation
    {
      Uint16* data16 = NULL;
      Uint8* data = NULL;

      ITagVisitor::Action action;
      
      if ((element.getTag() == DCM_PixelData ||  // (*) New in Orthanc 1.9.1
           evr == EVR_OW) &&
          element.getUint16Array(data16) == EC_Normal)
      {
        action = visitor.VisitBinary(parentTags, parentIndexes, tag, vr, data16, element.getLength());
      }
      else if (evr != EVR_OW &&
               element.getUint8Array(data) == EC_Normal)
      {
        /**
         * WARNING: The call to "getUint8Array()" crashes
         * (segmentation fault) on big-endian architectures if applied
         * to pixel data, during the call to "swapIfNecessary()" in
         * "DcmPolymorphOBOW::getUint8Array()" (this method is not
         * reimplemented in derived class "DcmPixelData"). However,
         * "getUint16Array()" works correctly, hence (*).
         **/
        action = visitor.VisitBinary(parentTags, parentIndexes, tag, vr, data, element.getLength());
      }
      else
      {
        action = visitor.VisitNotSupported(parentTags, parentIndexes, tag, vr);
      }

      switch (action)
      {
        case ITagVisitor::Action_None:
          return true;  // We're done

        case ITagVisitor::Action_Remove:
          return false;

        case ITagVisitor::Action_Replace:
          throw OrthancException(ErrorCode_NotImplemented, "Iterator cannot replace binary data");

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    /**
     * Deal with plain strings (and convert them to UTF-8)
     **/

    char *c = NULL;
    if (element.isaString() &&
        element.getString(c).good())
    {
      std::string utf8;

      if (c != NULL)  // This case corresponds to the empty string
      {
        if (element.getTag() == DCM_SpecificCharacterSet)
        {
          utf8.assign(c);
        }
        else
        {
          std::string s(c);
          utf8 = Toolbox::ConvertToUtf8(s, encoding, hasCodeExtensions);
        }
      }

      std::string newValue;
      ITagVisitor::Action action = visitor.VisitString
        (newValue, parentTags, parentIndexes, tag, vr, utf8);

      switch (action)
      {
        case ITagVisitor::Action_None:
          return true;

        case ITagVisitor::Action_Remove:
          return false;

        case ITagVisitor::Action_Replace:
        {
          std::string s = Toolbox::ConvertFromUtf8(newValue, encoding);
          if (element.putString(s.c_str()) != EC_Normal)
          {
            throw OrthancException(ErrorCode_InternalError,
                                   "Iterator cannot replace value of tag: " + tag.Format());
          }

          return true;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }


    ITagVisitor::Action action;
    
    try
    {
      // http://support.dcmtk.org/docs/dcvr_8h-source.html
      switch (evr)
      {

        /**
         * Plain string values.
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
          Uint8* data = NULL;
          
          if (element.getUint8Array(data) == EC_Normal)
          {
            const Uint32 length = element.getLength();
            Uint32 l = 0;
            while (l < length &&
                   data[l] != 0)
            {
              l++;
            }

            std::string ignored;
            std::string s(reinterpret_cast<const char*>(data), l);
            action = visitor.VisitString(ignored, parentTags, parentIndexes, tag, vr,
                                         Toolbox::ConvertToUtf8(s, encoding, hasCodeExtensions));
          }
          else
          {
            action = visitor.VisitNotSupported(parentTags, parentIndexes, tag, vr);
          }

          if (action == ITagVisitor::Action_Replace)
          {
            LOG(WARNING) << "Iterator cannot replace this string tag: "
                         << FromDcmtkBridge::GetTagName(element)
                         << " (" << tag.Format() << ")";
            return true;
          }

          break;
        }
    
        /**
         * Numeric types
         **/ 
      
        case EVR_SL:  // signed long
        {
          DcmSignedLong& content = dynamic_cast<DcmSignedLong&>(element);

          std::vector<int64_t> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Sint32 f;
            if (content.getSint32(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitIntegers(parentTags, parentIndexes, tag, vr, values);
          break;
        }

        case EVR_SS:  // signed short
        {
          DcmSignedShort& content = dynamic_cast<DcmSignedShort&>(element);

          std::vector<int64_t> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Sint16 f;
            if (content.getSint16(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitIntegers(parentTags, parentIndexes, tag, vr, values);
          break;
        }

        case EVR_UL:  // unsigned long
#if DCMTK_VERSION_NUMBER >= 362
        case EVR_OL:
#endif
        {
          DcmUnsignedLong& content = dynamic_cast<DcmUnsignedLong&>(element);

          std::vector<int64_t> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Uint32 f;
            if (content.getUint32(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitIntegers(parentTags, parentIndexes, tag, vr, values);
          break;
        }

        case EVR_US:  // unsigned short
        {
          DcmUnsignedShort& content = dynamic_cast<DcmUnsignedShort&>(element);

          std::vector<int64_t> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Uint16 f;
            if (content.getUint16(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitIntegers(parentTags, parentIndexes, tag, vr, values);
          break;
        }

        case EVR_FL:  // float single-precision
        case EVR_OF:
        {
          DcmFloatingPointSingle& content = dynamic_cast<DcmFloatingPointSingle&>(element);

          std::vector<double> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Float32 f;
            if (content.getFloat32(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitDoubles(parentTags, parentIndexes, tag, vr, values);
          break;
        }

        case EVR_FD:  // float double-precision
#if DCMTK_VERSION_NUMBER >= 361
        case EVR_OD:
#endif
        {
          DcmFloatingPointDouble& content = dynamic_cast<DcmFloatingPointDouble&>(element);

          std::vector<double> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            Float64 f;
            if (content.getFloat64(f, i).good())
            {
              values.push_back(f);
            }
          }

          action = visitor.VisitDoubles(parentTags, parentIndexes, tag, vr, values);
          break;
        }


        /**
         * Attribute tag.
         **/

        case EVR_AT:
        {
          DcmAttributeTag& content = dynamic_cast<DcmAttributeTag&>(element);

          std::vector<DicomTag> values;
          values.reserve(content.getVM());

          for (unsigned long i = 0; i < content.getVM(); i++)
          {
            DcmTagKey f;
            if (content.getTagVal(f, i).good())
            {
              DicomTag t(f.getGroup(), f.getElement());
              values.push_back(t);
            }
          }

          assert(vr == ValueRepresentation_AttributeTag);
          action = visitor.VisitAttributes(parentTags, parentIndexes, tag, values);
          break;
        }


        /**
         * Sequence types, should never occur at this point because of
         * "element.isLeaf()".
         **/

        case EVR_SQ:  // sequence of items
        {
          return true;
        }
        
        
        /**
         * Internal to DCMTK.
         **/ 

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
        case EVR_PixelData:  // used internally for uncompressed pixeld data
        case EVR_OverlayData:  // used internally for overlay data
        {
          action = visitor.VisitNotSupported(parentTags, parentIndexes, tag, vr);
          break;
        }
        

        /**
         * Default case.
         **/ 

        default:
          return true;
      }

      switch (action)
      {
        case ITagVisitor::Action_None:
          return true;  // We're done

        case ITagVisitor::Action_Remove:
          return false;

        case ITagVisitor::Action_Replace:
          throw OrthancException(ErrorCode_NotImplemented, "Iterator cannot replace non-string-like data");

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      return true;
    }
    catch (std::bad_cast&)
    {
      return true;
    }
  }


  // Returns "true" iff the element must be kept. If "false" is
  // returned, the element will be removed.
  static bool ApplyVisitorToElement(DcmElement& element,
                                    ITagVisitor& visitor,
                                    const std::vector<DicomTag>& parentTags,
                                    const std::vector<size_t>& parentIndexes,
                                    Encoding encoding,
                                    bool hasCodeExtensions)
  {
    assert(parentTags.size() == parentIndexes.size());

    DicomTag tag(FromDcmtkBridge::Convert(element.getTag()));

    if (element.isLeaf())
    {
      return ApplyVisitorToLeaf(element, visitor, parentTags, parentIndexes, tag, encoding, hasCodeExtensions);
    }
    else
    {
      // "All subclasses of DcmElement except for DcmSequenceOfItems
      // are leaf nodes, while DcmSequenceOfItems, DcmItem, DcmDataset
      // etc. are not." The following dynamic_cast is thus OK.
      DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(element);

      ITagVisitor::Action action = visitor.VisitSequence(parentTags, parentIndexes, tag, sequence.card());

      switch (action)
      {
        case ITagVisitor::Action_None:
          if (sequence.card() != 0)  // Minor optimization to avoid creating "tags" and "indexes" if not needed
          {
            std::vector<DicomTag> tags = parentTags;
            std::vector<size_t> indexes = parentIndexes;
            tags.push_back(tag);
            indexes.push_back(0);

            for (unsigned long i = 0; i < sequence.card(); i++)
            {
              indexes.back() = static_cast<size_t>(i);
              DcmItem* child = sequence.getItem(i);
              ApplyVisitorToDataset(*child, visitor, tags, indexes, encoding, hasCodeExtensions);
            }
          }

          return true;  // Keep

        case ITagVisitor::Action_Remove:
          return false;

        case ITagVisitor::Action_Replace:
          throw OrthancException(ErrorCode_NotImplemented, "Iterator cannot replace sequences");

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

    }
  }


  void FromDcmtkBridge::Apply(DcmItem& dataset,
                              ITagVisitor& visitor,
                              Encoding defaultEncoding)
  {
    std::vector<DicomTag> parentTags;
    std::vector<size_t> parentIndexes;
    bool hasCodeExtensions;
    Encoding encoding = DetectEncoding(hasCodeExtensions, dataset, defaultEncoding);
    ApplyVisitorToDataset(dataset, visitor, parentTags, parentIndexes, encoding, hasCodeExtensions);
  }



  bool FromDcmtkBridge::LookupOrthancTransferSyntax(DicomTransferSyntax& target,
                                                    DcmFileFormat& dicom)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      return LookupOrthancTransferSyntax(target, *dicom.getDataset());
    }
  }


  bool FromDcmtkBridge::LookupOrthancTransferSyntax(DicomTransferSyntax& target,
                                                    DcmDataset& dataset)
  {
    E_TransferSyntax xfer = dataset.getCurrentXfer();
    if (xfer == EXS_Unknown)
    {
      dataset.updateOriginalXfer();
      xfer = dataset.getOriginalXfer();
      if (xfer == EXS_Unknown)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Cannot determine the transfer syntax of the DICOM instance");
      }
    }

    return FromDcmtkBridge::LookupOrthancTransferSyntax(target, xfer);
  }


  void FromDcmtkBridge::LogMissingTagsForStore(DcmDataset& dicom)
  {
    std::string patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid;

    const char* c = NULL;
    if (dicom.findAndGetString(DCM_PatientID, c).good() &&
        c != NULL)
    {
      patientId.assign(c);
    }

    c = NULL;
    if (dicom.findAndGetString(DCM_StudyInstanceUID, c).good() &&
        c != NULL)
    {
      studyInstanceUid.assign(c);
    }

    c = NULL;
    if (dicom.findAndGetString(DCM_SeriesInstanceUID, c).good() &&
        c != NULL)
    {
      seriesInstanceUid.assign(c);
    }

    c = NULL;
    if (dicom.findAndGetString(DCM_SOPInstanceUID, c).good() &&
        c != NULL)
    {
      sopInstanceUid.assign(c);
    }
    
    DicomMap::LogMissingTagsForStore(patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid);
  }


  void FromDcmtkBridge::IDicomPathVisitor::ApplyInternal(FromDcmtkBridge::IDicomPathVisitor& visitor,
                                                         DcmItem& item,
                                                         const DicomPath& pattern,
                                                         const DicomPath& actualPath)
  {
    const size_t level = actualPath.GetPrefixLength();
      
    if (level == pattern.GetPrefixLength())
    {
      visitor.Visit(item, actualPath);
    }
    else
    {
      assert(level < pattern.GetPrefixLength());

      const DicomTag& tmp = pattern.GetPrefixTag(level);
      DcmTagKey tag(tmp.GetGroup(), tmp.GetElement());

      DcmSequenceOfItems *sequence = NULL;
      if (item.findAndGetSequence(tag, sequence).good() &&
          sequence != NULL)
      {
        for (unsigned long i = 0; i < sequence->card(); i++)
        {
          if (pattern.IsPrefixUniversal(level) ||
              pattern.GetPrefixIndex(level) == static_cast<size_t>(i))
          {
            DcmItem *child = sequence->getItem(i);
            if (child != NULL)
            {
              DicomPath childPath = actualPath;
              childPath.AddIndexedTagToPrefix(pattern.GetPrefixTag(level), static_cast<size_t>(i));
              
              ApplyInternal(visitor, *child, pattern, childPath);
            }
          }
        }
      }
    }
  }


  void FromDcmtkBridge::IDicomPathVisitor::Apply(IDicomPathVisitor& visitor,
                                                 DcmDataset& dataset,
                                                 const DicomPath& path)
  {
    DicomPath actualPath(path.GetFinalTag());
    ApplyInternal(visitor, dataset, path, actualPath);
  }


  void FromDcmtkBridge::RemovePath(DcmDataset& dataset,
                                   const DicomPath& path)
  {
    class Visitor : public FromDcmtkBridge::IDicomPathVisitor
    {
    public:
      virtual void Visit(DcmItem& item,
                         const DicomPath& path) ORTHANC_OVERRIDE
      {
        DcmTagKey key(path.GetFinalTag().GetGroup(), path.GetFinalTag().GetElement());
        std::unique_ptr<DcmElement> removed(item.remove(key));
      }
    };
    
    Visitor visitor;
    IDicomPathVisitor::Apply(visitor, dataset, path);
  }
  

  void FromDcmtkBridge::ClearPath(DcmDataset& dataset,
                                  const DicomPath& path,
                                  bool onlyIfExists)
  {
    class Visitor : public FromDcmtkBridge::IDicomPathVisitor
    {
    public:
      bool  onlyIfExists_;
      
    public:
      explicit Visitor(bool onlyIfExists) :
        onlyIfExists_(onlyIfExists)
      {
      }
      
      virtual void Visit(DcmItem& item,
                         const DicomPath& path) ORTHANC_OVERRIDE
      {
        DcmTagKey key(path.GetFinalTag().GetGroup(), path.GetFinalTag().GetElement());

        if (onlyIfExists_ &&
            !item.tagExists(key))
        {
          // The tag is non-existing, do not clear it
        }
        else
        {
          if (!item.insertEmptyElement(key, OFTrue /* replace old value */).good())
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }
      }
    };
    
    Visitor visitor(onlyIfExists);
    IDicomPathVisitor::Apply(visitor, dataset, path);
  }
  

  void FromDcmtkBridge::ReplacePath(DcmDataset& dataset,
                                    const DicomPath& path,
                                    const DcmElement& element,
                                    DicomReplaceMode mode)
  {
    class Visitor : public FromDcmtkBridge::IDicomPathVisitor
    {
    private:
      std::unique_ptr<DcmElement> element_;
      DicomReplaceMode            mode_;
    
    public:
      Visitor(const DcmElement& element,
              DicomReplaceMode mode) :
        element_(dynamic_cast<DcmElement*>(element.clone())),
        mode_(mode)
      {
        if (element_.get() == NULL)
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot clone DcmElement");
        }
      }
    
      virtual void Visit(DcmItem& item,
                         const DicomPath& path) ORTHANC_OVERRIDE
      {
        std::unique_ptr<DcmElement> cloned(dynamic_cast<DcmElement*>(element_->clone()));
        if (cloned.get() == NULL)
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot clone DcmElement");
        }
        else
        {      
          DcmTagKey key(path.GetFinalTag().GetGroup(), path.GetFinalTag().GetElement());

          if (!item.tagExists(key))
          {
            switch (mode_)
            {
              case DicomReplaceMode_InsertIfAbsent:
                break;  // Fine, we can proceed with insertion
                
              case DicomReplaceMode_ThrowIfAbsent:
                throw OrthancException(ErrorCode_InexistentItem, "Cannot replace inexistent tag: " + GetTagName(*element_));
                
              case DicomReplaceMode_IgnoreIfAbsent:
                return;  // Don't proceed with insertion
                
              default:
                throw OrthancException(ErrorCode_ParameterOutOfRange);
            }
          }
          
          if (!item.insert(cloned.release(), OFTrue /* replace old */).good())
          {
            throw OrthancException(ErrorCode_InternalError, "Cannot replace an element: " + GetTagName(*element_));
          }
        }
      }
    };

    DcmTagKey key(path.GetFinalTag().GetGroup(), path.GetFinalTag().GetElement());
  
    if (element.getTag() != key)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "The final tag must be the same as the tag of the element during a replacement");
    }
    else
    {
      Visitor visitor(element, mode);
      IDicomPathVisitor::Apply(visitor, dataset, path);
    }
  }


  bool FromDcmtkBridge::LookupSequenceItem(DicomMap& target,
                                           DcmDataset& dataset,
                                           const DicomPath& path,
                                           size_t sequenceIndex)
  {
    class Visitor : public FromDcmtkBridge::IDicomPathVisitor
    {
    private:
      bool       found_;
      DicomMap&  target_;
      size_t     sequenceIndex_;
      
    public:
      Visitor(DicomMap& target,
              size_t sequenceIndex) :
        found_(false),
        target_(target),
        sequenceIndex_(sequenceIndex)
      {
      }
      
      virtual void Visit(DcmItem& item,
                         const DicomPath& path) ORTHANC_OVERRIDE
      {
        DcmTagKey tag(path.GetFinalTag().GetGroup(), path.GetFinalTag().GetElement());

        DcmSequenceOfItems *sequence = NULL;
        
        if (item.findAndGetSequence(tag, sequence).good() &&
            sequence != NULL &&
            sequenceIndex_ < sequence->card())
        {
          std::set<DicomTag> ignoreTagLength;
          ExtractDicomSummary(target_, *sequence->getItem(sequenceIndex_), 0, ignoreTagLength);
          found_ = true;
        }
      }

      bool HasFound() const
      {
        return found_;
      }
    };

    Visitor visitor(target, sequenceIndex);
    IDicomPathVisitor::Apply(visitor, dataset, path);
    return visitor.HasFound();
  }


  bool FromDcmtkBridge::LookupStringValue(std::string& target,
                                          DcmDataset& dataset,
                                          const DicomTag& key)
  {
    DcmTagKey dcmkey(key.GetGroup(), key.GetElement());
    
    const char* str = NULL;
    const Uint8* data = NULL;
    unsigned long size = 0;

    if (dataset.findAndGetString(dcmkey, str).good() &&
        str != NULL)
    {
      target.assign(str);
      return true;
    }
    else if (dataset.findAndGetUint8Array(dcmkey, data, &size).good() &&
             data != NULL &&
             size > 0)
    {
      /**
       * This special case is necessary for borderline DICOM files
       * that have DICOM tags have the "UN" value representation. New
       * in Orthanc 1.10.1.
       * https://groups.google.com/g/orthanc-users/c/86fobx3ZyoM/m/KBG17un6AQAJ
       **/
      unsigned long l = 0;
      while (l < size &&
             data[l] != 0)
      {
        l++;
      }

      target.assign(reinterpret_cast<const char*>(data), l);
      return true;
    }
    else
    {
      return false;
    }
  }
}


#include "./FromDcmtkBridge_TransferSyntaxes.impl.h"
