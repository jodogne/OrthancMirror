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

#include "../Sources/OrthancInitialization.h"
#include "../Sources/ServerEnumerations.h"


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
