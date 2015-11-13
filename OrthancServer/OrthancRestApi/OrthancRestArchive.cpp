/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../DicomDirWriter.h"
#include "../../Core/FileStorage/StorageAccessor.h"
#include "../../Core/Compression/HierarchicalZipWriter.h"
#include "../../Core/HttpServer/FilesystemHttpSender.h"
#include "../../Core/Logging.h"
#include "../../Core/Uuid.h"
#include "../ServerContext.h"

#include <stdio.h>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

static const uint64_t MEGA_BYTES = 1024 * 1024;
static const uint64_t GIGA_BYTES = 1024 * 1024 * 1024;

namespace Orthanc
{
  // Download of ZIP files ----------------------------------------------------
 
  static std::string GetDirectoryNameInArchive(const Json::Value& resource,
                                               ResourceType resourceType)
  {
    std::string s;
    const Json::Value& tags = resource["MainDicomTags"];

    switch (resourceType)
    {
      case ResourceType_Patient:
      {
        std::string p = tags["PatientID"].asString();
        std::string n = tags["PatientName"].asString();
        s = p + " " + n;
        break;
      }

      case ResourceType_Study:
      {
        std::string p;
        if (tags.isMember("AccessionNumber"))
        {
          p = tags["AccessionNumber"].asString() + " ";
        }

        s = p + tags["StudyDescription"].asString();
        break;
      }
        
      case ResourceType_Series:
      {
        std::string d = tags["SeriesDescription"].asString();
        std::string m = tags["Modality"].asString();
        s = m + " " + d;
        break;
      }
        
      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    // Get rid of special characters
    return Toolbox::ConvertToAscii(s);
  }

  static bool CreateRootDirectoryInArchive(HierarchicalZipWriter& writer,
                                           ServerContext& context,
                                           const Json::Value& resource,
                                           ResourceType resourceType)
  {
    if (resourceType == ResourceType_Patient)
    {
      return true;
    }

    ResourceType parentType = GetParentResourceType(resourceType);
    Json::Value parent;

    switch (resourceType)
    {
      case ResourceType_Study:
      {
        if (!context.GetIndex().LookupResource(parent, resource["ParentPatient"].asString(), parentType))
        {
          return false;
        }

        break;
      }
        
      case ResourceType_Series:
        if (!context.GetIndex().LookupResource(parent, resource["ParentStudy"].asString(), parentType) ||
            !CreateRootDirectoryInArchive(writer, context, parent, parentType))
        {
          return false;
        }
        break;
        
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    writer.OpenDirectory(GetDirectoryNameInArchive(parent, parentType).c_str());
    return true;
  }

  static bool ArchiveInstance(HierarchicalZipWriter& writer,
                              ServerContext& context,
                              const std::string& instancePublicId,
                              const char* filename)
  {
    writer.OpenFile(filename);

    std::string dicom;
    context.ReadFile(dicom, instancePublicId, FileContentType_Dicom);
    writer.Write(dicom);

    return true;
  }

  static bool ArchiveInternal(HierarchicalZipWriter& writer,
                              ServerContext& context,
                              const std::string& publicId,
                              ResourceType resourceType,
                              bool isFirstLevel)
  { 
    Json::Value resource;
    if (!context.GetIndex().LookupResource(resource, publicId, resourceType))
    {
      return false;
    }    

    if (isFirstLevel && 
        !CreateRootDirectoryInArchive(writer, context, resource, resourceType))
    {
      return false;
    }

    writer.OpenDirectory(GetDirectoryNameInArchive(resource, resourceType).c_str());

    switch (resourceType)
    {
      case ResourceType_Patient:
        for (Json::Value::ArrayIndex i = 0; i < resource["Studies"].size(); i++)
        {
          std::string studyId = resource["Studies"][i].asString();
          if (!ArchiveInternal(writer, context, studyId, ResourceType_Study, false))
          {
            return false;
          }
        }
        break;

      case ResourceType_Study:
        for (Json::Value::ArrayIndex i = 0; i < resource["Series"].size(); i++)
        {
          std::string seriesId = resource["Series"][i].asString();
          if (!ArchiveInternal(writer, context, seriesId, ResourceType_Series, false))
          {
            return false;
          }
        }
        break;

      case ResourceType_Series:
      {
        // Create a filename prefix, depending on the modality
        char format[24] = "%08d.dcm";

        if (resource["MainDicomTags"].isMember("Modality"))
        {
          std::string modality = resource["MainDicomTags"]["Modality"].asString();

          if (modality.size() == 1)
          {
            snprintf(format, sizeof(format) - 1, "%c%%07d.dcm", toupper(modality[0]));
          }
          else if (modality.size() >= 2)
          {
            snprintf(format, sizeof(format) - 1, "%c%c%%06d.dcm", toupper(modality[0]), toupper(modality[1]));
          }
        }

        char filename[24];

        for (Json::Value::ArrayIndex i = 0; i < resource["Instances"].size(); i++)
        {
          snprintf(filename, sizeof(filename) - 1, format, i);

          std::string publicId = resource["Instances"][i].asString();

          // This was the implementation up to Orthanc 0.7.0:
          // std::string filename = instance["MainDicomTags"]["SOPInstanceUID"].asString() + ".dcm";

          if (!ArchiveInstance(writer, context, publicId, filename))
          {
            return false;
          }
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    writer.CloseDirectory();
    return true;
  }                                 


  static bool IsZip64Required(uint64_t uncompressedSize,
                              unsigned int countInstances)
  {
    static const uint64_t  SAFETY_MARGIN = 64 * MEGA_BYTES;

    /**
     * Determine whether ZIP64 is required. Original ZIP format can
     * store up to 2GB of data (some implementation supporting up to
     * 4GB of data), and up to 65535 files.
     * https://en.wikipedia.org/wiki/Zip_(file_format)#ZIP64
     **/

    const bool isZip64 = (uncompressedSize >= 2 * GIGA_BYTES - SAFETY_MARGIN ||
                          countInstances >= 65535);

    LOG(INFO) << "Creating a ZIP file with " << countInstances << " files of size "
              << (uncompressedSize / MEGA_BYTES) << "MB using the "
              << (isZip64 ? "ZIP64" : "ZIP32") << " file format";

    return isZip64;
  }


  static bool IsZip64Required(ServerIndex& index,
                              const std::string& id)
  {
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    unsigned int countStudies;
    unsigned int countSeries;
    unsigned int countInstances;

    index.GetStatistics(compressedSize, uncompressedSize, 
                        countStudies, countSeries, countInstances, id);

    return IsZip64Required(uncompressedSize, countInstances);
  }
                              

  template <enum ResourceType resourceType>
  static void GetArchive(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");
    bool isZip64 = IsZip64Required(context.GetIndex(), id);

    // Create a RAII for the temporary file to manage the ZIP file
    Toolbox::TemporaryFile tmp;

    {
      // Create a ZIP writer
      HierarchicalZipWriter writer(tmp.GetPath().c_str());
      writer.SetZip64(isZip64);

      // Store the requested resource into the ZIP
      if (!ArchiveInternal(writer, context, id, resourceType, true))
      {
        return;
      }
    }

    // Prepare the sending of the ZIP file
    FilesystemHttpSender sender(tmp.GetPath());
    sender.SetContentType("application/zip");
    sender.SetContentFilename(id + ".zip");

    // Send the ZIP
    call.GetOutput().AnswerStream(sender);

    // The temporary file is automatically removed thanks to the RAII
  }


  static void GetMediaArchive(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");
    bool isZip64 = IsZip64Required(context.GetIndex(), id);

    // Create a RAII for the temporary file to manage the ZIP file
    Toolbox::TemporaryFile tmp;

    {
      // Create a ZIP writer
      HierarchicalZipWriter writer(tmp.GetPath().c_str());
      writer.SetZip64(isZip64);
      writer.OpenDirectory("IMAGES");

      // Create the DICOMDIR writer
      DicomDirWriter dicomDir;

      // Retrieve the list of the instances
      std::list<std::string> instances;
      context.GetIndex().GetChildInstances(instances, id);

      size_t pos = 0;
      for (std::list<std::string>::const_iterator
             it = instances.begin(); it != instances.end(); ++it, ++pos)
      {
        // "DICOM restricts the filenames on DICOM media to 8
        // characters (some systems wrongly use 8.3, but this does not
        // conform to the standard)."
        std::string filename = "IM" + boost::lexical_cast<std::string>(pos);
        writer.OpenFile(filename.c_str());

        std::string dicom;
        context.ReadFile(dicom, *it, FileContentType_Dicom);
        writer.Write(dicom);

        ParsedDicomFile parsed(dicom);
        dicomDir.Add("IMAGES", filename, parsed);
      }

      // Add the DICOMDIR
      writer.CloseDirectory();
      writer.OpenFile("DICOMDIR");
      std::string s;
      dicomDir.Encode(s);
      writer.Write(s);
    }

    // Prepare the sending of the ZIP file
    FilesystemHttpSender sender(tmp.GetPath());
    sender.SetContentType("application/zip");
    sender.SetContentFilename(id + ".zip");

    // Send the ZIP
    call.GetOutput().AnswerStream(sender);

    // The temporary file is automatically removed thanks to the RAII
  }




  namespace
  {
    class ResourceIdentifiers
    {
    private:
      ResourceType   level_;
      std::string    patient_;
      std::string    study_;
      std::string    series_;
      std::string    instance_;

      static void GoToParent(ServerIndex& index,
                             std::string& current)
      {
        std::string tmp;

        if (index.LookupParent(tmp, current))
        {
          current = tmp;
        }
        else
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
      }


    public:
      ResourceIdentifiers(ServerIndex& index,
                          const std::string& publicId)
      {
        if (!index.LookupResourceType(level_, publicId))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }

        std::string current = publicId;;
        switch (level_)  // Do not add "break" below!
        {
          case ResourceType_Instance:
            instance_ = current;
            GoToParent(index, current);
            
          case ResourceType_Series:
            series_ = current;
            GoToParent(index, current);

          case ResourceType_Study:
            study_ = current;
            GoToParent(index, current);

          case ResourceType_Patient:
            patient_ = current;
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      const std::string& GetIdentifier(ResourceType level) const
      {
        // Some sanity check to ensure enumerations are not altered
        assert(ResourceType_Patient < ResourceType_Study);
        assert(ResourceType_Study < ResourceType_Series);
        assert(ResourceType_Series < ResourceType_Instance);

        if (level > level_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        switch (level)
        {
          case ResourceType_Patient:
            return patient_;

          case ResourceType_Study:
            return study_;

          case ResourceType_Series:
            return series_;

          case ResourceType_Instance:
            return instance_;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
    };


    class IArchiveVisitor : public boost::noncopyable
    {
    public:
      virtual ~IArchiveVisitor()
      {
      }

      virtual void Open(ResourceType level,
                        const std::string& publicId) = 0;

      virtual void Close() = 0;

      virtual void AddInstance(const std::string& instanceId,
                               const FileInfo& dicom) = 0;
    };


    class ArchiveIndex
    {
    private:
      struct Instance
      {
        std::string  id_;
        FileInfo     dicom_;

        Instance(const std::string& id,
                 const FileInfo& dicom) : 
          id_(id), dicom_(dicom)
        {
        }
      };

      // A "NULL" value for ArchiveIndex indicates a non-expanded node
      typedef std::map<std::string, ArchiveIndex*>   Resources;

      ResourceType         level_;
      Resources            resources_;   // Only at patient/study/series level
      std::list<Instance>  instances_;   // Only at instance level


      void AddResourceToExpand(ServerIndex& index,
                               const std::string& id)
      {
        if (level_ == ResourceType_Instance)
        {
          FileInfo tmp;
          if (index.LookupAttachment(tmp, id, FileContentType_Dicom))
          {
            instances_.push_back(Instance(id, tmp));
          }
        }
        else
        {
          resources_[id] = NULL;
        }
      }


    public:
      ArchiveIndex(ResourceType level) :
        level_(level)
      {
      }

      ~ArchiveIndex()
      {
        for (Resources::iterator it = resources_.begin();
             it != resources_.end(); ++it)
        {
          delete it->second;
        }
      }


      void Add(ServerIndex& index,
               const ResourceIdentifiers& resource)
      {
        const std::string& id = resource.GetIdentifier(level_);
        Resources::iterator previous = resources_.find(id);

        if (level_ == ResourceType_Instance)
        {
          AddResourceToExpand(index, id);
        }
        else if (resource.GetLevel() == level_)
        {
          // Mark this resource for further expansion
          if (previous != resources_.end())
          {
            delete previous->second;
          }

          resources_[id] = NULL;
        }
        else if (previous == resources_.end())
        {
          // This is the first time we meet this resource
          std::auto_ptr<ArchiveIndex> child(new ArchiveIndex(GetChildResourceType(level_)));
          child->Add(index, resource);
          resources_[id] = child.release();
        }
        else if (previous->second != NULL)
        {
          previous->second->Add(index, resource);
        }
        else
        {
          // Nothing to do: This item is marked for further expansion
        }
      }


      void Expand(ServerIndex& index)
      {
        if (level_ == ResourceType_Instance)
        {
          // Expanding an instance node makes no sense
          return;
        }

        for (Resources::iterator it = resources_.begin();
             it != resources_.end(); ++it)
        {
          if (it->second == NULL)
          {
            // This is resource is marked for expansion
            std::list<std::string> children;
            index.GetChildren(children, it->first);

            std::auto_ptr<ArchiveIndex> child(new ArchiveIndex(GetChildResourceType(level_)));

            for (std::list<std::string>::const_iterator 
                   it2 = children.begin(); it2 != children.end(); ++it2)
            {
              child->AddResourceToExpand(index, *it2);
            }

            it->second = child.release();
          }

          assert(it->second != NULL);
          it->second->Expand(index);
        }        
      }


      void Apply(IArchiveVisitor& visitor) const
      {
        if (level_ == ResourceType_Instance)
        {
          for (std::list<Instance>::const_iterator 
                 it = instances_.begin(); it != instances_.end(); ++it)
          {
            visitor.AddInstance(it->id_, it->dicom_);
          }          
        }
        else
        {
          for (Resources::const_iterator it = resources_.begin();
               it != resources_.end(); ++it)
          {
            assert(it->second != NULL);  // There must have been a call to "Expand()"
            visitor.Open(level_, it->first);
            it->second->Apply(visitor);
            visitor.Close();
          }
        }
      }
    };


    class StatisticsVisitor : public IArchiveVisitor
    {
    private:
      uint64_t       size_;
      unsigned int   instances_;
      
    public:
      StatisticsVisitor() : size_(0), instances_(0)
      {
      }

      uint64_t GetUncompressedSize() const
      {
        return size_;
      }

      unsigned int GetInstancesCount() const
      {
        return instances_;
      }

      virtual void Open(ResourceType level,
                        const std::string& publicId)
      {
      }

      virtual void Close()
      {
      }

      virtual void AddInstance(const std::string& instanceId,
                               const FileInfo& dicom)
      {
        instances_ ++;
        size_ += dicom.GetUncompressedSize();
      }
    };


    class PrintVisitor : public IArchiveVisitor
    {
    private:
      std::ostream& out_;
      std::string   indent_;

    public:
      PrintVisitor(std::ostream& out) : out_(out)
      {
      }

      virtual void Open(ResourceType level,
                        const std::string& publicId)
      {
        switch (level)
        {
          case ResourceType_Patient:  indent_ = "";       break;
          case ResourceType_Study:    indent_ = "  ";     break;
          case ResourceType_Series:   indent_ = "    ";   break;
          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        out_ << indent_ << publicId << std::endl;
      }

      virtual void Close()
      {
      }

      virtual void AddInstance(const std::string& instanceId,
                               const FileInfo& dicom)
      {
        out_ << "      " << instanceId << std::endl;
      }
    };
  }


  static void CreateBatchArchive(RestApiPostCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    Json::Value resources;
    if (call.ParseJsonRequest(resources) &&
        resources.type() == Json::arrayValue)
    {
      ArchiveIndex archive(ResourceType_Patient);  // root

      for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
      {
        if (resources[i].type() != Json::stringValue)
        {
          return;   // Bad request
        }

        ResourceIdentifiers resource(index, resources[i].asString());
        archive.Add(index, resource);
      }

      archive.Expand(index);

      PrintVisitor v(std::cout);
      archive.Apply(v);

      StatisticsVisitor s;
      archive.Apply(s);

      std::cout << s.GetUncompressedSize() << " " << s.GetInstancesCount() << std::endl;
    }
  }  



  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive", GetArchive<ResourceType_Patient>);
    Register("/studies/{id}/archive", GetArchive<ResourceType_Study>);
    Register("/series/{id}/archive", GetArchive<ResourceType_Series>);

    Register("/patients/{id}/media", GetMediaArchive);
    Register("/studies/{id}/media", GetMediaArchive);
    Register("/series/{id}/media", GetMediaArchive);

    Register("/tools/archive", CreateBatchArchive);
  }
}
