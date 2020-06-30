/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomWebJsonVisitor.h"

#include "../OrthancException.h"
#include "../Toolbox.h"
#include "FromDcmtkBridge.h"

#include <boost/math/special_functions/round.hpp>
#include <boost/lexical_cast.hpp>


static const char* const KEY_ALPHABETIC = "Alphabetic";
static const char* const KEY_IDEOGRAPHIC = "Ideographic";
static const char* const KEY_PHONETIC = "Phonetic";
static const char* const KEY_BULK_DATA_URI = "BulkDataURI";
static const char* const KEY_INLINE_BINARY = "InlineBinary";
static const char* const KEY_SQ = "SQ";
static const char* const KEY_TAG = "tag";
static const char* const KEY_VALUE = "Value";
static const char* const KEY_VR = "vr";


namespace Orthanc
{
#if ORTHANC_ENABLE_PUGIXML == 1
  static void DecomposeXmlPersonName(pugi::xml_node& target,
                                     const std::string& source)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, source, '^');

    if (tokens.size() >= 1)
    {
      target.append_child("FamilyName").text() = tokens[0].c_str();
    }
            
    if (tokens.size() >= 2)
    {
      target.append_child("GivenName").text() = tokens[1].c_str();
    }
            
    if (tokens.size() >= 3)
    {
      target.append_child("MiddleName").text() = tokens[2].c_str();
    }
            
    if (tokens.size() >= 4)
    {
      target.append_child("NamePrefix").text() = tokens[3].c_str();
    }
            
    if (tokens.size() >= 5)
    {
      target.append_child("NameSuffix").text() = tokens[4].c_str();
    }
  }
  
  static void ExploreXmlDataset(pugi::xml_node& target,
                                const Json::Value& source)
  {
    // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.3.html#table_F.3.1-1
    assert(source.type() == Json::objectValue);

    Json::Value::Members members = source.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const DicomTag tag = FromDcmtkBridge::ParseTag(members[i]);
      const Json::Value& content = source[members[i]];

      assert(content.type() == Json::objectValue &&
             content.isMember(KEY_VR) &&
             content[KEY_VR].type() == Json::stringValue);
      const std::string vr = content[KEY_VR].asString();

      const std::string keyword = FromDcmtkBridge::GetTagName(tag, "");
    
      pugi::xml_node node = target.append_child("DicomAttribute");
      node.append_attribute(KEY_TAG).set_value(members[i].c_str());
      node.append_attribute(KEY_VR).set_value(vr.c_str());

      if (keyword != std::string(DcmTag_ERROR_TagName))
      {
        node.append_attribute("keyword").set_value(keyword.c_str());
      }   

      if (content.isMember(KEY_VALUE))
      {
        assert(content[KEY_VALUE].type() == Json::arrayValue);
        
        for (Json::Value::ArrayIndex j = 0; j < content[KEY_VALUE].size(); j++)
        {
          std::string number = boost::lexical_cast<std::string>(j + 1);

          if (vr == "SQ")
          {
            if (content[KEY_VALUE][j].type() == Json::objectValue)
            {
              pugi::xml_node child = node.append_child("Item");
              child.append_attribute("number").set_value(number.c_str());
              ExploreXmlDataset(child, content[KEY_VALUE][j]);
            }
          }
          if (vr == "PN")
          {
            bool hasAlphabetic = (content[KEY_VALUE][j].isMember(KEY_ALPHABETIC) &&
                                  content[KEY_VALUE][j][KEY_ALPHABETIC].type() == Json::stringValue);

            bool hasIdeographic = (content[KEY_VALUE][j].isMember(KEY_IDEOGRAPHIC) &&
                                   content[KEY_VALUE][j][KEY_IDEOGRAPHIC].type() == Json::stringValue);

            bool hasPhonetic = (content[KEY_VALUE][j].isMember(KEY_PHONETIC) &&
                                content[KEY_VALUE][j][KEY_PHONETIC].type() == Json::stringValue);

            if (hasAlphabetic ||
                hasIdeographic ||
                hasPhonetic)
            {
              pugi::xml_node child = node.append_child("PersonName");
              child.append_attribute("number").set_value(number.c_str());

              if (hasAlphabetic)
              {
                pugi::xml_node name = child.append_child(KEY_ALPHABETIC);
                DecomposeXmlPersonName(name, content[KEY_VALUE][j][KEY_ALPHABETIC].asString());
              }

              if (hasIdeographic)
              {
                pugi::xml_node name = child.append_child(KEY_IDEOGRAPHIC);
                DecomposeXmlPersonName(name, content[KEY_VALUE][j][KEY_IDEOGRAPHIC].asString());
              }

              if (hasPhonetic)
              {
                pugi::xml_node name = child.append_child(KEY_PHONETIC);
                DecomposeXmlPersonName(name, content[KEY_VALUE][j][KEY_PHONETIC].asString());
              }
            }
          }
          else
          {
            pugi::xml_node child = node.append_child("Value");
            child.append_attribute("number").set_value(number.c_str());

            switch (content[KEY_VALUE][j].type())
            {
              case Json::stringValue:
                child.text() = content[KEY_VALUE][j].asCString();
                break;

              case Json::realValue:
                child.text() = content[KEY_VALUE][j].asFloat();
                break;

              case Json::intValue:
                child.text() = content[KEY_VALUE][j].asInt();
                break;

              case Json::uintValue:
                child.text() = content[KEY_VALUE][j].asUInt();
                break;

              default:
                break;
            }
          }
        }
      }
      else if (content.isMember(KEY_BULK_DATA_URI) &&
               content[KEY_BULK_DATA_URI].type() == Json::stringValue)
      {
        pugi::xml_node child = node.append_child("BulkData");
        child.append_attribute("URI").set_value(content[KEY_BULK_DATA_URI].asCString());
      }
      else if (content.isMember(KEY_INLINE_BINARY) &&
               content[KEY_INLINE_BINARY].type() == Json::stringValue)
      {
        pugi::xml_node child = node.append_child("InlineBinary");
        child.text() = content[KEY_INLINE_BINARY].asCString();
      }
    }
  }
