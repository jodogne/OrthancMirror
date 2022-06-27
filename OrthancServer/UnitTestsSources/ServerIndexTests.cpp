/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../OrthancFramework/Sources/FileStorage/FilesystemStorage.h"
#include "../../OrthancFramework/Sources/FileStorage/MemoryStorageArea.h"
#include "../../OrthancFramework/Sources/Images/Image.h"
#include "../../OrthancFramework/Sources/Logging.h"

#include "../Sources/Database/SQLiteDatabaseWrapper.h"
#include "../Sources/OrthancConfiguration.h"
#include "../Sources/Search/DatabaseLookup.h"
#include "../Sources/ServerContext.h"
#include "../Sources/ServerToolbox.h"

#include <ctype.h>
#include <algorithm>

using namespace Orthanc;

namespace
{
  class TestDatabaseListener : public IDatabaseListener
  {
  public:
    std::vector<std::string> deletedFiles_;
    std::vector<std::string> deletedResources_;
    std::string ancestorId_;
    ResourceType ancestorType_;

    void Reset()
    {
      ancestorId_ = "";
      deletedFiles_.clear();
    }

    virtual void SignalRemainingAncestor(ResourceType type,
                                         const std::string& publicId)
      ORTHANC_OVERRIDE
    {
      ancestorId_ = publicId;
      ancestorType_ = type;
    }

    virtual void SignalAttachmentDeleted(const FileInfo& info) ORTHANC_OVERRIDE
    {
      const std::string fileUuid = info.GetUuid();
      deletedFiles_.push_back(fileUuid);
      LOG(INFO) << "A file must be removed: " << fileUuid;
    }       

    virtual void SignalResourceDeleted(ResourceType type,
                                       const std::string& publicId) ORTHANC_OVERRIDE
    {
      LOG(INFO) << "Deleted resource " << publicId << " of type " << EnumerationToString(type);
      deletedResources_.push_back(publicId);
    }
  };


  class DatabaseWrapperTest : public ::testing::Test
  {
  protected:
    std::unique_ptr<TestDatabaseListener>  listener_;
    std::unique_ptr<SQLiteDatabaseWrapper> index_;
    std::unique_ptr<SQLiteDatabaseWrapper::UnitTestsTransaction>  transaction_;

  public:
    DatabaseWrapperTest()
    {
    }

    virtual void SetUp() ORTHANC_OVERRIDE
    {
      listener_.reset(new TestDatabaseListener);
      index_.reset(new SQLiteDatabaseWrapper);
      index_->Open();
      transaction_.reset(dynamic_cast<SQLiteDatabaseWrapper::UnitTestsTransaction*>(
                           index_->StartTransaction(TransactionType_ReadWrite, *listener_)));
    }

    virtual void TearDown() ORTHANC_OVERRIDE
    {
      transaction_->Commit(0);
      transaction_.reset();
      
      index_->Close();
      index_.reset(NULL);
      listener_.reset(NULL);
    }

    void CheckTableRecordCount(uint32_t expected, const char* table)
    {
      ASSERT_EQ(expected, transaction_->GetTableRecordCount(table));
    }

    void CheckNoParent(int64_t id)
    {
      std::string s;
      ASSERT_FALSE(transaction_->GetParentPublicId(s, id));
    }

    void CheckParentPublicId(const char* expected, int64_t id)
    {
      std::string s;
      ASSERT_TRUE(transaction_->GetParentPublicId(s, id));
      ASSERT_EQ(expected, s);
    }

    void CheckNoChild(int64_t id)
    {
      std::list<std::string> j;
      transaction_->GetChildren(j, id);
      ASSERT_EQ(0u, j.size());
    }

    void CheckOneChild(const char* expected, int64_t id)
    {
      std::list<std::string> j;
      transaction_->GetChildren(j, id);
      ASSERT_EQ(1u, j.size());
      ASSERT_EQ(expected, j.front());
    }

    void CheckTwoChildren(const char* expected1,
                          const char* expected2,
                          int64_t id)
    {
      std::list<std::string> j;
      transaction_->GetChildren(j, id);
      ASSERT_EQ(2u, j.size());
      ASSERT_TRUE((expected1 == j.front() && expected2 == j.back()) ||
                  (expected1 == j.back() && expected2 == j.front()));                    
    }

    void DoLookupIdentifier(std::list<std::string>& result,
                            ResourceType level,
                            const DicomTag& tag,
                            ConstraintType type,
                            const std::string& value)
    {
      assert(ServerToolbox::IsIdentifier(tag, level));
      
      DicomTagConstraint c(tag, type, value, true, true);
      
      std::vector<DatabaseConstraint> lookup;
      lookup.push_back(c.ConvertToDatabaseConstraint(level, DicomTagType_Identifier));
      
      transaction_->ApplyLookupResources(result, NULL, lookup, level, 0 /* no limit */);
    }    

