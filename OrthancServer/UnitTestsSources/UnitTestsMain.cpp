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


#include "PrecompiledHeadersUnitTests.h"
#include "../../OrthancFramework/Sources/EnumerationDictionary.h"

#include "gtest/gtest.h"

#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Toolbox.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/Images/Image.h"
#include "../../OrthancFramework/Sources/Images/PngWriter.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/ToDcmtkBridge.h"

#include "../Sources/OrthancConfiguration.h"  // For the FontRegistry
#include "../Sources/OrthancInitialization.h"
#include "../Sources/ServerEnumerations.h"
#include "../Sources/ServerToolbox.h"
#include "../Plugins/Engine/PluginsEnumerations.h"


using namespace Orthanc;


TEST(EnumerationDictionary, Simple)
{
  EnumerationDictionary<MetadataType>  d;

  ASSERT_THROW(d.Translate("ReceptionDate"), OrthancException);
  ASSERT_EQ(MetadataType_ModifiedFrom, d.Translate("5"));
  ASSERT_EQ(256, d.Translate("256"));

  d.Add(MetadataType_Instance_ReceptionDate, "ReceptionDate");

  ASSERT_EQ(MetadataType_Instance_ReceptionDate, d.Translate("ReceptionDate"));
  ASSERT_EQ(MetadataType_Instance_ReceptionDate, d.Translate("2"));
  ASSERT_EQ("ReceptionDate", d.Translate(MetadataType_Instance_ReceptionDate));

  ASSERT_THROW(d.Add(MetadataType_Instance_ReceptionDate, "Hello"), OrthancException);
  ASSERT_THROW(d.Add(MetadataType_ModifiedFrom, "ReceptionDate"), OrthancException); // already used
  ASSERT_THROW(d.Add(MetadataType_ModifiedFrom, "1024"), OrthancException); // cannot register numbers
  d.Add(MetadataType_ModifiedFrom, "ModifiedFrom");  // ok
}


TEST(EnumerationDictionary, ServerEnumerations)
{
  ASSERT_STREQ("Patient", EnumerationToString(ResourceType_Patient));
  ASSERT_STREQ("Study", EnumerationToString(ResourceType_Study));
  ASSERT_STREQ("Series", EnumerationToString(ResourceType_Series));
  ASSERT_STREQ("Instance", EnumerationToString(ResourceType_Instance));

  ASSERT_STREQ("ModifiedSeries", EnumerationToString(ChangeType_ModifiedSeries));

  ASSERT_STREQ("Failure", EnumerationToString(StoreStatus_Failure));
  ASSERT_STREQ("Success", EnumerationToString(StoreStatus_Success));

  ASSERT_STREQ("CompletedSeries", EnumerationToString(ChangeType_CompletedSeries));

  ASSERT_EQ("IndexInSeries", EnumerationToString(MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("LastUpdate", EnumerationToString(MetadataType_LastUpdate));

  ASSERT_EQ(ResourceType_Patient, StringToResourceType("PATienT"));
  ASSERT_EQ(ResourceType_Study, StringToResourceType("STudy"));
  ASSERT_EQ(ResourceType_Series, StringToResourceType("SeRiEs"));
  ASSERT_EQ(ResourceType_Instance, StringToResourceType("INStance"));
  ASSERT_EQ(ResourceType_Instance, StringToResourceType("IMagE"));
  ASSERT_THROW(StringToResourceType("heLLo"), OrthancException);

  ASSERT_EQ(2047, StringToMetadata("2047"));
  ASSERT_THROW(StringToMetadata("Ceci est un test"), OrthancException);
  ASSERT_THROW(RegisterUserMetadata(128, ""), OrthancException); // too low (< 1024)
  ASSERT_THROW(RegisterUserMetadata(128000, ""), OrthancException); // too high (> 65535)
  RegisterUserMetadata(2047, "Ceci est un test");
  ASSERT_EQ(2047, StringToMetadata("2047"));
  ASSERT_EQ(2047, StringToMetadata("Ceci est un test"));

  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("Generic")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("GenericNoWildcardInDates")));
  ASSERT_STREQ("GenericNoUniversalWildcard", EnumerationToString(StringToModalityManufacturer("GenericNoUniversalWildcard")));
  ASSERT_STREQ("StoreScp", EnumerationToString(StringToModalityManufacturer("StoreScp")));
  ASSERT_STREQ("Vitrea", EnumerationToString(StringToModalityManufacturer("Vitrea")));
  ASSERT_STREQ("GE", EnumerationToString(StringToModalityManufacturer("GE")));
  // backward compatibility tests (to remove once we make these manufacturer really obsolete)
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("MedInria")));
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("EFilm2")));
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("ClearCanvas")));
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("Dcm4Chee")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("SyngoVia")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("AgfaImpax")));
}



