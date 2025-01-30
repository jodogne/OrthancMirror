/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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
#include <gtest/gtest.h>

#include "../../OrthancFramework/Sources/Compatibility.h"
#include "../../OrthancFramework/Sources/FileStorage/MemoryStorageArea.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"

#include "../Sources/Database/SQLiteDatabaseWrapper.h"
#include "../Sources/ServerContext.h"

using namespace Orthanc;

TEST(ServerConfig, AcceptedSopClasses)
{
  const std::string path = "UnitTestsStorage";

  MemoryStorageArea storage;
  SQLiteDatabaseWrapper db;   // The SQLite DB is in memory
  db.Open();
  ServerContext context(db, storage, true /* running unit tests */, 10, false, 1);

  { // default config -> all SOP Classes should be accepted
    std::set<std::string> s;

    context.GetAcceptedSopClasses(s, 0);
    ASSERT_LE(100u, s.size());

    ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") != s.end());
    ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") != s.end());

    context.GetAcceptedSopClasses(s, 1);
    ASSERT_EQ(1u, s.size());
  }

  {
    std::list<std::string> acceptedStorageClasses;
    std::set<std::string> rejectedStorageClasses;

    std::set<std::string> s;
    context.GetAcceptedSopClasses(s, 0);
    size_t allSize = s.size();

    { // default config but reject one class
      acceptedStorageClasses.clear();
      rejectedStorageClasses.clear();
      rejectedStorageClasses.insert("1.2.840.10008.5.1.4.1.1.4");

      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_EQ(allSize - 1, s.size());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") == s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") != s.end());

      context.GetAcceptedSopClasses(s, 1);
      ASSERT_EQ(1u, s.size());
    }

    { // default config but reject one regex
      acceptedStorageClasses.clear();
      rejectedStorageClasses.clear();
      rejectedStorageClasses.insert("1.2.840.10008.5.1.4.1.*");
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") == s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") == s.end());
    }

    { // accept a single - no rejection
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.840.10008.5.1.4.1.1.4");
      rejectedStorageClasses.clear();
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_EQ(1, s.size());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") != s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") == s.end());
    }

    { // accept from regex - reject one
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.840.10008.5.1.4.1.*");
      rejectedStorageClasses.clear();
      rejectedStorageClasses.insert("1.2.840.10008.5.1.4.1.1.12.1.1");
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_LE(10, s.size());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") != s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") == s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.2.1") != s.end());
    }

    { // accept from regex - reject from regex
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.840.10008.5.1.4.1.*");
      rejectedStorageClasses.clear();
      rejectedStorageClasses.insert("1.2.840.10008.5.1.4.1.1.12.1.*");
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_LE(10, s.size());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.4") != s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.1.1") == s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.2.1") != s.end());
    }

    { // accept one that is unknown form DCMTK
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.3.4");
      rejectedStorageClasses.clear();
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_EQ(1, s.size());
      ASSERT_TRUE(s.find("1.2.3.4") != s.end());
    }

    { // accept the default ones + a custom one
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.840.*");
      acceptedStorageClasses.push_back("1.2.3.4");
      rejectedStorageClasses.clear();
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_LE(100u, s.size());
      ASSERT_TRUE(s.find("1.2.3.4") != s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.2.1") != s.end());
    }

    { // test the ? in regex to replace a single char
      acceptedStorageClasses.clear();
      acceptedStorageClasses.push_back("1.2.840.10008.5.1.4.1.1.12.2.?");
      acceptedStorageClasses.push_back("1.2.3.4");
      rejectedStorageClasses.clear();
      context.SetAcceptedSopClasses(acceptedStorageClasses, rejectedStorageClasses);

      context.GetAcceptedSopClasses(s, 0);
      ASSERT_EQ(2u, s.size());
      ASSERT_TRUE(s.find("1.2.3.4") != s.end());
      ASSERT_TRUE(s.find("1.2.840.10008.5.1.4.1.1.12.2.1") != s.end());
    }

  }

  context.Stop();
  db.Close();
}