    void DoLookupIdentifier2(std::list<std::string>& result,
                             ResourceType level,
                             const DicomTag& tag,
                             ConstraintType type1,
                             const std::string& value1,
                             ConstraintType type2,
                             const std::string& value2)
    {
      assert(ServerToolbox::IsIdentifier(tag, level));
      
      DicomTagConstraint c1(tag, type1, value1, true, true);
      DicomTagConstraint c2(tag, type2, value2, true, true);
      
      std::vector<DatabaseConstraint> lookup;
      lookup.push_back(c1.ConvertToDatabaseConstraint(level, DicomTagType_Identifier));
      lookup.push_back(c2.ConvertToDatabaseConstraint(level, DicomTagType_Identifier));
      
      transaction_->ApplyLookupResources(result, NULL, lookup, level, 0 /* no limit */);
    }
  };
}


TEST_F(DatabaseWrapperTest, Simple)
{
  int64_t a[] = {
    transaction_->CreateResource("a", ResourceType_Patient),   // 0
    transaction_->CreateResource("b", ResourceType_Study),     // 1
    transaction_->CreateResource("c", ResourceType_Series),    // 2
    transaction_->CreateResource("d", ResourceType_Instance),  // 3
    transaction_->CreateResource("e", ResourceType_Instance),  // 4
    transaction_->CreateResource("f", ResourceType_Instance),  // 5
    transaction_->CreateResource("g", ResourceType_Study)      // 6
  };

  ASSERT_EQ("a", transaction_->GetPublicId(a[0]));
  ASSERT_EQ("b", transaction_->GetPublicId(a[1]));
  ASSERT_EQ("c", transaction_->GetPublicId(a[2]));
  ASSERT_EQ("d", transaction_->GetPublicId(a[3]));
  ASSERT_EQ("e", transaction_->GetPublicId(a[4]));
  ASSERT_EQ("f", transaction_->GetPublicId(a[5]));
  ASSERT_EQ("g", transaction_->GetPublicId(a[6]));

  ASSERT_EQ(ResourceType_Patient, transaction_->GetResourceType(a[0]));
  ASSERT_EQ(ResourceType_Study, transaction_->GetResourceType(a[1]));
  ASSERT_EQ(ResourceType_Series, transaction_->GetResourceType(a[2]));
  ASSERT_EQ(ResourceType_Instance, transaction_->GetResourceType(a[3]));
  ASSERT_EQ(ResourceType_Instance, transaction_->GetResourceType(a[4]));
  ASSERT_EQ(ResourceType_Instance, transaction_->GetResourceType(a[5]));
  ASSERT_EQ(ResourceType_Study, transaction_->GetResourceType(a[6]));

  {
    std::list<std::string> t;
    transaction_->GetAllPublicIds(t, ResourceType_Patient);

    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("a", t.front());

    transaction_->GetAllPublicIds(t, ResourceType_Series);
    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("c", t.front());

    transaction_->GetAllPublicIds(t, ResourceType_Study);
    ASSERT_EQ(2u, t.size());

    transaction_->GetAllPublicIds(t, ResourceType_Instance);
    ASSERT_EQ(3u, t.size());
  }

  transaction_->SetGlobalProperty(GlobalProperty_FlushSleep, true, "World");

  transaction_->AttachChild(a[0], a[1]);
  transaction_->AttachChild(a[1], a[2]);
  transaction_->AttachChild(a[2], a[3]);
  transaction_->AttachChild(a[2], a[4]);
  transaction_->AttachChild(a[6], a[5]);

  int64_t parent;
  ASSERT_FALSE(transaction_->LookupParent(parent, a[0]));
  ASSERT_TRUE(transaction_->LookupParent(parent, a[1])); ASSERT_EQ(a[0], parent);
  ASSERT_TRUE(transaction_->LookupParent(parent, a[2])); ASSERT_EQ(a[1], parent);
  ASSERT_TRUE(transaction_->LookupParent(parent, a[3])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(transaction_->LookupParent(parent, a[4])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(transaction_->LookupParent(parent, a[5])); ASSERT_EQ(a[6], parent);
  ASSERT_FALSE(transaction_->LookupParent(parent, a[6]));

  std::string s;

  CheckNoParent(a[0]);
  CheckNoParent(a[6]);
  CheckParentPublicId("a", a[1]);
  CheckParentPublicId("b", a[2]);
  CheckParentPublicId("c", a[3]);
  CheckParentPublicId("c", a[4]);
  CheckParentPublicId("g", a[5]);

  std::list<std::string> l;
  transaction_->GetChildrenPublicId(l, a[0]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("b", l.front());
  transaction_->GetChildrenPublicId(l, a[1]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("c", l.front());
  transaction_->GetChildrenPublicId(l, a[3]); ASSERT_EQ(0u, l.size()); 
  transaction_->GetChildrenPublicId(l, a[4]); ASSERT_EQ(0u, l.size()); 
  transaction_->GetChildrenPublicId(l, a[5]); ASSERT_EQ(0u, l.size()); 
  transaction_->GetChildrenPublicId(l, a[6]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("f", l.front());

  transaction_->GetChildrenPublicId(l, a[2]); ASSERT_EQ(2u, l.size()); 
  if (l.front() == "d")
  {
    ASSERT_EQ("e", l.back());
  }
  else
  {
    ASSERT_EQ("d", l.back());
    ASSERT_EQ("e", l.front());
  }

  std::map<MetadataType, std::string> md;
  transaction_->GetAllMetadata(md, a[4]);
  ASSERT_EQ(0u, md.size());

  transaction_->AddAttachment(a[4], FileInfo("my json file", FileContentType_DicomAsJson, 42, "md5", 
                                             CompressionType_ZlibWithSize, 21, "compressedMD5"), 42);
  transaction_->AddAttachment(a[4], FileInfo("my dicom file", FileContentType_Dicom, 42, "md5"), 43);
  transaction_->AddAttachment(a[6], FileInfo("world", FileContentType_Dicom, 44, "md5"), 44);
  
  // TODO - REVISIONS - "42" is revision number, that is not currently stored (*)
  transaction_->SetMetadata(a[4], MetadataType_RemoteAet, "PINNACLE", 42);
  
  transaction_->GetAllMetadata(md, a[4]);
  ASSERT_EQ(1u, md.size());
  ASSERT_EQ("PINNACLE", md[MetadataType_RemoteAet]);
  transaction_->SetMetadata(a[4], MetadataType_ModifiedFrom, "TUTU", 10);
  transaction_->GetAllMetadata(md, a[4]);
  ASSERT_EQ(2u, md.size());

  std::map<MetadataType, std::string> md2;
  transaction_->GetAllMetadata(md2, a[4]);
  ASSERT_EQ(2u, md2.size());
  ASSERT_EQ("TUTU", md2[MetadataType_ModifiedFrom]);
  ASSERT_EQ("PINNACLE", md2[MetadataType_RemoteAet]);

  transaction_->DeleteMetadata(a[4], MetadataType_ModifiedFrom);
  transaction_->GetAllMetadata(md, a[4]);
  ASSERT_EQ(1u, md.size());
  ASSERT_EQ("PINNACLE", md[MetadataType_RemoteAet]);

  transaction_->GetAllMetadata(md2, a[4]);
  ASSERT_EQ(1u, md2.size());
  ASSERT_EQ("PINNACLE", md2[MetadataType_RemoteAet]);


  ASSERT_EQ(21u + 42u + 44u, transaction_->GetTotalCompressedSize());
  ASSERT_EQ(42u + 42u + 44u, transaction_->GetTotalUncompressedSize());

  transaction_->SetMainDicomTag(a[3], DicomTag(0x0010, 0x0010), "PatientName");

  int64_t b;
  ResourceType t;
  ASSERT_TRUE(transaction_->LookupResource(b, t, "g"));
  ASSERT_EQ(7, b);
  ASSERT_EQ(ResourceType_Study, t);

  int64_t revision;
  ASSERT_TRUE(transaction_->LookupMetadata(s, revision, a[4], MetadataType_RemoteAet));
  ASSERT_EQ(0, revision);   // "0" instead of "42" because of (*)
  ASSERT_FALSE(transaction_->LookupMetadata(s, revision, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ(0, revision);
  ASSERT_EQ("PINNACLE", s);

  std::string u;
  ASSERT_TRUE(transaction_->LookupMetadata(u, revision, a[4], MetadataType_RemoteAet));
  ASSERT_EQ(0, revision);
  ASSERT_EQ("PINNACLE", u);
  ASSERT_FALSE(transaction_->LookupMetadata(u, revision, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ(0, revision);

  ASSERT_TRUE(transaction_->LookupGlobalProperty(s, GlobalProperty_FlushSleep, true));
  ASSERT_FALSE(transaction_->LookupGlobalProperty(s, static_cast<GlobalProperty>(42), true));
  ASSERT_EQ("World", s);

  FileInfo att;
  ASSERT_TRUE(transaction_->LookupAttachment(att, revision, a[4], FileContentType_DicomAsJson));
  ASSERT_EQ(0, revision);  // "0" instead of "42" because of (*)
  ASSERT_EQ("my json file", att.GetUuid());
  ASSERT_EQ(21u, att.GetCompressedSize());
  ASSERT_EQ("md5", att.GetUncompressedMD5());
  ASSERT_EQ("compressedMD5", att.GetCompressedMD5());
  ASSERT_EQ(42u, att.GetUncompressedSize());
  ASSERT_EQ(CompressionType_ZlibWithSize, att.GetCompressionType());

  ASSERT_TRUE(transaction_->LookupAttachment(att, revision, a[6], FileContentType_Dicom));
  ASSERT_EQ(0, revision);  // "0" instead of "42" because of (*)
  ASSERT_EQ("world", att.GetUuid());
  ASSERT_EQ(44u, att.GetCompressedSize());
  ASSERT_EQ("md5", att.GetUncompressedMD5());
  ASSERT_EQ("md5", att.GetCompressedMD5());
  ASSERT_EQ(44u, att.GetUncompressedSize());
  ASSERT_EQ(CompressionType_None, att.GetCompressionType());

  ASSERT_EQ(0u, listener_->deletedFiles_.size());
  ASSERT_EQ(0u, listener_->deletedResources_.size());

  CheckTableRecordCount(7, "Resources");
  CheckTableRecordCount(3, "AttachedFiles");
  CheckTableRecordCount(1, "Metadata");
  CheckTableRecordCount(1, "MainDicomTags");

  transaction_->DeleteResource(a[0]);
  ASSERT_EQ(5u, listener_->deletedResources_.size());
  ASSERT_EQ(2u, listener_->deletedFiles_.size());
  ASSERT_FALSE(std::find(listener_->deletedFiles_.begin(), 
                         listener_->deletedFiles_.end(),
                         "my json file") == listener_->deletedFiles_.end());
  ASSERT_FALSE(std::find(listener_->deletedFiles_.begin(), 
                         listener_->deletedFiles_.end(),
                         "my dicom file") == listener_->deletedFiles_.end());

  CheckTableRecordCount(2, "Resources");
  CheckTableRecordCount(0, "Metadata");
  CheckTableRecordCount(1, "AttachedFiles");
  CheckTableRecordCount(0, "MainDicomTags");

  transaction_->DeleteResource(a[5]);
  ASSERT_EQ(7u, listener_->deletedResources_.size());

  CheckTableRecordCount(0, "Resources");
  CheckTableRecordCount(0, "AttachedFiles");
  CheckTableRecordCount(3, "GlobalProperties");

  std::string tmp;
  ASSERT_TRUE(transaction_->LookupGlobalProperty(tmp, GlobalProperty_DatabaseSchemaVersion, true));
  ASSERT_EQ("6", tmp);
  ASSERT_TRUE(transaction_->LookupGlobalProperty(tmp, GlobalProperty_FlushSleep, true));
  ASSERT_EQ("World", tmp);
  ASSERT_TRUE(transaction_->LookupGlobalProperty(tmp, GlobalProperty_GetTotalSizeIsFast, true));
  ASSERT_EQ("1", tmp);

  ASSERT_EQ(3u, listener_->deletedFiles_.size());
  ASSERT_FALSE(std::find(listener_->deletedFiles_.begin(), 
                         listener_->deletedFiles_.end(),
                         "world") == listener_->deletedFiles_.end());
}


TEST_F(DatabaseWrapperTest, Upward)
{
  int64_t a[] = {
    transaction_->CreateResource("a", ResourceType_Patient),   // 0
    transaction_->CreateResource("b", ResourceType_Study),     // 1
    transaction_->CreateResource("c", ResourceType_Series),    // 2
    transaction_->CreateResource("d", ResourceType_Instance),  // 3
    transaction_->CreateResource("e", ResourceType_Instance),  // 4
    transaction_->CreateResource("f", ResourceType_Study),     // 5
    transaction_->CreateResource("g", ResourceType_Series),    // 6
    transaction_->CreateResource("h", ResourceType_Series)     // 7
  };

  transaction_->AttachChild(a[0], a[1]);
  transaction_->AttachChild(a[1], a[2]);
  transaction_->AttachChild(a[2], a[3]);
  transaction_->AttachChild(a[2], a[4]);
  transaction_->AttachChild(a[1], a[6]);
  transaction_->AttachChild(a[0], a[5]);
  transaction_->AttachChild(a[5], a[7]);

  CheckTwoChildren("b", "f", a[0]);
  CheckTwoChildren("c", "g", a[1]);
  CheckTwoChildren("d", "e", a[2]);
  CheckNoChild(a[3]);
  CheckNoChild(a[4]);
  CheckOneChild("h", a[5]);
  CheckNoChild(a[6]);
  CheckNoChild(a[7]);

  listener_->Reset();
  transaction_->DeleteResource(a[3]);
  ASSERT_EQ("c", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Series, listener_->ancestorType_);

  listener_->Reset();
  transaction_->DeleteResource(a[4]);
  ASSERT_EQ("b", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Study, listener_->ancestorType_);

  listener_->Reset();
  transaction_->DeleteResource(a[7]);
  ASSERT_EQ("a", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Patient, listener_->ancestorType_);

  listener_->Reset();
  transaction_->DeleteResource(a[6]);
  ASSERT_EQ("", listener_->ancestorId_);  // No more ancestor
}


TEST_F(DatabaseWrapperTest, PatientRecycling)
{
  std::vector<int64_t> patients;
  for (int i = 0; i < 10; i++)
  {
    std::string p = "Patient " + boost::lexical_cast<std::string>(i);
    patients.push_back(transaction_->CreateResource(p, ResourceType_Patient));
    transaction_->AddAttachment(patients[i], FileInfo(p, FileContentType_Dicom, i + 10, 
                                                      "md5-" + boost::lexical_cast<std::string>(i)), 42);
    ASSERT_FALSE(transaction_->IsProtectedPatient(patients[i]));
  }

  CheckTableRecordCount(10u, "Resources");
  CheckTableRecordCount(10u, "PatientRecyclingOrder");

  listener_->Reset();
  ASSERT_EQ(0u, listener_->deletedResources_.size());

  transaction_->DeleteResource(patients[5]);
  transaction_->DeleteResource(patients[0]);
  ASSERT_EQ(2u, listener_->deletedResources_.size());

  CheckTableRecordCount(8u, "Resources");
  CheckTableRecordCount(8u, "PatientRecyclingOrder");

  ASSERT_EQ(2u, listener_->deletedFiles_.size());
  ASSERT_EQ("Patient 5", listener_->deletedFiles_[0]);
  ASSERT_EQ("Patient 0", listener_->deletedFiles_[1]);

  int64_t p;
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[1]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(3u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[2]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(4u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[3]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(5u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[4]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(6u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[6]);
  transaction_->DeleteResource(p);
  transaction_->DeleteResource(patients[8]);
  ASSERT_EQ(8u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[7]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(9u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[9]);
  transaction_->DeleteResource(p);
  ASSERT_FALSE(transaction_->SelectPatientToRecycle(p));
  ASSERT_EQ(10u, listener_->deletedResources_.size());

  ASSERT_EQ(10u, listener_->deletedFiles_.size());

  CheckTableRecordCount(0, "Resources");
  CheckTableRecordCount(0, "PatientRecyclingOrder");
}


TEST_F(DatabaseWrapperTest, PatientProtection)
{
  std::vector<int64_t> patients;
  for (int i = 0; i < 5; i++)
  {
    std::string p = "Patient " + boost::lexical_cast<std::string>(i);
    patients.push_back(transaction_->CreateResource(p, ResourceType_Patient));
    transaction_->AddAttachment(patients[i], FileInfo(p, FileContentType_Dicom, i + 10,
                                                      "md5-" + boost::lexical_cast<std::string>(i)), 42);
    ASSERT_FALSE(transaction_->IsProtectedPatient(patients[i]));
  }

  CheckTableRecordCount(5, "Resources");
  CheckTableRecordCount(5, "PatientRecyclingOrder");

  ASSERT_FALSE(transaction_->IsProtectedPatient(patients[2]));
  transaction_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "Resources");
  CheckTableRecordCount(4, "PatientRecyclingOrder");

  transaction_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");
  transaction_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  transaction_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  CheckTableRecordCount(5, "Resources");
  transaction_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");
  transaction_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(transaction_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  transaction_->SetProtectedPatient(patients[3], true);
  ASSERT_TRUE(transaction_->IsProtectedPatient(patients[3]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");

  CheckTableRecordCount(5, "Resources");
  ASSERT_EQ(0u, listener_->deletedFiles_.size());

  // Unprotecting a patient puts it at the last position in the recycling queue
  int64_t p;
  ASSERT_EQ(0u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[0]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(1u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p, patients[1])); ASSERT_EQ(p, patients[4]);
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[1]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(2u, listener_->deletedResources_.size());
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[4]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(3u, listener_->deletedResources_.size());
  ASSERT_FALSE(transaction_->SelectPatientToRecycle(p, patients[2]));
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[2]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(4u, listener_->deletedResources_.size());
  // "patients[3]" is still protected
  ASSERT_FALSE(transaction_->SelectPatientToRecycle(p));

  ASSERT_EQ(4u, listener_->deletedFiles_.size());
  CheckTableRecordCount(1, "Resources");
  CheckTableRecordCount(0, "PatientRecyclingOrder");

  transaction_->SetProtectedPatient(patients[3], false);
  CheckTableRecordCount(1, "PatientRecyclingOrder");
  ASSERT_FALSE(transaction_->SelectPatientToRecycle(p, patients[3]));
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p, patients[2]));
  ASSERT_TRUE(transaction_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[3]);
  transaction_->DeleteResource(p);
  ASSERT_EQ(5u, listener_->deletedResources_.size());

  ASSERT_EQ(5u, listener_->deletedFiles_.size());
  CheckTableRecordCount(0, "Resources");
  CheckTableRecordCount(0, "PatientRecyclingOrder");
}


TEST(ServerIndex, Sequence)
{
  const std::string path = "UnitTestsStorage";

  SystemToolbox::RemoveFile(path + "/index");
  FilesystemStorage storage(path);
  SQLiteDatabaseWrapper db;   // The SQLite DB is in memory
  db.Open();
  ServerContext context(db, storage, true /* running unit tests */, 10);
  context.SetupJobsEngine(true, false);

  ServerIndex& index = context.GetIndex();

  ASSERT_EQ(1u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence, true));
  ASSERT_EQ(2u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence, true));
  ASSERT_EQ(3u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence, true));
  ASSERT_EQ(4u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence, true));

  context.Stop();
  db.Close();
}


TEST_F(DatabaseWrapperTest, LookupIdentifier)
{
  int64_t a[] = {
    transaction_->CreateResource("a", ResourceType_Study),   // 0
    transaction_->CreateResource("b", ResourceType_Study),   // 1
    transaction_->CreateResource("c", ResourceType_Study),   // 2
    transaction_->CreateResource("d", ResourceType_Series)   // 3
  };

  transaction_->SetIdentifierTag(a[0], DICOM_TAG_STUDY_INSTANCE_UID, "0");
  transaction_->SetIdentifierTag(a[1], DICOM_TAG_STUDY_INSTANCE_UID, "1");
  transaction_->SetIdentifierTag(a[2], DICOM_TAG_STUDY_INSTANCE_UID, "0");
  transaction_->SetIdentifierTag(a[3], DICOM_TAG_SERIES_INSTANCE_UID, "0");

  std::list<std::string> s;

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_Equal, "0");
  ASSERT_EQ(2u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "a") != s.end());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "c") != s.end());

  DoLookupIdentifier(s, ResourceType_Series, DICOM_TAG_SERIES_INSTANCE_UID, ConstraintType_Equal, "0");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "d") != s.end());

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_Equal, "1");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "b") != s.end());

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_Equal, "1");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "b") != s.end());

  DoLookupIdentifier(s, ResourceType_Series, DICOM_TAG_SERIES_INSTANCE_UID, ConstraintType_Equal, "1");
  ASSERT_EQ(0u, s.size());

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_GreaterOrEqual, "0");
  ASSERT_EQ(3u, s.size());

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_GreaterOrEqual, "1");
  ASSERT_EQ(1u, s.size());

  DoLookupIdentifier(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, ConstraintType_GreaterOrEqual, "2");
  ASSERT_EQ(0u, s.size());

  DoLookupIdentifier2(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID,
                      ConstraintType_GreaterOrEqual, "0", ConstraintType_SmallerOrEqual, "0");
  ASSERT_EQ(2u, s.size());

  DoLookupIdentifier2(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID,
                      ConstraintType_GreaterOrEqual, "1", ConstraintType_SmallerOrEqual, "1");
  ASSERT_EQ(1u, s.size());

  DoLookupIdentifier2(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID,
                      ConstraintType_GreaterOrEqual, "0", ConstraintType_SmallerOrEqual, "1");
  ASSERT_EQ(3u, s.size());
}


