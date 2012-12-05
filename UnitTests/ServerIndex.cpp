#include "gtest/gtest.h"

#include "../OrthancServer/DatabaseWrapper.h"

#include <ctype.h>
#include <glog/logging.h>

using namespace Orthanc;

namespace
{
  class ServerIndexListener : public IServerIndexListener
  {
  public:
    std::set<std::string> deletedFiles_;
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

    virtual void SignalFileDeleted(const std::string& fileUuid)
    {
      deletedFiles_.insert(fileUuid);
      LOG(INFO) << "A file must be removed: " << fileUuid;
    }                                
  };
}


TEST(DatabaseWrapper, Simple)
{
  ServerIndexListener listener;
  DatabaseWrapper index(listener);

  int64_t a[] = {
    index.CreateResource("a", ResourceType_Patient),   // 0
    index.CreateResource("b", ResourceType_Study),     // 1
    index.CreateResource("c", ResourceType_Series),    // 2
    index.CreateResource("d", ResourceType_Instance),  // 3
    index.CreateResource("e", ResourceType_Instance),  // 4
    index.CreateResource("f", ResourceType_Instance),  // 5
    index.CreateResource("g", ResourceType_Study)      // 6
  };

  ASSERT_EQ("a", index.GetPublicId(a[0]));
  ASSERT_EQ("b", index.GetPublicId(a[1]));
  ASSERT_EQ("c", index.GetPublicId(a[2]));
  ASSERT_EQ("d", index.GetPublicId(a[3]));
  ASSERT_EQ("e", index.GetPublicId(a[4]));
  ASSERT_EQ("f", index.GetPublicId(a[5]));
  ASSERT_EQ("g", index.GetPublicId(a[6]));

  {
    Json::Value t;
    index.GetAllPublicIds(t, ResourceType_Patient);

    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("a", t[0u].asString());

    index.GetAllPublicIds(t, ResourceType_Series);
    ASSERT_EQ(1u, t.size());
    ASSERT_EQ("c", t[0u].asString());

    index.GetAllPublicIds(t, ResourceType_Study);
    ASSERT_EQ(2u, t.size());

    index.GetAllPublicIds(t, ResourceType_Instance);
    ASSERT_EQ(3u, t.size());
  }

  index.SetGlobalProperty(GlobalProperty_FlushSleep, "World");

  index.AttachChild(a[0], a[1]);
  index.AttachChild(a[1], a[2]);
  index.AttachChild(a[2], a[3]);
  index.AttachChild(a[2], a[4]);
  index.AttachChild(a[6], a[5]);

  int64_t parent;
  ASSERT_FALSE(index.LookupParent(parent, a[0]));
  ASSERT_TRUE(index.LookupParent(parent, a[1])); ASSERT_EQ(a[0], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[2])); ASSERT_EQ(a[1], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[3])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[4])); ASSERT_EQ(a[2], parent);
  ASSERT_TRUE(index.LookupParent(parent, a[5])); ASSERT_EQ(a[6], parent);
  ASSERT_FALSE(index.LookupParent(parent, a[6]));

  std::string s;
  
  ASSERT_FALSE(index.GetParentPublicId(s, a[0]));
  ASSERT_FALSE(index.GetParentPublicId(s, a[6]));
  ASSERT_TRUE(index.GetParentPublicId(s, a[1])); ASSERT_EQ("a", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[2])); ASSERT_EQ("b", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[3])); ASSERT_EQ("c", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[4])); ASSERT_EQ("c", s);
  ASSERT_TRUE(index.GetParentPublicId(s, a[5])); ASSERT_EQ("g", s);

  std::list<std::string> l;
  index.GetChildrenPublicId(l, a[0]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("b", l.front());
  index.GetChildrenPublicId(l, a[1]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("c", l.front());
  index.GetChildrenPublicId(l, a[3]); ASSERT_EQ(0u, l.size()); 
  index.GetChildrenPublicId(l, a[4]); ASSERT_EQ(0u, l.size()); 
  index.GetChildrenPublicId(l, a[5]); ASSERT_EQ(0u, l.size()); 
  index.GetChildrenPublicId(l, a[6]); ASSERT_EQ(1u, l.size()); ASSERT_EQ("f", l.front());

  index.GetChildrenPublicId(l, a[2]); ASSERT_EQ(2u, l.size()); 
  if (l.front() == "d")
  {
    ASSERT_EQ("e", l.back());
  }
  else
  {
    ASSERT_EQ("d", l.back());
    ASSERT_EQ("e", l.front());
  }

  index.AddAttachment(a[4], FileInfo("my json file", FileContentType_Json, 42, CompressionType_Zlib, 21));
  index.AddAttachment(a[4], FileInfo("my dicom file", FileContentType_Dicom, 42));
  index.AddAttachment(a[6], FileInfo("world", FileContentType_Dicom, 44));
  index.SetMetadata(a[4], MetadataType_Instance_RemoteAet, "PINNACLE");

  ASSERT_EQ(21u + 42u + 44u, index.GetTotalCompressedSize());
  ASSERT_EQ(42u + 42u + 44u, index.GetTotalUncompressedSize());

  DicomMap m;
  m.SetValue(0x0010, 0x0010, "PatientName");
  index.SetMainDicomTags(a[3], m);

  int64_t b;
  ResourceType t;
  ASSERT_TRUE(index.LookupResource("g", b, t));
  ASSERT_EQ(7, b);
  ASSERT_EQ(ResourceType_Study, t);

  ASSERT_TRUE(index.LookupMetadata(s, a[4], MetadataType_Instance_RemoteAet));
  ASSERT_FALSE(index.LookupMetadata(s, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("PINNACLE", s);
  ASSERT_EQ("PINNACLE", index.GetMetadata(a[4], MetadataType_Instance_RemoteAet));
  ASSERT_EQ("None", index.GetMetadata(a[4], MetadataType_Instance_IndexInSeries, "None"));

  ASSERT_TRUE(index.LookupGlobalProperty(s, GlobalProperty_FlushSleep));
  ASSERT_FALSE(index.LookupGlobalProperty(s, static_cast<GlobalProperty>(42)));
  ASSERT_EQ("World", s);
  ASSERT_EQ("World", index.GetGlobalProperty(GlobalProperty_FlushSleep));
  ASSERT_EQ("None", index.GetGlobalProperty(static_cast<GlobalProperty>(42), "None"));

  FileInfo att;
  ASSERT_TRUE(index.LookupAttachment(att, a[4], FileContentType_Json));
  ASSERT_EQ("my json file", att.GetUuid());
  ASSERT_EQ(21u, att.GetCompressedSize());
  ASSERT_EQ(42u, att.GetUncompressedSize());
  ASSERT_EQ(CompressionType_Zlib, att.GetCompressionType());

  ASSERT_EQ(0u, listener.deletedFiles_.size());
  ASSERT_EQ(7u, index.GetTableRecordCount("Resources")); 
  ASSERT_EQ(3u, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(1u, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(1u, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[0]);

  ASSERT_EQ(2u, listener.deletedFiles_.size());
  ASSERT_FALSE(listener.deletedFiles_.find("my json file") == listener.deletedFiles_.end());
  ASSERT_FALSE(listener.deletedFiles_.find("my dicom file") == listener.deletedFiles_.end());

  ASSERT_EQ(2u, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(0u, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(1u, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(0u, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[5]);
  ASSERT_EQ(0u, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(0u, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(2u, index.GetTableRecordCount("GlobalProperties"));

  ASSERT_EQ(3u, listener.deletedFiles_.size());
  ASSERT_FALSE(listener.deletedFiles_.find("world") == listener.deletedFiles_.end());
}




TEST(DatabaseWrapper, Upward)
{
  ServerIndexListener listener;
  DatabaseWrapper index(listener);

  int64_t a[] = {
    index.CreateResource("a", ResourceType_Patient),   // 0
    index.CreateResource("b", ResourceType_Study),     // 1
    index.CreateResource("c", ResourceType_Series),    // 2
    index.CreateResource("d", ResourceType_Instance),  // 3
    index.CreateResource("e", ResourceType_Instance),  // 4
    index.CreateResource("f", ResourceType_Study),     // 5
    index.CreateResource("g", ResourceType_Series),    // 6
    index.CreateResource("h", ResourceType_Series)     // 7
  };

  index.AttachChild(a[0], a[1]);
  index.AttachChild(a[1], a[2]);
  index.AttachChild(a[2], a[3]);
  index.AttachChild(a[2], a[4]);
  index.AttachChild(a[1], a[6]);
  index.AttachChild(a[0], a[5]);
  index.AttachChild(a[5], a[7]);

  {
    Json::Value j;
    index.GetChildren(j, a[0]);
    ASSERT_EQ(2u, j.size());
    ASSERT_TRUE((j[0u] == "b" && j[1u] == "f") ||
                (j[1u] == "b" && j[0u] == "f"));

    index.GetChildren(j, a[1]);
    ASSERT_EQ(2u, j.size());
    ASSERT_TRUE((j[0u] == "c" && j[1u] == "g") ||
                (j[1u] == "c" && j[0u] == "g"));

    index.GetChildren(j, a[2]);
    ASSERT_EQ(2u, j.size());
    ASSERT_TRUE((j[0u] == "d" && j[1u] == "e") ||
                (j[1u] == "d" && j[0u] == "e"));

    index.GetChildren(j, a[3]); ASSERT_EQ(0u, j.size());
    index.GetChildren(j, a[4]); ASSERT_EQ(0u, j.size());
    index.GetChildren(j, a[5]); ASSERT_EQ(1u, j.size()); ASSERT_EQ("h", j[0u].asString());
    index.GetChildren(j, a[6]); ASSERT_EQ(0u, j.size());
    index.GetChildren(j, a[7]); ASSERT_EQ(0u, j.size());
  }

  listener.Reset();
  index.DeleteResource(a[3]);
  ASSERT_EQ("c", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Series, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[4]);
  ASSERT_EQ("b", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Study, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[7]);
  ASSERT_EQ("a", listener.ancestorId_);
  ASSERT_EQ(ResourceType_Patient, listener.ancestorType_);

  listener.Reset();
  index.DeleteResource(a[6]);
  ASSERT_EQ("", listener.ancestorId_);  // No more ancestor
}