#endif


#if ORTHANC_ENABLE_PUGIXML == 1
  static void DicomWebJsonToXml(pugi::xml_document& target,
                                const Json::Value& source)
  {
    pugi::xml_node root = target.append_child("NativeDicomModel");
    root.append_attribute("xmlns").set_value("http://dicom.nema.org/PS3.19/models/NativeDICOM");
    root.append_attribute("xsi:schemaLocation").set_value("http://dicom.nema.org/PS3.19/models/NativeDICOM");
    root.append_attribute("xmlns:xsi").set_value("http://www.w3.org/2001/XMLSchema-instance");

    ExploreXmlDataset(root, source);

    pugi::xml_node decl = target.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("utf-8");
  }
#endif


  std::string DicomWebJsonVisitor::FormatTag(const DicomTag& tag)
  {
    char buf[16];
    sprintf(buf, "%04X%04X", tag.GetGroup(), tag.GetElement());
    return std::string(buf);
  }

    
  Json::Value& DicomWebJsonVisitor::CreateNode(const std::vector<DicomTag>& parentTags,
                                               const std::vector<size_t>& parentIndexes,
                                               const DicomTag& tag)
  {
    assert(parentTags.size() == parentIndexes.size());      

    Json::Value* node = &result_;

    for (size_t i = 0; i < parentTags.size(); i++)
    {
      std::string t = FormatTag(parentTags[i]);

      if (!node->isMember(t))
      {
        Json::Value item = Json::objectValue;
        item[KEY_VR] = KEY_SQ;
        item[KEY_VALUE] = Json::arrayValue;
        item[KEY_VALUE].append(Json::objectValue);
        (*node) [t] = item;

        node = &(*node)[t][KEY_VALUE][0];
      }
      else if ((*node)  [t].type() != Json::objectValue ||
               !(*node) [t].isMember(KEY_VR) ||
               (*node)  [t][KEY_VR].type() != Json::stringValue ||
               (*node)  [t][KEY_VR].asString() != KEY_SQ ||
               !(*node) [t].isMember(KEY_VALUE) ||
               (*node)  [t][KEY_VALUE].type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        size_t currentSize = (*node) [t][KEY_VALUE].size();

        if (parentIndexes[i] < currentSize)
        {
          // The node already exists
        }
        else if (parentIndexes[i] == currentSize)
        {
          (*node) [t][KEY_VALUE].append(Json::objectValue);
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError);
        }
          
        node = &(*node) [t][KEY_VALUE][Json::ArrayIndex(parentIndexes[i])];
      }
    }

    assert(node->type() == Json::objectValue);

    std::string t = FormatTag(tag);
    if (node->isMember(t))
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      (*node) [t] = Json::objectValue;
      return (*node) [t];
    }
  }

    
  Json::Value DicomWebJsonVisitor::FormatInteger(int64_t value)
  {
    if (value < 0)
    {
      return Json::Value(static_cast<int32_t>(value));
    }
    else
    {
      return Json::Value(static_cast<uint32_t>(value));
    }
  }

    
  Json::Value DicomWebJsonVisitor::FormatDouble(double value)
  {
    try
    {
      long long a = boost::math::llround<double>(value);

      double d = fabs(value - static_cast<double>(a));

      if (d <= std::numeric_limits<double>::epsilon() * 100.0)
      {
        return FormatInteger(a);
      }
      else
      {
        return Json::Value(value);
      }
    }
    catch (boost::math::rounding_error&)
    {
      // Can occur if "long long" is too small to receive this value
      // (e.g. infinity)
      return Json::Value(value);
    }
  }