TEST(ServerIndex, AttachmentRecycling)
{
  const std::string path = "UnitTestsStorage";

  SystemToolbox::RemoveFile(path + "/index");
  FilesystemStorage storage(path);
  SQLiteDatabaseWrapper db;   // The SQLite DB is in memory
  db.Open();
  ServerContext context(db, storage, true /* running unit tests */, 10);
  context.SetupJobsEngine(true, false);
  ServerIndex& index = context.GetIndex();

  index.SetMaximumStorageSize(10);

  uint64_t diskSize, uncompressedSize, countPatients, countStudies, countSeries, countInstances;
  index.GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                            countStudies, countSeries, countInstances);

  ASSERT_EQ(0u, countPatients);
  ASSERT_EQ(0u, diskSize);

  ServerIndex::Attachments attachments;

  std::vector<std::string> ids;
  for (int i = 0; i < 10; i++)
  {
    std::string id = boost::lexical_cast<std::string>(i);
    DicomMap instance;
    instance.SetValue(DICOM_TAG_PATIENT_ID, "patient-" + id, false);
    instance.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "study-" + id, false);
    instance.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "series-" + id, false);
    instance.SetValue(DICOM_TAG_SOP_INSTANCE_UID, "instance-" + id, false);
    instance.SetValue(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.1", false);  // CR image

    ParsedDicomFile dicom(instance, GetDefaultDicomEncoding(), false /* be strict */);

    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));

    std::map<MetadataType, std::string> instanceMetadata;

    {
      DicomMap summary;
      DicomSequencesMap sequences;
      OrthancConfiguration::DefaultExtractDicomSummary(summary, toStore->GetParsedDicomFile());
      toStore->SetOrigin(DicomInstanceOrigin::FromPlugins());

      DicomTransferSyntax transferSyntax;
      bool hasTransferSyntax = dicom.LookupTransferSyntax(transferSyntax);
      ASSERT_EQ(StoreStatus_Success, index.Store(
                  instanceMetadata, summary, sequences, attachments, toStore->GetMetadata(),
                  toStore->GetOrigin(), false /* don't overwrite */,
                  hasTransferSyntax, transferSyntax, true /* pixel data offset */, 42, false));
    }
    
    ASSERT_EQ(7u, instanceMetadata.size());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_RemoteAet) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_ReceptionDate) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_TransferSyntax) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_SopClassUid) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_PixelDataOffset) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_MainDicomTagsSignature) != instanceMetadata.end());

    ASSERT_EQ("42", instanceMetadata[MetadataType_Instance_PixelDataOffset]);

    // The default transfer syntax depends on the OS endianness
    std::string s = instanceMetadata[MetadataType_Instance_TransferSyntax];
    ASSERT_TRUE(s == "1.2.840.10008.1.2.1" ||
                s == "1.2.840.10008.1.2.2");

    ASSERT_EQ("1.2.840.10008.5.1.4.1.1.1", instanceMetadata[MetadataType_Instance_SopClassUid]);

    DicomInstanceHasher hasher(instance);
    ids.push_back(hasher.HashPatient());
    ids.push_back(hasher.HashStudy());
    ids.push_back(hasher.HashSeries());
    ids.push_back(hasher.HashInstance());
  }

  index.GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                            countStudies, countSeries, countInstances);
  ASSERT_EQ(10u, countPatients);
  ASSERT_EQ(0u, diskSize);

  for (size_t i = 0; i < ids.size(); i++)
  {
    FileInfo info(Toolbox::GenerateUuid(), FileContentType_Dicom, 1, "md5");
    int64_t revision = -1;
    index.AddAttachment(revision, info, ids[i], false /* no previous revision */, -1, "");
    ASSERT_EQ(0, revision);

    index.GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                              countStudies, countSeries, countInstances);
    ASSERT_GE(10u, diskSize);
  }

  // Because the DB is in memory, the SQLite index must not have been created
  ASSERT_FALSE(SystemToolbox::IsRegularFile(path + "/index"));

  context.Stop();
  db.Close();
}


