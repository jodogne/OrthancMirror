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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../Core/DicomParsing/DicomDirWriter.h"
#include "../../Core/FileStorage/StorageAccessor.h"
#include "../../Core/Compression/HierarchicalZipWriter.h"
#include "../../Core/HttpServer/FilesystemHttpSender.h"
#include "../../Core/Logging.h"
#include "../../Core/TemporaryFile.h"
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


    class ArchiveWriterVisitor : public IArchiveVisitor
    {
    private:
      HierarchicalZipWriter&  writer_;
      ServerContext&            context_;
      char                    instanceFormat_[24];
      unsigned int            countInstances_;

      static std::string GetTag(const DicomMap& tags,
                                const DicomTag& tag)
      {
        const DicomValue* v = tags.TestAndGetValue(tag);
        if (v != NULL &&
            !v->IsBinary() &&
            !v->IsNull())
        {
          return v->GetContent();
        }
        else
        {
          return "";
        }
      }

    public:
      ArchiveWriterVisitor(HierarchicalZipWriter& writer,
                           ServerContext& context) :
        writer_(writer),
        context_(context),
        countInstances_(0)
      {
        snprintf(instanceFormat_, sizeof(instanceFormat_) - 1, "%%08d.dcm");
      }

      virtual void Open(ResourceType level,
                        const std::string& publicId)
      {
        std::string path;

        DicomMap tags;
        if (context_.GetIndex().GetMainDicomTags(tags, publicId, level, level))
        {
          switch (level)
          {
            case ResourceType_Patient:
              path = GetTag(tags, DICOM_TAG_PATIENT_ID) + " " + GetTag(tags, DICOM_TAG_PATIENT_NAME);
              break;

            case ResourceType_Study:
              path = GetTag(tags, DICOM_TAG_ACCESSION_NUMBER) + " " + GetTag(tags, DICOM_TAG_STUDY_DESCRIPTION);
              break;

            case ResourceType_Series:
            {
              std::string modality = GetTag(tags, DICOM_TAG_MODALITY);
              path = modality + " " + GetTag(tags, DICOM_TAG_SERIES_DESCRIPTION);

              if (modality.size() == 0)
              {
                snprintf(instanceFormat_, sizeof(instanceFormat_) - 1, "%%08d.dcm");
              }
              else if (modality.size() == 1)
              {
                snprintf(instanceFormat_, sizeof(instanceFormat_) - 1, "%c%%07d.dcm", 
                         toupper(modality[0]));
              }
              else if (modality.size() >= 2)
              {
                snprintf(instanceFormat_, sizeof(instanceFormat_) - 1, "%c%c%%06d.dcm", 
                         toupper(modality[0]), toupper(modality[1]));
              }

              countInstances_ = 0;

              break;
            }

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }

        path = Toolbox::StripSpaces(Toolbox::ConvertToAscii(path));

        if (path.empty())
        {
          path = std::string("Unknown ") + EnumerationToString(level);
        }

        writer_.OpenDirectory(path.c_str());
      }

      virtual void Close()
      {
        writer_.CloseDirectory();
      }

      virtual void AddInstance(const std::string& instanceId,
                               const FileInfo& dicom)
      {
        std::string content;
        context_.ReadAttachment(content, dicom);

        char filename[24];
        snprintf(filename, sizeof(filename) - 1, instanceFormat_, countInstances_);
        countInstances_ ++;

        writer_.OpenFile(filename);
        writer_.Write(content);
      }

      static void Apply(RestApiOutput& output,
                        ServerContext& context,
                        ArchiveIndex& archive,
                        const std::string& filename)
      {
        archive.Expand(context.GetIndex());

        StatisticsVisitor stats;
        archive.Apply(stats);

        const bool isZip64 = IsZip64Required(stats.GetUncompressedSize(), stats.GetInstancesCount());

        // Create a RAII for the temporary file to manage the ZIP file
        TemporaryFile tmp;

        {
          // Create a ZIP writer
          HierarchicalZipWriter writer(tmp.GetPath().c_str());
          writer.SetZip64(isZip64);

          ArchiveWriterVisitor v(writer, context);
          archive.Apply(v);
        }

        // Prepare the sending of the ZIP file
        FilesystemHttpSender sender(tmp.GetPath());
        sender.SetContentType("application/zip");
        sender.SetContentFilename(filename);

        // Send the ZIP
        output.AnswerStream(sender);

        // The temporary file is automatically removed thanks to the RAII
      }
    };

    
    class MediaWriterVisitor : public IArchiveVisitor
    {
    private:
      HierarchicalZipWriter&  writer_;
      DicomDirWriter          dicomDir_;
      ServerContext&          context_;
      unsigned int            countInstances_;

    public:
      MediaWriterVisitor(HierarchicalZipWriter& writer,
                         ServerContext& context) :
        writer_(writer),
        context_(context),
        countInstances_(0)
      {
      }

      void EncodeDicomDir(std::string& result)
      {
        dicomDir_.Encode(result);
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
        // "DICOM restricts the filenames on DICOM media to 8
        // characters (some systems wrongly use 8.3, but this does not
        // conform to the standard)."
        std::string filename = "IM" + boost::lexical_cast<std::string>(countInstances_);
        writer_.OpenFile(filename.c_str());

        std::string content;
        context_.ReadAttachment(content, dicom);
        writer_.Write(content);

        ParsedDicomFile parsed(content);
        dicomDir_.Add("IMAGES", filename, parsed);

        countInstances_ ++;
      }

      static void Apply(RestApiOutput& output,
                        ServerContext& context,
                        ArchiveIndex& archive,
                        const std::string& filename,
                        bool enableExtendedSopClass)
      {
        archive.Expand(context.GetIndex());

        StatisticsVisitor stats;
        archive.Apply(stats);

        const bool isZip64 = IsZip64Required(stats.GetUncompressedSize(), stats.GetInstancesCount());

        // Create a RAII for the temporary file to manage the ZIP file
        TemporaryFile tmp;

        {
          // Create a ZIP writer
          HierarchicalZipWriter writer(tmp.GetPath().c_str());
          writer.SetZip64(isZip64);
          writer.OpenDirectory("IMAGES");

          // Create a DICOMDIR writer
          MediaWriterVisitor v(writer, context);

          // Request type-3 arguments to be added to the DICOMDIR
          v.dicomDir_.EnableExtendedSopClass(enableExtendedSopClass);

          archive.Apply(v);

          // Add the DICOMDIR
          writer.CloseDirectory();
          writer.OpenFile("DICOMDIR");
          std::string s;
          v.EncodeDicomDir(s);
          writer.Write(s);
        }

        // Prepare the sending of the ZIP file
        FilesystemHttpSender sender(tmp.GetPath());
        sender.SetContentType("application/zip");
        sender.SetContentFilename(filename);

        // Send the ZIP
        output.AnswerStream(sender);

        // The temporary file is automatically removed thanks to the RAII
      }
    };
  }


  static bool AddResourcesOfInterest(ArchiveIndex& archive,
                                     RestApiPostCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    Json::Value resources;
    if (call.ParseJsonRequest(resources) &&
        resources.type() == Json::arrayValue)
    {
      for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
      {
        if (resources[i].type() != Json::stringValue)
        {
          return false;   // Bad request
        }

        ResourceIdentifiers resource(index, resources[i].asString());
        archive.Add(index, resource);
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static void CreateBatchArchive(RestApiPostCall& call)
  {
    ArchiveIndex archive(ResourceType_Patient);  // root

    if (AddResourcesOfInterest(archive, call))
    {
      ArchiveWriterVisitor::Apply(call.GetOutput(),
                                  OrthancRestApi::GetContext(call),
                                  archive,
                                  "Archive.zip");
    }
  }  

  
  template <bool Extended>
  static void CreateBatchMedia(RestApiPostCall& call)
  {
    ArchiveIndex archive(ResourceType_Patient);  // root

    if (AddResourcesOfInterest(archive, call))
    {
      MediaWriterVisitor::Apply(call.GetOutput(),
                                OrthancRestApi::GetContext(call),
                                archive,
                                "Archive.zip",
                                Extended);
    }
  }  


  static void CreateArchive(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::string id = call.GetUriComponent("id", "");
    ResourceIdentifiers resource(index, id);

    ArchiveIndex archive(ResourceType_Patient);  // root
    archive.Add(OrthancRestApi::GetIndex(call), resource);

    ArchiveWriterVisitor::Apply(call.GetOutput(),
                                OrthancRestApi::GetContext(call),
                                archive,
                                id + ".zip");
  }


  static void CreateMedia(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::string id = call.GetUriComponent("id", "");
    ResourceIdentifiers resource(index, id);

    ArchiveIndex archive(ResourceType_Patient);  // root
    archive.Add(OrthancRestApi::GetIndex(call), resource);

    MediaWriterVisitor::Apply(call.GetOutput(),
                              OrthancRestApi::GetContext(call),
                              archive,
                              id + ".zip",
                              call.HasArgument("extended"));
  }


  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive", CreateArchive);
    Register("/studies/{id}/archive", CreateArchive);
    Register("/series/{id}/archive", CreateArchive);

    Register("/patients/{id}/media", CreateMedia);
    Register("/studies/{id}/media", CreateMedia);
    Register("/series/{id}/media", CreateMedia);

    Register("/tools/create-archive", CreateBatchArchive);
    Register("/tools/create-media", CreateBatchMedia<false>);
    Register("/tools/create-media-extended", CreateBatchMedia<true>);
  }
}
