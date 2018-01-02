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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../Core/FileStorage/FilesystemStorage.h"
#include "../Core/Logging.h"
#include "../OrthancServer/DatabaseWrapper.h"
#include "../OrthancServer/ServerContext.h"
#include "../OrthancServer/ServerIndex.h"
#include "../OrthancServer/Search/LookupIdentifierQuery.h"

#include <ctype.h>
#include <algorithm>

using namespace Orthanc;

namespace
{
  enum DatabaseWrapperClass
  {
    DatabaseWrapperClass_SQLite
  };


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
    {
      ancestorId_ = publicId;
      ancestorType_ = type;
    }

    virtual void SignalFileDeleted(const FileInfo& info)
    {
      const std::string fileUuid = info.GetUuid();
      deletedFiles_.push_back(fileUuid);
      LOG(INFO) << "A file must be removed: " << fileUuid;
    }       

    virtual void SignalChange(const ServerIndexChange& change)
    {
      if (change.GetChangeType() == ChangeType_Deleted)
      {
        deletedResources_.push_back(change.GetPublicId());        
      }

      LOG(INFO) << "Change related to resource " << change.GetPublicId() << " of type " 
                << EnumerationToString(change.GetResourceType()) << ": " 
                << EnumerationToString(change.GetChangeType());
    }

  };


  class DatabaseWrapperTest : public ::testing::TestWithParam<DatabaseWrapperClass>
  {
  protected:
    std::auto_ptr<TestDatabaseListener> listener_;
    std::auto_ptr<IDatabaseWrapper> index_;

    DatabaseWrapperTest()
    {
    }

    virtual void SetUp() 
    {
      listener_.reset(new TestDatabaseListener);

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
          index_.reset(new DatabaseWrapper());
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      index_->SetListener(*listener_);
      index_->Open();
    }

    virtual void TearDown()
    {
      index_->Close();
      index_.reset(NULL);
      listener_.reset(NULL);
    }

    void CheckTableRecordCount(uint32_t expected, const char* table)
    {
      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          ASSERT_EQ(expected, sqlite->GetTableRecordCount(table));
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void CheckNoParent(int64_t id)
    {
      std::string s;

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          ASSERT_FALSE(sqlite->GetParentPublicId(s, id));
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void CheckParentPublicId(const char* expected, int64_t id)
    {
      std::string s;

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          ASSERT_TRUE(sqlite->GetParentPublicId(s, id));
          ASSERT_EQ(expected, s);
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void CheckNoChild(int64_t id)
    {
      std::list<std::string> j;

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          sqlite->GetChildren(j, id);
          ASSERT_EQ(0u, j.size());
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void CheckOneChild(const char* expected, int64_t id)
    {
      std::list<std::string> j;

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          sqlite->GetChildren(j, id);
          ASSERT_EQ(1u, j.size());
          ASSERT_EQ(expected, j.front());
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void CheckTwoChildren(const char* expected1,
                          const char* expected2,
                          int64_t id)
    {
      std::list<std::string> j;

      switch (GetParam())
      {
        case DatabaseWrapperClass_SQLite:
        {
          DatabaseWrapper* sqlite = dynamic_cast<DatabaseWrapper*>(index_.get());
          sqlite->GetChildren(j, id);
          ASSERT_EQ(2u, j.size());
          ASSERT_TRUE((expected1 == j.front() && expected2 == j.back()) ||
                      (expected1 == j.back() && expected2 == j.front()));                    
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }


    void DoLookup(std::list<std::string>& result,
                  ResourceType level,
                  const DicomTag& tag,
                  const std::string& value)
    {
      LookupIdentifierQuery query(level);
      query.AddConstraint(tag, IdentifierConstraintType_Equal, value);
      query.Apply(result, *index_);
    }

  };
}


INSTANTIATE_TEST_CASE_P(DatabaseWrapperName,
                        DatabaseWrapperTest,
                        ::testing::Values(DatabaseWrapperClass_SQLite));


TEST_P(DatabaseWrapperTest, Simple)
{
  int64_t a[] = {
    index_->CreateResource("a", ResourceType_Patient),   // 0
    index_->CreateResource("b", ResourceType_Study),     // 1
    index_->CreateResource("c", ResourceType_Series),    // 2
    index_->CreateResource("d", ResourceType_Instance),  // 3
    index_->CreateResource("e", ResourceType_Instance),  // 4
    index_->CreateResource("f", ResourceType_Instance),  // 5
    index_->CreateResource("g", ResourceType_Study)      // 6
  };

  ASSERT_EQ("a", index_->GetPublicId(a[0]));
  ASSERT_EQ("b", index_->GetPublicId(a[1]));
  ASSERT_EQ("c", index_->GetPublicId(a[2]));
  ASSERT_EQ("d", index_->GetPublicId(a[3]));
  ASSERT_EQ("e", index_->GetPublicId(a[4]));
  ASSERT_EQ("f", index_->GetPublicId(a[5]));
  ASSERT_EQ("g", index_->GetPublicId(a[6]));

  ASSERT_EQ(ResourceType_Patient, index_->GetResourceType(a[0]));
  ASSERT_EQ(ResourceType_Study, index_->GetResourceType(a[1]));
  ASSERT_EQ(ResourceType_Series, index_->GetResourceType(a[2]));
  ASSERT_EQ(ResourceType_Instance, index_->GetResourceType(a[3]));
  ASSERT_EQ(ResourceType_Instance, index_->GetResourceType(a[4]));
  ASSERT_EQ(ResourceType_Instance, index_->GetResourceType(a[5]));
  ASSERT_EQ(ResourceType_Study, index_->GetResourceType(a[6]));

  {
    std::list<std::string> t;
    index_->GetAllPublicIds(t, ResourceType_Patient);

    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("a", t.front());

    index_->GetAllPublicIds(t, ResourceType_Series);
    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("c", t.front());

    index_->GetAllPublicIds(t, ResourceType_Study);
    ASSERT_EQ(2u, t.size());

    index_->GetAllPublicIds(t, ResourceType_Instance);
    ASSERT_EQ(3u, t.size());
  }

  index_->SetGlobalProperty(GlobalProperty_FlushSleep, "World");

  index_->AttachChild(a[0], a[1]);
  index_->AttachChild(a[1], a[2]);
  index_->AttachChild(a[2], a[3]);
  index_->AttachChild(a[2], a[4]);
  index_->AttachChild(a[6], a[5]);

  int64_t parent;
  ASSERT_FALSE(index_->LookupParent(parent, a[0]));
  ASSERT_TRUE(index_->LookupParent(parent, a[1])); ASSERT_EQ(a[0], parent);
  ASSERT_TRUE(index_->LookupParent(parent, a[2])); ASSERT_EQ(a[1], parent);
  ASSERT_TRUE(index_->LookupParent(parent, a[3])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index_->LookupParent(parent, a[4])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index_->LookupParent(parent, a[5])); ASSERT_EQ(a[6], parent);
  ASSERT_FALSE(index_->LookupParent(parent, a[6]));

  std::string s;

  CheckNoParent(a[0]);
  CheckNoParent(a[6]);
  CheckParentPublicId("a", a[1]);
  CheckParentPublicId("b", a[2]);
  CheckParentPublicId("c", a[3]);
  CheckParentPublicId("c", a[4]);
  CheckParentPublicId("g", a[5]);

  std::list<std::string> l;
  index_->GetChildrenPublicId(l, a[0]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("b", l.front());
  index_->GetChildrenPublicId(l, a[1]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("c", l.front());
  index_->GetChildrenPublicId(l, a[3]); ASSERT_EQ(0u, l.size()); 
  index_->GetChildrenPublicId(l, a[4]); ASSERT_EQ(0u, l.size()); 
  index_->GetChildrenPublicId(l, a[5]); ASSERT_EQ(0u, l.size()); 
  index_->GetChildrenPublicId(l, a[6]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("f", l.front());

  index_->GetChildrenPublicId(l, a[2]); ASSERT_EQ(2u, l.size()); 
  if (l.front() == "d")
  {
    ASSERT_EQ("e", l.back());
  }
  else
  {
    ASSERT_EQ("d", l.back());
    ASSERT_EQ("e", l.front());
  }

  std::list<MetadataType> md;
  index_->ListAvailableMetadata(md, a[4]);
  ASSERT_EQ(0u, md.size());

  index_->AddAttachment(a[4], FileInfo("my json file", FileContentType_DicomAsJson, 42, "md5", 
                                       CompressionType_ZlibWithSize, 21, "compressedMD5"));
  index_->AddAttachment(a[4], FileInfo("my dicom file", FileContentType_Dicom, 42, "md5"));
  index_->AddAttachment(a[6], FileInfo("world", FileContentType_Dicom, 44, "md5"));
  index_->SetMetadata(a[4], MetadataType_Instance_RemoteAet, "PINNACLE");
  
  index_->ListAvailableMetadata(md, a[4]);
  ASSERT_EQ(1u, md.size());
  ASSERT_EQ(MetadataType_Instance_RemoteAet, md.front());
  index_->SetMetadata(a[4], MetadataType_ModifiedFrom, "TUTU");
  index_->ListAvailableMetadata(md, a[4]);
  ASSERT_EQ(2u, md.size());

  std::map<MetadataType, std::string> md2;
  index_->GetAllMetadata(md2, a[4]);
  ASSERT_EQ(2u, md2.size());
  ASSERT_EQ("TUTU", md2[MetadataType_ModifiedFrom]);
  ASSERT_EQ("PINNACLE", md2[MetadataType_Instance_RemoteAet]);

  index_->DeleteMetadata(a[4], MetadataType_ModifiedFrom);
  index_->ListAvailableMetadata(md, a[4]);
  ASSERT_EQ(1u, md.size());
  ASSERT_EQ(MetadataType_Instance_RemoteAet, md.front());

  index_->GetAllMetadata(md2, a[4]);
  ASSERT_EQ(1u, md2.size());
  ASSERT_EQ("PINNACLE", md2[MetadataType_Instance_RemoteAet]);


  ASSERT_EQ(21u + 42u + 44u, index_->GetTotalCompressedSize());
  ASSERT_EQ(42u + 42u + 44u, index_->GetTotalUncompressedSize());

  index_->SetMainDicomTag(a[3], DicomTag(0x0010, 0x0010), "PatientName");

  int64_t b;
  ResourceType t;
  ASSERT_TRUE(index_->LookupResource(b, t, "g"));
  ASSERT_EQ(7, b);
  ASSERT_EQ(ResourceType_Study, t);

  ASSERT_TRUE(index_->LookupMetadata(s, a[4], MetadataType_Instance_RemoteAet));
  ASSERT_FALSE(index_->LookupMetadata(s, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("PINNACLE", s);

  std::string u;
  ASSERT_TRUE(index_->LookupMetadata(u, a[4], MetadataType_Instance_RemoteAet));
  ASSERT_EQ("PINNACLE", u);
  ASSERT_FALSE(index_->LookupMetadata(u, a[4], MetadataType_Instance_IndexInSeries));

  ASSERT_TRUE(index_->LookupGlobalProperty(s, GlobalProperty_FlushSleep));
  ASSERT_FALSE(index_->LookupGlobalProperty(s, static_cast<GlobalProperty>(42)));
  ASSERT_EQ("World", s);

  FileInfo att;
  ASSERT_TRUE(index_->LookupAttachment(att, a[4], FileContentType_DicomAsJson));
  ASSERT_EQ("my json file", att.GetUuid());
  ASSERT_EQ(21u, att.GetCompressedSize());
  ASSERT_EQ("md5", att.GetUncompressedMD5());
  ASSERT_EQ("compressedMD5", att.GetCompressedMD5());
  ASSERT_EQ(42u, att.GetUncompressedSize());
  ASSERT_EQ(CompressionType_ZlibWithSize, att.GetCompressionType());

  ASSERT_TRUE(index_->LookupAttachment(att, a[6], FileContentType_Dicom));
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

  index_->DeleteResource(a[0]);
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

  index_->DeleteResource(a[5]);
  ASSERT_EQ(7u, listener_->deletedResources_.size());

  CheckTableRecordCount(0, "Resources");
  CheckTableRecordCount(0, "AttachedFiles");
  CheckTableRecordCount(2, "GlobalProperties");

  ASSERT_EQ(3u, listener_->deletedFiles_.size());
  ASSERT_FALSE(std::find(listener_->deletedFiles_.begin(), 
                         listener_->deletedFiles_.end(),
                         "world") == listener_->deletedFiles_.end());
}




TEST_P(DatabaseWrapperTest, Upward)
{
  int64_t a[] = {
    index_->CreateResource("a", ResourceType_Patient),   // 0
    index_->CreateResource("b", ResourceType_Study),     // 1
    index_->CreateResource("c", ResourceType_Series),    // 2
    index_->CreateResource("d", ResourceType_Instance),  // 3
    index_->CreateResource("e", ResourceType_Instance),  // 4
    index_->CreateResource("f", ResourceType_Study),     // 5
    index_->CreateResource("g", ResourceType_Series),    // 6
    index_->CreateResource("h", ResourceType_Series)     // 7
  };

  index_->AttachChild(a[0], a[1]);
  index_->AttachChild(a[1], a[2]);
  index_->AttachChild(a[2], a[3]);
  index_->AttachChild(a[2], a[4]);
  index_->AttachChild(a[1], a[6]);
  index_->AttachChild(a[0], a[5]);
  index_->AttachChild(a[5], a[7]);

  CheckTwoChildren("b", "f", a[0]);
  CheckTwoChildren("c", "g", a[1]);
  CheckTwoChildren("d", "e", a[2]);
  CheckNoChild(a[3]);
  CheckNoChild(a[4]);
  CheckOneChild("h", a[5]);
  CheckNoChild(a[6]);
  CheckNoChild(a[7]);

  listener_->Reset();
  index_->DeleteResource(a[3]);
  ASSERT_EQ("c", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Series, listener_->ancestorType_);

  listener_->Reset();
  index_->DeleteResource(a[4]);
  ASSERT_EQ("b", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Study, listener_->ancestorType_);

  listener_->Reset();
  index_->DeleteResource(a[7]);
  ASSERT_EQ("a", listener_->ancestorId_);
  ASSERT_EQ(ResourceType_Patient, listener_->ancestorType_);

  listener_->Reset();
  index_->DeleteResource(a[6]);
  ASSERT_EQ("", listener_->ancestorId_);  // No more ancestor
}


TEST_P(DatabaseWrapperTest, PatientRecycling)
{
  std::vector<int64_t> patients;
  for (int i = 0; i < 10; i++)
  {
    std::string p = "Patient " + boost::lexical_cast<std::string>(i);
    patients.push_back(index_->CreateResource(p, ResourceType_Patient));
    index_->AddAttachment(patients[i], FileInfo(p, FileContentType_Dicom, i + 10, 
                                                "md5-" + boost::lexical_cast<std::string>(i)));
    ASSERT_FALSE(index_->IsProtectedPatient(patients[i]));
  }

  CheckTableRecordCount(10u, "Resources");
  CheckTableRecordCount(10u, "PatientRecyclingOrder");

  listener_->Reset();
  ASSERT_EQ(0u, listener_->deletedResources_.size());

  index_->DeleteResource(patients[5]);
  index_->DeleteResource(patients[0]);
  ASSERT_EQ(2u, listener_->deletedResources_.size());

  CheckTableRecordCount(8u, "Resources");
  CheckTableRecordCount(8u, "PatientRecyclingOrder");

  ASSERT_EQ(2u, listener_->deletedFiles_.size());
  ASSERT_EQ("Patient 5", listener_->deletedFiles_[0]);
  ASSERT_EQ("Patient 0", listener_->deletedFiles_[1]);

  int64_t p;
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[1]);
  index_->DeleteResource(p);
  ASSERT_EQ(3u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[2]);
  index_->DeleteResource(p);
  ASSERT_EQ(4u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[3]);
  index_->DeleteResource(p);
  ASSERT_EQ(5u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[4]);
  index_->DeleteResource(p);
  ASSERT_EQ(6u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[6]);
  index_->DeleteResource(p);
  index_->DeleteResource(patients[8]);
  ASSERT_EQ(8u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[7]);
  index_->DeleteResource(p);
  ASSERT_EQ(9u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[9]);
  index_->DeleteResource(p);
  ASSERT_FALSE(index_->SelectPatientToRecycle(p));
  ASSERT_EQ(10u, listener_->deletedResources_.size());

  ASSERT_EQ(10u, listener_->deletedFiles_.size());

  CheckTableRecordCount(0, "Resources");
  CheckTableRecordCount(0, "PatientRecyclingOrder");
}


TEST_P(DatabaseWrapperTest, PatientProtection)
{
  std::vector<int64_t> patients;
  for (int i = 0; i < 5; i++)
  {
    std::string p = "Patient " + boost::lexical_cast<std::string>(i);
    patients.push_back(index_->CreateResource(p, ResourceType_Patient));
    index_->AddAttachment(patients[i], FileInfo(p, FileContentType_Dicom, i + 10,
                                                "md5-" + boost::lexical_cast<std::string>(i)));
    ASSERT_FALSE(index_->IsProtectedPatient(patients[i]));
  }

  CheckTableRecordCount(5, "Resources");
  CheckTableRecordCount(5, "PatientRecyclingOrder");

  ASSERT_FALSE(index_->IsProtectedPatient(patients[2]));
  index_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "Resources");
  CheckTableRecordCount(4, "PatientRecyclingOrder");

  index_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");
  index_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  index_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  CheckTableRecordCount(5, "Resources");
  index_->SetProtectedPatient(patients[2], true);
  ASSERT_TRUE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");
  index_->SetProtectedPatient(patients[2], false);
  ASSERT_FALSE(index_->IsProtectedPatient(patients[2]));
  CheckTableRecordCount(5, "PatientRecyclingOrder");
  index_->SetProtectedPatient(patients[3], true);
  ASSERT_TRUE(index_->IsProtectedPatient(patients[3]));
  CheckTableRecordCount(4, "PatientRecyclingOrder");

  CheckTableRecordCount(5, "Resources");
  ASSERT_EQ(0u, listener_->deletedFiles_.size());

  // Unprotecting a patient puts it at the last position in the recycling queue
  int64_t p;
  ASSERT_EQ(0u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[0]);
  index_->DeleteResource(p);
  ASSERT_EQ(1u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p, patients[1])); ASSERT_EQ(p, patients[4]);
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[1]);
  index_->DeleteResource(p);
  ASSERT_EQ(2u, listener_->deletedResources_.size());
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[4]);
  index_->DeleteResource(p);
  ASSERT_EQ(3u, listener_->deletedResources_.size());
  ASSERT_FALSE(index_->SelectPatientToRecycle(p, patients[2]));
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[2]);
  index_->DeleteResource(p);
  ASSERT_EQ(4u, listener_->deletedResources_.size());
  // "patients[3]" is still protected
  ASSERT_FALSE(index_->SelectPatientToRecycle(p));

  ASSERT_EQ(4u, listener_->deletedFiles_.size());
  CheckTableRecordCount(1, "Resources");
  CheckTableRecordCount(0, "PatientRecyclingOrder");

  index_->SetProtectedPatient(patients[3], false);
  CheckTableRecordCount(1, "PatientRecyclingOrder");
  ASSERT_FALSE(index_->SelectPatientToRecycle(p, patients[3]));
  ASSERT_TRUE(index_->SelectPatientToRecycle(p, patients[2]));
  ASSERT_TRUE(index_->SelectPatientToRecycle(p)); ASSERT_EQ(p, patients[3]);
  index_->DeleteResource(p);
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
  DatabaseWrapper db;   // The SQLite DB is in memory
  db.Open();
  ServerContext context(db, storage);
  ServerIndex& index = context.GetIndex();

  ASSERT_EQ(1u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence));
  ASSERT_EQ(2u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence));
  ASSERT_EQ(3u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence));
  ASSERT_EQ(4u, index.IncrementGlobalSequence(GlobalProperty_AnonymizationSequence));

  context.Stop();
  db.Close();
}



TEST_P(DatabaseWrapperTest, LookupIdentifier)
{
  int64_t a[] = {
    index_->CreateResource("a", ResourceType_Study),   // 0
    index_->CreateResource("b", ResourceType_Study),   // 1
    index_->CreateResource("c", ResourceType_Study),   // 2
    index_->CreateResource("d", ResourceType_Series)   // 3
  };

  index_->SetIdentifierTag(a[0], DICOM_TAG_STUDY_INSTANCE_UID, "0");
  index_->SetIdentifierTag(a[1], DICOM_TAG_STUDY_INSTANCE_UID, "1");
  index_->SetIdentifierTag(a[2], DICOM_TAG_STUDY_INSTANCE_UID, "0");
  index_->SetIdentifierTag(a[3], DICOM_TAG_SERIES_INSTANCE_UID, "0");

  std::list<std::string> s;

  DoLookup(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, "0");
  ASSERT_EQ(2u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "a") != s.end());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "c") != s.end());

  DoLookup(s, ResourceType_Series, DICOM_TAG_SERIES_INSTANCE_UID, "0");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "d") != s.end());

  DoLookup(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, "1");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "b") != s.end());

  DoLookup(s, ResourceType_Study, DICOM_TAG_STUDY_INSTANCE_UID, "1");
  ASSERT_EQ(1u, s.size());
  ASSERT_TRUE(std::find(s.begin(), s.end(), "b") != s.end());

  DoLookup(s, ResourceType_Series, DICOM_TAG_SERIES_INSTANCE_UID, "1");
  ASSERT_EQ(0u, s.size());

  {
    LookupIdentifierQuery query(ResourceType_Study);
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_GreaterOrEqual, "0");
    query.Apply(s, *index_);
    ASSERT_EQ(3u, s.size());
  }

  {
    LookupIdentifierQuery query(ResourceType_Study);
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_GreaterOrEqual, "0");
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_SmallerOrEqual, "0");
    query.Apply(s, *index_);
    ASSERT_EQ(2u, s.size());
  }

  {
    LookupIdentifierQuery query(ResourceType_Study);
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_GreaterOrEqual, "1");
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_SmallerOrEqual, "1");
    query.Apply(s, *index_);
    ASSERT_EQ(1u, s.size());
  }

  {
    LookupIdentifierQuery query(ResourceType_Study);
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_GreaterOrEqual, "1");
    query.Apply(s, *index_);
    ASSERT_EQ(1u, s.size());
  }

  {
    LookupIdentifierQuery query(ResourceType_Study);
    query.AddConstraint(DICOM_TAG_STUDY_INSTANCE_UID, IdentifierConstraintType_GreaterOrEqual, "2");
    query.Apply(s, *index_);
    ASSERT_EQ(0u, s.size());
  }
}