TEST(ServerIndex, NormalizeIdentifier)
{
  ASSERT_EQ("H^L.LO", ServerToolbox::NormalizeIdentifier("   Hé^l.LO  %_  "));
  ASSERT_EQ("1.2.840.113619.2.176.2025", ServerToolbox::NormalizeIdentifier("   1.2.840.113619.2.176.2025  "));
}


TEST(ServerIndex, Overwrite)
{
  // Create a dummy 1x1 image
  Image image(PixelFormat_Grayscale8, 1, 1, false);
  reinterpret_cast<uint8_t*>(image.GetBuffer()) [0] = 128;

  for (unsigned int i = 0; i < 2; i++)
  {
    bool overwrite = (i == 0);

    MemoryStorageArea storage;
    SQLiteDatabaseWrapper db;   // The SQLite DB is in memory
    db.Open();
    ServerContext context(db, storage, true /* running unit tests */, 10);
    context.SetupJobsEngine(true, false);
    context.SetCompressionEnabled(true);

    DicomMap instance;
    instance.SetValue(DICOM_TAG_PATIENT_ID, "patient", false);
    instance.SetValue(DICOM_TAG_PATIENT_NAME, "name", false);
    instance.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "study", false);
    instance.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "series", false);
    instance.SetValue(DICOM_TAG_SOP_INSTANCE_UID, "sop", false);
    instance.SetValue(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.1", false);  // CR image

    DicomInstanceHasher hasher(instance);
    std::string id = hasher.HashInstance();
    context.SetOverwriteInstances(overwrite);

    uint64_t diskSize, uncompressedSize, countPatients, countStudies, countSeries, countInstances;
    context.GetIndex().GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                                           countStudies, countSeries, countInstances);

    ASSERT_EQ(0u, countInstances);
    ASSERT_EQ(0u, diskSize);

    {
      ParsedDicomFile dicom(instance, GetDefaultDicomEncoding(), false /* be strict */);

      // Add a pixel data so as to have one "FileContentType_DicomUntilPixelData"
      // (because of "context.SetCompressionEnabled(true)")
      dicom.EmbedImage(image);
      
      DicomInstanceHasher hasher(instance);
      
      std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));
      toStore->SetOrigin(DicomInstanceOrigin::FromPlugins());
      ASSERT_EQ(id, hasher.HashInstance());

      std::string id2;
      ServerContext::StoreResult result = context.Store(id2, *toStore, StoreInstanceMode_Default);
      ASSERT_EQ(StoreStatus_Success, result.GetStatus());
      ASSERT_EQ(id, id2);
    }

    {
      FileInfo nope;
      int64_t revision;
      ASSERT_FALSE(context.GetIndex().LookupAttachment(nope, revision, id, FileContentType_DicomAsJson));
    }

    FileInfo dicom1, pixelData1;
    int64_t revision;
    ASSERT_TRUE(context.GetIndex().LookupAttachment(dicom1, revision, id, FileContentType_Dicom));
    ASSERT_EQ(0, revision);
    revision = -1;
    ASSERT_TRUE(context.GetIndex().LookupAttachment(pixelData1, revision, id, FileContentType_DicomUntilPixelData));
    ASSERT_EQ(0, revision);

    context.GetIndex().GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                                           countStudies, countSeries, countInstances);
    ASSERT_EQ(1u, countInstances);
    ASSERT_EQ(dicom1.GetCompressedSize() + pixelData1.GetCompressedSize(), diskSize);
    ASSERT_EQ(dicom1.GetUncompressedSize() + pixelData1.GetUncompressedSize(), uncompressedSize);

    Json::Value tmp;
    context.ReadDicomAsJson(tmp, id);
    ASSERT_EQ("name", tmp["0010,0010"]["Value"].asString());
    
    {
      ServerContext::DicomCacheLocker locker(context, id);
      std::string tmp;
      locker.GetDicom().GetTagValue(tmp, DICOM_TAG_PATIENT_NAME);
      ASSERT_EQ("name", tmp);
    }

    {
      DicomMap instance2;
      instance2.Assign(instance);
      instance2.SetValue(DICOM_TAG_PATIENT_NAME, "overwritten", false);

      ParsedDicomFile dicom(instance2, GetDefaultDicomEncoding(), false /* be strict */);

      // Add a pixel data so as to have one "FileContentType_DicomUntilPixelData"
      dicom.EmbedImage(image);

      std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));
      toStore->SetOrigin(DicomInstanceOrigin::FromPlugins());

      std::string id2;
      ServerContext::StoreResult result = context.Store(id2, *toStore, StoreInstanceMode_Default);
      ASSERT_EQ(overwrite ? StoreStatus_Success : StoreStatus_AlreadyStored, result.GetStatus());
      ASSERT_EQ(id, id2);
    }

    {
      FileInfo nope;
      int64_t revision;
      ASSERT_FALSE(context.GetIndex().LookupAttachment(nope, revision, id, FileContentType_DicomAsJson));
    }

    FileInfo dicom2, pixelData2;
    ASSERT_TRUE(context.GetIndex().LookupAttachment(dicom2, revision, id, FileContentType_Dicom));
    ASSERT_EQ(0, revision);
    revision = -1;
    ASSERT_TRUE(context.GetIndex().LookupAttachment(pixelData2, revision, id, FileContentType_DicomUntilPixelData));
    ASSERT_EQ(0, revision);

    context.GetIndex().GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                                           countStudies, countSeries, countInstances);
    ASSERT_EQ(1u, countInstances);
    ASSERT_EQ(dicom2.GetCompressedSize() + pixelData2.GetCompressedSize(), diskSize);
    ASSERT_EQ(dicom2.GetUncompressedSize() + pixelData2.GetUncompressedSize(), uncompressedSize);

    if (overwrite)
    {
      ASSERT_NE(dicom1.GetUuid(), dicom2.GetUuid());
      ASSERT_NE(pixelData1.GetUuid(), pixelData2.GetUuid());
      ASSERT_NE(dicom1.GetUncompressedSize(), dicom2.GetUncompressedSize());
      ASSERT_NE(pixelData1.GetUncompressedSize(), pixelData2.GetUncompressedSize());
    
      context.ReadDicomAsJson(tmp, id);
      ASSERT_EQ("overwritten", tmp["0010,0010"]["Value"].asString());
    
      {
        ServerContext::DicomCacheLocker locker(context, id);
        std::string tmp;
        locker.GetDicom().GetTagValue(tmp, DICOM_TAG_PATIENT_NAME);
        ASSERT_EQ("overwritten", tmp);
      }
    }
    else
    {
      ASSERT_EQ(dicom1.GetUuid(), dicom2.GetUuid());
      ASSERT_EQ(pixelData1.GetUuid(), pixelData2.GetUuid());
      ASSERT_EQ(dicom1.GetUncompressedSize(), dicom2.GetUncompressedSize());
      ASSERT_EQ(pixelData1.GetUncompressedSize(), pixelData2.GetUncompressedSize());

      context.ReadDicomAsJson(tmp, id);
      ASSERT_EQ("name", tmp["0010,0010"]["Value"].asString());
    
      {
        ServerContext::DicomCacheLocker locker(context, id);
        std::string tmp;
        locker.GetDicom().GetTagValue(tmp, DICOM_TAG_PATIENT_NAME);
        ASSERT_EQ("name", tmp);
      }
    }

    context.Stop();
    db.Close();
  }
}