#if ORTHANC_ENABLE_PUGIXML == 1
  void DicomWebJsonVisitor::FormatXml(std::string& target) const
  {
    pugi::xml_document doc;
    DicomWebJsonToXml(doc, result_);
    Toolbox::XmlToString(target, doc);
  }
#endif


  void DicomWebJsonVisitor::VisitEmptySequence(const std::vector<DicomTag>& parentTags,
                                               const std::vector<size_t>& parentIndexes,
                                               const DicomTag& tag)
  {
    if (tag.GetElement() != 0x0000)
    {
      Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
      node[KEY_VR] = EnumerationToString(ValueRepresentation_Sequence);
    }
  }
  

  void DicomWebJsonVisitor::VisitBinary(const std::vector<DicomTag>& parentTags,
                                        const std::vector<size_t>& parentIndexes,
                                        const DicomTag& tag,
                                        ValueRepresentation vr,
                                        const void* data,
                                        size_t size)
  {
    assert(vr == ValueRepresentation_OtherByte ||
           vr == ValueRepresentation_OtherDouble ||
           vr == ValueRepresentation_OtherFloat ||
           vr == ValueRepresentation_OtherLong ||
           vr == ValueRepresentation_OtherWord ||
           vr == ValueRepresentation_Unknown);

    if (tag.GetElement() != 0x0000)
    {
      BinaryMode mode;
      std::string bulkDataUri;
        
      if (formatter_ == NULL)
      {
        mode = BinaryMode_InlineBinary;
      }
      else
      {
        mode = formatter_->Format(bulkDataUri, parentTags, parentIndexes, tag, vr);
      }

      if (mode != BinaryMode_Ignore)
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(vr);

        switch (mode)
        {
          case BinaryMode_BulkDataUri:
            node[KEY_BULK_DATA_URI] = bulkDataUri;
            break;

          case BinaryMode_InlineBinary:
          {
            std::string tmp(static_cast<const char*>(data), size);
          
            std::string base64;
            Toolbox::EncodeBase64(base64, tmp);

            node[KEY_INLINE_BINARY] = base64;
            break;
          }

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }
    }
  }


  void DicomWebJsonVisitor::VisitIntegers(const std::vector<DicomTag>& parentTags,
                                          const std::vector<size_t>& parentIndexes,
                                          const DicomTag& tag,
                                          ValueRepresentation vr,
                                          const std::vector<int64_t>& values)
  {
    if (tag.GetElement() != 0x0000 &&
        vr != ValueRepresentation_NotSupported)
    {
      Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
      node[KEY_VR] = EnumerationToString(vr);

      if (!values.empty())
      {
        Json::Value content = Json::arrayValue;
        for (size_t i = 0; i < values.size(); i++)
        {
          content.append(FormatInteger(values[i]));
        }

        node[KEY_VALUE] = content;
      }
    }
  }

  void DicomWebJsonVisitor::VisitDoubles(const std::vector<DicomTag>& parentTags,
                                         const std::vector<size_t>& parentIndexes,
                                         const DicomTag& tag,
                                         ValueRepresentation vr,
                                         const std::vector<double>& values)
  {
    if (tag.GetElement() != 0x0000 &&
        vr != ValueRepresentation_NotSupported)
    {
      Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
      node[KEY_VR] = EnumerationToString(vr);

      if (!values.empty())
      {
        Json::Value content = Json::arrayValue;
        for (size_t i = 0; i < values.size(); i++)
        {
          content.append(FormatDouble(values[i]));
        }
          
        node[KEY_VALUE] = content;
      }
    }
  }

  
  void DicomWebJsonVisitor::VisitAttributes(const std::vector<DicomTag>& parentTags,
                                            const std::vector<size_t>& parentIndexes,
                                            const DicomTag& tag,
                                            const std::vector<DicomTag>& values)
  {
    if (tag.GetElement() != 0x0000)
    {
      Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
      node[KEY_VR] = EnumerationToString(ValueRepresentation_AttributeTag);

      if (!values.empty())
      {
        Json::Value content = Json::arrayValue;
        for (size_t i = 0; i < values.size(); i++)
        {
          content.append(FormatTag(values[i]));
        }
          
        node[KEY_VALUE] = content;
      }
    }
  }

  
  ITagVisitor::Action
  DicomWebJsonVisitor::VisitString(std::string& newValue,
                                   const std::vector<DicomTag>& parentTags,
                                   const std::vector<size_t>& parentIndexes,
                                   const DicomTag& tag,
                                   ValueRepresentation vr,
                                   const std::string& value)
  {
    if (tag.GetElement() == 0x0000 ||
        vr == ValueRepresentation_NotSupported)
    {
      return Action_None;
    }
    else
    {
      Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
      node[KEY_VR] = EnumerationToString(vr);

#if 0
      /**
       * TODO - The JSON file has an UTF-8 encoding, thus DCMTK
       * replaces the specific character set with "ISO_IR 192"
       * (UNICODE UTF-8). On Google Cloud Healthcare, however, the
       * source encoding is reported, which seems more logical. We
       * thus choose the Google convention. Enabling this block will
       * mimic the DCMTK behavior.
       **/
      if (tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        node[KEY_VALUE].append("ISO_IR 192");
      }
      else
#endif
      {
        std::string truncated;
        
        if (!value.empty() &&
            value[value.size() - 1] == '\0')
        {
          truncated = value.substr(0, value.size() - 1);
        }
        else
        {
          truncated = value;
        }

        if (!truncated.empty())
        {
          std::vector<std::string> tokens;
          Toolbox::TokenizeString(tokens, truncated, '\\');

          if (tag == DICOM_TAG_SPECIFIC_CHARACTER_SET &&
              tokens.size() > 1 &&
              tokens[0].empty())
          {
            // Specific character set with code extension: Remove the
            // first element from the vector of encodings
            tokens.erase(tokens.begin());
          }

          node[KEY_VALUE] = Json::arrayValue;
          for (size_t i = 0; i < tokens.size(); i++)
          {
            try
            {
              switch (vr)
              {
                case ValueRepresentation_PersonName:
                {
                  Json::Value value = Json::objectValue;
                  if (!tokens[i].empty())
                  {
                    std::vector<std::string> components;
                    Toolbox::TokenizeString(components, tokens[i], '=');

                    if (components.size() >= 1)
                    {
                      value[KEY_ALPHABETIC] = components[0];
                    }

                    if (components.size() >= 2)
                    {
                      value[KEY_IDEOGRAPHIC] = components[1];
                    }

                    if (components.size() >= 3)
                    {
                      value[KEY_PHONETIC] = components[2];
                    }
                  }
                  
                  node[KEY_VALUE].append(value);
                  break;
                }
                  
                case ValueRepresentation_IntegerString:
                {
                  /**
                   * The calls to "StripSpaces()" below fix the
                   * issue reported by Rana Asim Wajid on 2019-06-05
                   * ("Error Exception while invoking plugin service
                   * 32: Bad file format"):
                   * https://groups.google.com/d/msg/orthanc-users/T32FovWPcCE/-hKFbfRJBgAJ
                   **/

                  std::string t = Orthanc::Toolbox::StripSpaces(tokens[i]);
                  if (t.empty())
                  {
                    node[KEY_VALUE].append(Json::nullValue);
                  }
                  else
                  {
                    int64_t value = boost::lexical_cast<int64_t>(t);
                    node[KEY_VALUE].append(FormatInteger(value));
                  }
                 
                  break;
                }
              
                case ValueRepresentation_DecimalString:
                {
                  std::string t = Orthanc::Toolbox::StripSpaces(tokens[i]);
                  if (t.empty())
                  {
                    node[KEY_VALUE].append(Json::nullValue);
                  }
                  else
                  {
                    double value = boost::lexical_cast<double>(t);
                    node[KEY_VALUE].append(FormatDouble(value));
                  }

                  break;
                }

                default:
                  if (tokens[i].empty())
                  {
                    node[KEY_VALUE].append(Json::nullValue);
                  }
                  else
                  {
                    node[KEY_VALUE].append(tokens[i]);
                  }
                  
                  break;
              }
            }
            catch (boost::bad_lexical_cast&)
            {
              std::string tmp;
              if (value.size() < 64 &&
                  Toolbox::IsAsciiString(value))
              {
                tmp = ": " + value;
              }
              
              LOG(WARNING) << "Ignoring DICOM tag (" << tag.Format()
                           << ") with invalid content for VR " << EnumerationToString(vr) << tmp;
            }
          }
        }
      }
    }
      
    return Action_None;
  }
}
