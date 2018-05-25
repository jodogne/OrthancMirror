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
    static const uint64_t      SAFETY_MARGIN = 64 * MEGA_BYTES;  // Should be large enough to hold DICOMDIR
    static const unsigned int  FILES_MARGIN = 10;

    /**
     * Determine whether ZIP64 is required. Original ZIP format can
     * store up to 2GB of data (some implementation supporting up to
     * 4GB of data), and up to 65535 files.
     * https://en.wikipedia.org/wiki/Zip_(file_format)#ZIP64
     **/

    const bool isZip64 = (uncompressedSize >= 2 * GIGA_BYTES - SAFETY_MARGIN ||
                          countInstances >= 65535 - FILES_MARGIN);

    LOG(INFO) << "Creating a ZIP file with " << countInstances << " files of size "
              << (uncompressedSize / MEGA_BYTES) << "MB using the "
              << (isZip64 ? "ZIP64" : "ZIP32") << " file format";

    return isZip64;
  }


  namespace
  {
    class ResourceIdentifiers : public boost::noncopyable
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


    class ArchiveIndex : public boost::noncopyable
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



    class ZipCommands : public boost::noncopyable
    {
    private:
      enum Type
      {
        Type_OpenDirectory,
        Type_CloseDirectory,
        Type_WriteInstance
      };

      class Command : public boost::noncopyable
      {
      private:
        Type          type_;
        std::string   filename_;
        std::string   instanceId_;
        FileInfo      info_;

      public:
        explicit Command(Type type) :
          type_(type)
        {
          assert(type_ == Type_CloseDirectory);
        }
        
        Command(Type type,
                const std::string& filename) :
          type_(type),
          filename_(filename)
        {
          assert(type_ == Type_OpenDirectory);
        }
        
        Command(Type type,
                const std::string& filename,
                const std::string& instanceId,
                const FileInfo& info) :
          type_(type),
          filename_(filename),
          instanceId_(instanceId),
          info_(info)
        {
          assert(type_ == Type_WriteInstance);
        }
        
        void Apply(HierarchicalZipWriter& writer,
                   ServerContext& context,
                   DicomDirWriter* dicomDir,
                   const std::string& dicomDirFolder) const
        {
          switch (type_)
          {
            case Type_OpenDirectory:
              writer.OpenDirectory(filename_.c_str());
              break;

            case Type_CloseDirectory:
              writer.CloseDirectory();
              break;

            case Type_WriteInstance:
            {
              std::string content;

              try
              {
                context.ReadAttachment(content, info_);
              }
              catch (OrthancException& e)
              {
                LOG(WARNING) << "An instance was removed after the job was issued: " << instanceId_;
                return;
              }
            
              writer.OpenFile(filename_.c_str());
              writer.Write(content);

              if (dicomDir != NULL)
              {
                ParsedDicomFile parsed(content);
                dicomDir->Add(dicomDirFolder, filename_, parsed);
              }
              
              break;
            }

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
      };
      
      std::deque<Command*>  commands_;
      uint64_t              uncompressedSize_;
      unsigned int          instancesCount_;

      
      void ApplyInternal(HierarchicalZipWriter& writer,
                         ServerContext& context,
                         size_t index,
                         DicomDirWriter* dicomDir,
                         const std::string& dicomDirFolder) const
      {
        if (index >= commands_.size())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

        commands_[index]->Apply(writer, context, dicomDir, dicomDirFolder);
      }
      
    public:
      ZipCommands() :
        uncompressedSize_(0),
        instancesCount_(0)
      {
      }
      
      ~ZipCommands()
      {
        for (std::deque<Command*>::iterator it = commands_.begin();
             it != commands_.end(); ++it)
        {
          assert(*it != NULL);
          delete *it;
        }
      }

      size_t GetSize() const
      {
        return commands_.size();
      }

      unsigned int GetInstancesCount() const
      {
        return instancesCount_;
      }

      uint64_t GetUncompressedSize() const
      {
        return uncompressedSize_;
      }

      void Apply(HierarchicalZipWriter& writer,
                 ServerContext& context,
                 size_t index,
                 DicomDirWriter& dicomDir,
                 const std::string& dicomDirFolder) const
      {
        ApplyInternal(writer, context, index, &dicomDir, dicomDirFolder);
      }

      void Apply(HierarchicalZipWriter& writer,
                 ServerContext& context,
                 size_t index) const
      {
        ApplyInternal(writer, context, index, NULL, "");
      }
      
      void AddOpenDirectory(const std::string& filename)
      {
        commands_.push_back(new Command(Type_OpenDirectory, filename));
      }

      void AddCloseDirectory()
      {
        commands_.push_back(new Command(Type_CloseDirectory));
      }

      void AddWriteInstance(const std::string& filename,
                            const std::string& instanceId,
                            const FileInfo& info)
      {
        commands_.push_back(new Command(Type_WriteInstance, filename, instanceId, info));
        instancesCount_ ++;
        uncompressedSize_ += info.GetUncompressedSize();
      }

      bool IsZip64() const
      {
        return IsZip64Required(GetUncompressedSize(), GetInstancesCount());
      }
    };
    
    

    class ArchiveIndexVisitor : public IArchiveVisitor
    {
    private:
      ZipCommands&    commands_;
      ServerContext&  context_;
      char            instanceFormat_[24];
      unsigned int    counter_;

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
      ArchiveIndexVisitor(ZipCommands& commands,
                          ServerContext& context) :
        commands_(commands),
        context_(context),
        counter_(0)
      {
        if (commands.GetSize() != 0)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        
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

              counter_ = 0;

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

        commands_.AddOpenDirectory(path.c_str());
      }

      virtual void Close()
      {
        commands_.AddCloseDirectory();
      }

      virtual void AddInstance(const std::string& instanceId,
                               const FileInfo& dicom)
      {
        char filename[24];
        snprintf(filename, sizeof(filename) - 1, instanceFormat_, counter_);
        counter_ ++;

        commands_.AddWriteInstance(filename, instanceId, dicom);
      }
    };

    
    class MediaIndexVisitor : public IArchiveVisitor
    {
    private:
      ZipCommands&    commands_;
      ServerContext&  context_;
      unsigned int    counter_;

    public:
      MediaIndexVisitor(ZipCommands& commands,
                        ServerContext& context) :
        commands_(commands),
        context_(context),
        counter_(0)
      {
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
        std::string filename = "IM" + boost::lexical_cast<std::string>(counter_);
        commands_.AddWriteInstance(filename, instanceId, dicom);

        counter_ ++;
      }
    };


    static const char* IMAGES_FOLDER = "IMAGES"; 


    class ZipWriterIterator : public boost::noncopyable
    {
    private:
      TemporaryFile&                        target_;
      ServerContext&                        context_;
      ZipCommands                           commands_;
      std::auto_ptr<HierarchicalZipWriter>  zip_;
      std::auto_ptr<DicomDirWriter>         dicomDir_;
      bool                                  isMedia_;

    public:
      ZipWriterIterator(TemporaryFile& target,
                        ServerContext& context,
                        ArchiveIndex& archive,
                        bool isMedia,
                        bool enableExtendedSopClass) :
        target_(target),
        context_(context),
        isMedia_(isMedia)
      {
        if (isMedia)
        {
          MediaIndexVisitor visitor(commands_, context);
          archive.Expand(context.GetIndex());

          commands_.AddOpenDirectory(IMAGES_FOLDER);        
          archive.Apply(visitor);
          commands_.AddCloseDirectory();

          dicomDir_.reset(new DicomDirWriter);
          dicomDir_->EnableExtendedSopClass(enableExtendedSopClass);
        }
        else
        {
          ArchiveIndexVisitor visitor(commands_, context);
          archive.Expand(context.GetIndex());
          archive.Apply(visitor);
        }

        zip_.reset(new HierarchicalZipWriter(target.GetPath().c_str()));
        zip_->SetZip64(commands_.IsZip64());
      }
      
      size_t GetStepsCount() const
      {
        return commands_.GetSize() + 1;
      }

      void RunStep(size_t index)
      {
        if (index > commands_.GetSize())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else if (index == commands_.GetSize())
        {
          // Last step: Add the DICOMDIR
          if (isMedia_)
          {
            assert(dicomDir_.get() != NULL);
            std::string s;
            dicomDir_->Encode(s);

            zip_->OpenFile("DICOMDIR");
            zip_->Write(s);
          }
        }
        else
        {
          if (isMedia_)
          {
            assert(dicomDir_.get() != NULL);
            commands_.Apply(*zip_, context_, index, *dicomDir_, IMAGES_FOLDER);
          }
          else
          {
            assert(dicomDir_.get() == NULL);
            commands_.Apply(*zip_, context_, index);
          }
        }
      }

      void RunAllSteps()
      {
        for (size_t i = 0; i < GetStepsCount(); i++)
        {
          RunStep(i);
        }
      }
    };


    static void CreateArchive(TemporaryFile& tmp,
                              ServerContext& context,
                              ArchiveIndex& archive)
    {
      ZipWriterIterator writer(tmp, context, archive, false, false);
      writer.RunAllSteps();
    }
    

    static void CreateMedia(TemporaryFile& tmp,
                            ServerContext& context,
                            ArchiveIndex& archive,
                            bool enableExtendedSopClass)
    {
      ZipWriterIterator writer(tmp, context, archive, true, enableExtendedSopClass);
      writer.RunAllSteps();
    }


    static void SendTemporaryFile(RestApiOutput& output,
                                  TemporaryFile& tmp,
                                  const std::string& filename)
    {
      // Prepare the sending of the ZIP file
      FilesystemHttpSender sender(tmp.GetPath());
      sender.SetContentType("application/zip");
      sender.SetContentFilename(filename);

      // Send the ZIP
      output.AnswerStream(sender);
    }



#if 0
    class ArchiveJob : public IJob
    {
    private:
      ServerContext&                        context_;
      ArchiveIndex                          archive_;
      bool                                  isMedia_;
      std::auto_ptr<TemporaryFile>          file_;
      std::auto_ptr<HierarchicalZipWriter>  writer_;
      std::auto_ptr<IArchiveVisitor>        visitor_;
      std::string                           description_;
      bool                                  started_;
      unsigned int                          currentInstance_;

    public:
      ArchiveJob(ServerContext& context,
                 ResourceType level,
                 bool isMedia) :
        context_(context),
        archive_(level),
        isMedia_(isMedia),
        started_(false),
        currentInstance_(0)
      {
      }

      void SetDescription(const std::string& description)
      {
        description_ = description;
      }

      const std::string& GetDescription() const
      {
        return description_;
      }

      void AddResource(const std::string& publicId)
      {
        if (started_)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        
        ResourceIdentifiers resource(context_.GetIndex(), publicId);
        archive_.Add(context_.GetIndex(), resource);
      }

      virtual void SignalResubmit()
      {
        LOG(ERROR) << "Cannot resubmit the creation of an archive";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      virtual void Start()
      {
        if (started_)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        started_ = true;

        archive_.Expand(context_.GetIndex());

        const bool isZip64 = IsZip64Required(stats_.GetUncompressedSize(), stats_.GetInstancesCount());

        file_.reset(new TemporaryFile);
        writer_.reset(new HierarchicalZipWriter(file_->GetPath().c_str()));
        writer_->SetZip64(isZip64);

        if (isMedia_)
        {
          //visitor_.reset(new 
        }
      }
    };
#endif
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
      TemporaryFile tmp;
      CreateArchive(tmp, OrthancRestApi::GetContext(call), archive);
      SendTemporaryFile(call.GetOutput(), tmp, "Archive.zip");
    }
  }  

  
  template <bool Extended>
  static void CreateBatchMedia(RestApiPostCall& call)
  {
    ArchiveIndex archive(ResourceType_Patient);  // root

    if (AddResourcesOfInterest(archive, call))
    {
      TemporaryFile tmp;
      CreateMedia(tmp, OrthancRestApi::GetContext(call), archive, Extended);
      SendTemporaryFile(call.GetOutput(), tmp, "Archive.zip");
    }
  }  


  static void CreateArchive(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::string id = call.GetUriComponent("id", "");
    ResourceIdentifiers resource(index, id);

    ArchiveIndex archive(ResourceType_Patient);  // root
    archive.Add(OrthancRestApi::GetIndex(call), resource);

    TemporaryFile tmp;
    CreateArchive(tmp, OrthancRestApi::GetContext(call), archive);
    SendTemporaryFile(call.GetOutput(), tmp, id + ".zip");
  }


  static void CreateMedia(RestApiGetCall& call)
  {
    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::string id = call.GetUriComponent("id", "");
    ResourceIdentifiers resource(index, id);

    ArchiveIndex archive(ResourceType_Patient);  // root
    archive.Add(OrthancRestApi::GetIndex(call), resource);

    TemporaryFile tmp;
    CreateMedia(tmp, OrthancRestApi::GetContext(call), archive, call.HasArgument("extended"));
    SendTemporaryFile(call.GetOutput(), tmp, id + ".zip");
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