TEST(FontRegistry, Basic)
{
  Orthanc::Image s(Orthanc::PixelFormat_RGB24, 640, 480, false);
  memset(s.GetBuffer(), 0, s.GetPitch() * s.GetHeight());

  {
    Orthanc::OrthancConfiguration::ReaderLock lock;
    ASSERT_GE(1u, lock.GetConfiguration().GetFontRegistry().GetSize());
    lock.GetConfiguration().GetFontRegistry().GetFont(0).Draw
      (s, "Hello world É\n\rComment ça va ?\nq", 50, 60, 255, 0, 0);
  }

  Orthanc::PngWriter w;
  w.WriteToFile("UnitTestsResults/font.png", s);
}


TEST(FromDcmtkBridge, ValueRepresentationConversions)
{
#if ORTHANC_ENABLE_PLUGINS == 1
  ASSERT_EQ(1, ValueRepresentation_ApplicationEntity);
  ASSERT_EQ(1, OrthancPluginValueRepresentation_AE);

  for (int i = ValueRepresentation_ApplicationEntity;
       i <= ValueRepresentation_NotSupported; i++)
  {
    ValueRepresentation vr = static_cast<ValueRepresentation>(i);

    if (vr == ValueRepresentation_NotSupported)
    {
      ASSERT_THROW(ToDcmtkBridge::Convert(vr), OrthancException);
      ASSERT_THROW(Plugins::Convert(vr), OrthancException);
    }
    else if (vr == ValueRepresentation_OtherDouble || 
             vr == ValueRepresentation_OtherLong ||
             vr == ValueRepresentation_UniversalResource ||
             vr == ValueRepresentation_UnlimitedCharacters)
    {
      // These VR are not supported as of DCMTK 3.6.0
      ASSERT_THROW(ToDcmtkBridge::Convert(vr), OrthancException);
      ASSERT_EQ(OrthancPluginValueRepresentation_UN, Plugins::Convert(vr));
    }
    else
    {
      ASSERT_EQ(vr, FromDcmtkBridge::Convert(ToDcmtkBridge::Convert(vr)));

      OrthancPluginValueRepresentation plugins = Plugins::Convert(vr);
      ASSERT_EQ(vr, Plugins::Convert(plugins));
    }
  }

  for (int i = OrthancPluginValueRepresentation_AE;
       i <= OrthancPluginValueRepresentation_UT; i++)
  {
    OrthancPluginValueRepresentation plugins = static_cast<OrthancPluginValueRepresentation>(i);
    ValueRepresentation orthanc = Plugins::Convert(plugins);
    ASSERT_EQ(plugins, Plugins::Convert(orthanc));
  }
#endif
}



namespace Orthanc
{
  // Namespace for the "FRIEND_TEST()" directive in "FromDcmtkBridge" to apply:
  // https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#private-class-members

  static const DicomTag REFERENCED_STUDY_SEQUENCE(0x0008, 0x1110);
  static const DicomTag REFERENCED_PATIENT_SEQUENCE(0x0008, 0x1120);