TEST(ServerIndex, AttachmentRecycling)
{
  const std::string path = "UnitTestsStorage";

  SystemToolbox::RemoveFile(path + "/index");
  FilesystemStorage storage(path);
  DatabaseWrapper db;   // The SQLite DB is in memory
  db.Open();
  ServerContext context(db, storage);
  ServerIndex& index = context.GetIndex();

  index.SetMaximumStorageSize(10);

  Json::Value tmp;
  index.ComputeStatistics(tmp);
  ASSERT_EQ(0, tmp["CountPatients"].asInt());
  ASSERT_EQ(0, boost::lexical_cast<int>(tmp["TotalDiskSize"].asString()));

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

    std::map<MetadataType, std::string> instanceMetadata;
    DicomInstanceToStore toStore;
    toStore.SetSummary(instance);
    ASSERT_EQ(StoreStatus_Success, index.Store(instanceMetadata, toStore, attachments));
    ASSERT_EQ(5u, instanceMetadata.size());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_RemoteAet) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_ReceptionDate) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_TransferSyntax) != instanceMetadata.end());
    ASSERT_TRUE(instanceMetadata.find(MetadataType_Instance_SopClassUid) != instanceMetadata.end());

    // By default, an Explicit VR Little Endian is used by Orthanc
    ASSERT_EQ("1.2.840.10008.1.2.1", instanceMetadata[MetadataType_Instance_TransferSyntax]);

    ASSERT_EQ("1.2.840.10008.5.1.4.1.1.1", instanceMetadata[MetadataType_Instance_SopClassUid]);

    DicomInstanceHasher hasher(instance);
    ids.push_back(hasher.HashPatient());
    ids.push_back(hasher.HashStudy());
    ids.push_back(hasher.HashSeries());
    ids.push_back(hasher.HashInstance());
  }

  index.ComputeStatistics(tmp);
  ASSERT_EQ(10, tmp["CountPatients"].asInt());
  ASSERT_EQ(0, boost::lexical_cast<int>(tmp["TotalDiskSize"].asString()));

  for (size_t i = 0; i < ids.size(); i++)
  {
    FileInfo info(SystemToolbox::GenerateUuid(), FileContentType_Dicom, 1, "md5");
    index.AddAttachment(info, ids[i]);

    index.ComputeStatistics(tmp);
    ASSERT_GE(10, boost::lexical_cast<int>(tmp["TotalDiskSize"].asString()));
  }

  // Because the DB is in memory, the SQLite index must not have been created
  ASSERT_FALSE(SystemToolbox::IsRegularFile(path + "/index"));

  context.Stop();
  db.Close();
}


TEST(LookupIdentifierQuery, NormalizeIdentifier)
{
  ASSERT_EQ("H^L.LO", ServerToolbox::NormalizeIdentifier("   Hé^l.LO  %_  "));
  ASSERT_EQ("1.2.840.113619.2.176.2025", ServerToolbox::NormalizeIdentifier("   1.2.840.113619.2.176.2025  "));
}