TEST(ServerIndex, DicomUntilPixelData)
{
  // Create a dummy 1x1 image
  Image image(PixelFormat_Grayscale8, 1, 1, false);
  reinterpret_cast<uint8_t*>(image.GetBuffer()) [0] = 128;

  for (unsigned int i = 0; i < 2; i++)
  {
    const bool compression = (i == 0);
    
    MemoryStorageArea storage;
    SQLiteDatabaseWrapper db;   // The SQLite DB is in memory
    db.Open();
    ServerContext context(db, storage, true /* running unit tests */, 10);
    context.SetupJobsEngine(true, false);
    context.SetCompressionEnabled(compression);

    for (unsigned int j = 0; j < 2; j++)
    {
      const bool withPixelData = (j == 0);

      ParsedDicomFile dicom(true);
      
      if (withPixelData)
      {
        dicom.EmbedImage(image);
      }

      std::string id;
      size_t dicomSize;

      {
        std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));
        dicomSize = toStore->GetBufferSize();
        toStore->SetOrigin(DicomInstanceOrigin::FromPlugins());
        ServerContext::StoreResult result = context.Store(id, *toStore, StoreInstanceMode_Default);
        ASSERT_EQ(StoreStatus_Success, result.GetStatus());
      }

      std::set<FileContentType> attachments;
      context.GetIndex().ListAvailableAttachments(attachments, id, ResourceType_Instance);

      ASSERT_TRUE(attachments.find(FileContentType_Dicom) != attachments.end());
      
      if (compression &&
          withPixelData)
      {
        ASSERT_EQ(2u, attachments.size());
        ASSERT_TRUE(attachments.find(FileContentType_DicomUntilPixelData) != attachments.end());
      }
      else
      {
        ASSERT_EQ(1u, attachments.size());
      }

      std::string s;
      int64_t revision;
      bool found = context.GetIndex().LookupMetadata(s, revision, id, ResourceType_Instance,
                                                     MetadataType_Instance_PixelDataOffset);
      
      if (withPixelData)
      {
        ASSERT_TRUE(found);
        ASSERT_EQ(0, revision);
        ASSERT_GT(boost::lexical_cast<int>(s), 128 /* length of the DICOM preamble */);
        ASSERT_LT(boost::lexical_cast<size_t>(s), dicomSize);
      }
      else
      {
        ASSERT_FALSE(found);        
      }
    }
  }
}