  static void CreateSampleJson(Json::Value& a)
  {
    {
      Json::Value b = Json::objectValue;
      b["PatientName"] = "Hello";
      b["PatientID"] = "World";
      b["StudyDescription"] = "Toto";
      a.append(b);
    }

    {
      Json::Value b = Json::objectValue;
      b["PatientName"] = "data:application/octet-stream;base64,SGVsbG8y";  // echo -n "Hello2" | base64
      b["PatientID"] = "World2";
      a.append(b);
    }
  }
  
  TEST(FromDcmtkBridge, FromJson)
  {
    std::unique_ptr<DcmElement> element;

    {
      Json::Value a;
      a = "Hello";
      element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8, ""));

      Json::Value b;
      std::set<DicomTag> ignoreTagLength;
      ignoreTagLength.insert(DICOM_TAG_PATIENT_ID);

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b.isMember("0010,0010"));
      ASSERT_EQ("Hello", b["0010,0010"].asString());

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isNull()); // "Hello" has more than 3 characters

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Full,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isObject());
      ASSERT_EQ("PatientName", b["0010,0010"]["Name"].asString());
      ASSERT_EQ("TooLong", b["0010,0010"]["Type"].asString());
      ASSERT_TRUE(b["0010,0010"]["Value"].isNull());

      ignoreTagLength.insert(DICOM_TAG_PATIENT_NAME);
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_EQ("Hello", b["0010,0010"].asString());
    }

    {
      Json::Value a;
      a = "Hello";
      // Cannot assign a string to a sequence
      ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, false, Encoding_Utf8, "")), OrthancException);
    }

    {
      Json::Value a = Json::arrayValue;
      a.append("Hello");
      // Cannot assign an array to a string
      ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8, "")), OrthancException);
    }

    {
      Json::Value a;
      a = "data:application/octet-stream;base64,SGVsbG8=";  // echo -n "Hello" | base64
      element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, true, Encoding_Utf8, ""));

      Json::Value b;
      std::set<DicomTag> ignoreTagLength;
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_EQ("Hello", b["0010,0010"].asString());
    }

    {
      Json::Value a = Json::arrayValue;
      CreateSampleJson(a);
      element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, true, Encoding_Utf8, ""));

      {
        Json::Value b;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
        ASSERT_EQ(Json::arrayValue, b["0008,1110"].type());
        ASSERT_EQ(2u, b["0008,1110"].size());
      
        Json::Value::ArrayIndex i = (b["0008,1110"][0]["0010,0010"].asString() == "Hello") ? 0 : 1;

        ASSERT_EQ(3u, b["0008,1110"][i].size());
        ASSERT_EQ(2u, b["0008,1110"][1 - i].size());
        ASSERT_EQ(b["0008,1110"][i]["0010,0010"].asString(), "Hello");
        ASSERT_EQ(b["0008,1110"][i]["0010,0020"].asString(), "World");
        ASSERT_EQ(b["0008,1110"][i]["0008,1030"].asString(), "Toto");
        ASSERT_EQ(b["0008,1110"][1 - i]["0010,0010"].asString(), "Hello2");
        ASSERT_EQ(b["0008,1110"][1 - i]["0010,0020"].asString(), "World2");
      }

      {
        Json::Value b;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Full,
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);

        Json::Value c;
        Toolbox::SimplifyDicomAsJson(c, b, DicomToJsonFormat_Human);

        a[1]["PatientName"] = "Hello2";  // To remove the Data URI Scheme encoding
        ASSERT_EQ(0, c["ReferencedStudySequence"].compare(a));
      }
    }
  }
}


int main(int argc, char **argv)
{
  Logging::Initialize();
  Toolbox::InitializeGlobalLocale(NULL);
  Logging::EnableInfoLevel(true);
  Toolbox::DetectEndianness();
  SystemToolbox::MakeDirectory("UnitTestsResults");
  OrthancInitialize();

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  OrthancFinalize();
  Logging::Finalize();

  return result;
}
