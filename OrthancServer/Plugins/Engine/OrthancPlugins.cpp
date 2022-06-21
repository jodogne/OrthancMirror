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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "OrthancPlugins.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif

#if !defined(DCMTK_VERSION_NUMBER)
#  error The macro DCMTK_VERSION_NUMBER must be defined
#endif


#include "../../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../../OrthancFramework/Sources/Compression/ZlibCompressor.h"
#include "../../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../../OrthancFramework/Sources/DicomParsing/DicomWebJsonVisitor.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../../OrthancFramework/Sources/DicomParsing/ToDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/HttpServer/HttpServer.h"
#include "../../../OrthancFramework/Sources/HttpServer/HttpToolbox.h"
#include "../../../OrthancFramework/Sources/Images/Image.h"
#include "../../../OrthancFramework/Sources/Images/ImageProcessing.h"
#include "../../../OrthancFramework/Sources/Images/JpegReader.h"
#include "../../../OrthancFramework/Sources/Images/JpegWriter.h"
#include "../../../OrthancFramework/Sources/Images/PngReader.h"
#include "../../../OrthancFramework/Sources/Images/PngWriter.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/Lua/LuaFunctionCall.h"
#include "../../../OrthancFramework/Sources/MallocMemoryBuffer.h"
#include "../../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../../../OrthancFramework/Sources/StringMemoryBuffer.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"
#include "../../Sources/Database/VoidDatabaseListener.h"
#include "../../Sources/OrthancConfiguration.h"
#include "../../Sources/OrthancFindRequestHandler.h"
#include "../../Sources/Search/HierarchicalMatcher.h"
#include "../../Sources/ServerContext.h"
#include "../../Sources/ServerToolbox.h"
#include "OrthancPluginDatabase.h"
#include "OrthancPluginDatabaseV3.h"
#include "PluginsEnumerations.h"
#include "PluginsJob.h"

#include <boost/regex.hpp>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmnet/dimse.h>

#define ERROR_MESSAGE_64BIT "A 64bit version of the Orthanc SDK is necessary to use buffers > 4GB, but is currently not available"


namespace Orthanc
{
  class OrthancPlugins::WebDavCollection : public IWebDavBucket
  {
  private:
    PluginsErrorDictionary&                      errorDictionary_;
    std::string                                  uri_;
    OrthancPluginWebDavIsExistingFolderCallback  isExistingFolder_;
    OrthancPluginWebDavListFolderCallback        listFolder_;
    OrthancPluginWebDavRetrieveFileCallback      retrieveFile_;
    OrthancPluginWebDavStoreFileCallback         storeFile_;
    OrthancPluginWebDavCreateFolderCallback      createFolder_;
    OrthancPluginWebDavDeleteItemCallback        deleteItem_;
    void*                                        payload_;

    class PathHelper : public boost::noncopyable
    {
    private:
      std::vector<const char*>  items_;

    public:
      explicit PathHelper(const std::vector<std::string>& path)
      {
        items_.resize(path.size());
        for (size_t i = 0; i < path.size(); i++)
        {
          items_[i] = path[i].c_str();
        }
      }
      
      uint32_t GetSize() const
      {
        return static_cast<uint32_t>(items_.size());
      }

      const char* const* GetItems() const
      {
        return (items_.empty() ? NULL : &items_[0]);
      }
    };


    static MimeType ParseMimeType(const char* mimeType)
    {
      MimeType mime;
      if (LookupMimeType(mime, mimeType))
      {
        return mime;
      }
      else
      {
        LOG(WARNING) << "Unknown MIME type in plugin: " << mimeType;
        return MimeType_Binary;
      }
    }
    
    static OrthancPluginErrorCode AddFile(
      OrthancPluginWebDavCollection*  collection,
      const char*                     displayName,
      uint64_t                        contentSize,
      const char*                     mimeType,
      const char*                     creationTime)
    {
      try
      {
        std::unique_ptr<File> f(new File(displayName));
        f->SetCreationTime(boost::posix_time::from_iso_string(creationTime));
        f->SetContentLength(contentSize);

        if (mimeType == NULL ||
            std::string(mimeType).empty())
        {
          f->SetMimeType(SystemToolbox::AutodetectMimeType(displayName));
        }
        else
        {
          f->SetMimeType(ParseMimeType(mimeType));
        }
        
        reinterpret_cast<Collection*>(collection)->AddResource(f.release());
        return OrthancPluginErrorCode_Success;
      }
      catch (OrthancException& e)
      {
        return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
      }
      catch (...)
      {
        return OrthancPluginErrorCode_InternalError;
      }
    }
    
    static OrthancPluginErrorCode AddFolder(
      OrthancPluginWebDavCollection*  collection,
      const char*                     displayName,
      const char*                     creationTime)
    {
      try
      {
        std::unique_ptr<Folder> f(new Folder(displayName));
        f->SetCreationTime(boost::posix_time::from_iso_string(creationTime));
        reinterpret_cast<Collection*>(collection)->AddResource(f.release());
        return OrthancPluginErrorCode_Success;
      }
      catch (OrthancException& e)
      {
        return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
      }
      catch (boost::bad_lexical_cast&)
      {
        LOG(ERROR) << "Presumably ill-formed date in the plugin";
        return OrthancPluginErrorCode_ParameterOutOfRange;
      }
      catch (...)
      {
        return OrthancPluginErrorCode_InternalError;
      }
    }


    class ContentTarget : public boost::noncopyable
    {
    private:
      bool                       isSent_;
      MimeType&                  mime_;
      std::string&               content_;
      boost::posix_time::ptime&  modificationTime_;

    public:
      ContentTarget(const std::string& displayName,
                    MimeType& mime,
                    std::string& content,
                    boost::posix_time::ptime& modificationTime) :
        isSent_(false),
        mime_(mime),
        content_(content),
        modificationTime_(modificationTime)
      {
        mime = SystemToolbox::AutodetectMimeType(displayName);
      }

      bool IsSent() const
      {
        return isSent_;
      }
      
      static OrthancPluginErrorCode RetrieveFile(
        OrthancPluginWebDavCollection*  collection,
        const void*                     data,
        uint64_t                        size,
        const char*                     mimeType,
        const char*                     creationTime)
      {
        ContentTarget& target = *reinterpret_cast<ContentTarget*>(collection);
        
        if (target.isSent_)
        {
          return OrthancPluginErrorCode_BadSequenceOfCalls;
        }
        else
        {
          try
          {
            target.isSent_ = true;

            if (mimeType != NULL &&
                !std::string(mimeType).empty())
            {
              target.mime_ = ParseMimeType(mimeType);
            }
            
            target.content_.assign(reinterpret_cast<const char*>(data), size);
            target.modificationTime_ = boost::posix_time::from_iso_string(creationTime);
            return OrthancPluginErrorCode_Success;
          }
          catch (Orthanc::OrthancException& e)
          {
            return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Presumably ill-formed date in the plugin";
            return OrthancPluginErrorCode_ParameterOutOfRange;
          }
          catch (...)
          {
            return OrthancPluginErrorCode_InternalError;
          }
        }
      }
    };


  public:
    WebDavCollection(PluginsErrorDictionary& errorDictionary,
                     const _OrthancPluginRegisterWebDavCollection& p) :
      errorDictionary_(errorDictionary),
      uri_(p.uri),
      isExistingFolder_(p.isExistingFolder),
      listFolder_(p.listFolder),
      retrieveFile_(p.retrieveFile),
      storeFile_(p.storeFile),
      createFolder_(p.createFolder),
      deleteItem_(p.deleteItem),
      payload_(p.payload)
    {
    }

    const std::string& GetUri() const
    {
      return uri_;
    }

    virtual bool IsExistingFolder(const std::vector<std::string>& path)
    {
      PathHelper helper(path);

      uint8_t isExisting;
      OrthancPluginErrorCode code = isExistingFolder_(&isExisting, helper.GetSize(), helper.GetItems(), payload_);

      if (code == OrthancPluginErrorCode_Success)
      {
        return (isExisting != 0);
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

    virtual bool ListCollection(Collection& collection,
                                const std::vector<std::string>& path)
    {
      PathHelper helper(path);

      uint8_t isExisting;
      OrthancPluginErrorCode code = listFolder_(&isExisting, reinterpret_cast<OrthancPluginWebDavCollection*>(&collection), 
                                                AddFile, AddFolder, helper.GetSize(), helper.GetItems(), payload_);

      if (code == OrthancPluginErrorCode_Success)
      {
        return (isExisting != 0);
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& modificationTime, 
                                const std::vector<std::string>& path)
    {
      PathHelper helper(path);
      
      ContentTarget target(path.back(), mime, content, modificationTime);
      OrthancPluginErrorCode code = retrieveFile_(
        reinterpret_cast<OrthancPluginWebDavCollection*>(&target),
        ContentTarget::RetrieveFile, helper.GetSize(), helper.GetItems(), payload_);
      
      if (code == OrthancPluginErrorCode_Success)
      {
        return target.IsSent();
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

    virtual bool StoreFile(const std::string& content,
                           const std::vector<std::string>& path)
    {
      PathHelper helper(path);

      uint8_t isReadOnly;
      OrthancPluginErrorCode code = storeFile_(&isReadOnly, helper.GetSize(), helper.GetItems(),
                                                content.empty() ? NULL : content.c_str(), content.size(), payload_);

      if (code == OrthancPluginErrorCode_Success)
      {
        return (isReadOnly != 0);
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

    virtual bool CreateFolder(const std::vector<std::string>& path)
    {
      PathHelper helper(path);

      uint8_t isReadOnly;
      OrthancPluginErrorCode code = createFolder_(&isReadOnly, helper.GetSize(), helper.GetItems(), payload_);

      if (code == OrthancPluginErrorCode_Success)
      {
        return (isReadOnly != 0);
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }      

    virtual bool DeleteItem(const std::vector<std::string>& path)
    {
      PathHelper helper(path);

      uint8_t isReadOnly;
      OrthancPluginErrorCode code = deleteItem_(&isReadOnly, helper.GetSize(), helper.GetItems(), payload_);

      if (code == OrthancPluginErrorCode_Success)
      {
        return (isReadOnly != 0);
      }
      else
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

    virtual void Start()
    {
    }

    virtual void Stop()
    {
    }
  };
  

  static void CopyToMemoryBuffer(OrthancPluginMemoryBuffer& target,
                                 const void* data,
                                 size_t size)
  {
    if (static_cast<uint32_t>(size) != size)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory, ERROR_MESSAGE_64BIT);
    }

    target.size = size;

    if (size == 0)
    {
      target.data = NULL;
    }
    else
    {
      target.data = malloc(size);
      if (target.data != NULL)
      {
        memcpy(target.data, data, size);
      }
      else
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
    }
  }


  static void CopyToMemoryBuffer(OrthancPluginMemoryBuffer& target,
                                 const std::string& str)
  {
    if (str.size() == 0)
    {
      target.size = 0;
      target.data = NULL;
    }
    else
    {
      CopyToMemoryBuffer(target, str.c_str(), str.size());
    }
  }


  static char* CopyString(const std::string& str)
  {
    if (static_cast<uint32_t>(str.size()) != str.size())
    {
      throw OrthancException(ErrorCode_NotEnoughMemory, ERROR_MESSAGE_64BIT);
    }

    char *result = reinterpret_cast<char*>(malloc(str.size() + 1));
    if (result == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    if (str.size() == 0)
    {
      result[0] = '\0';
    }
    else
    {
      memcpy(result, &str[0], str.size() + 1);
    }

    return result;
  }


  static void CopyDictionary(OrthancPluginMemoryBuffer& target,
                             const std::map<std::string, std::string>& dictionary)
  {
    Json::Value json = Json::objectValue;

    for (HttpClient::HttpHeaders::const_iterator 
           it = dictionary.begin(); it != dictionary.end(); ++it)
    {
      json[it->first] = it->second;
    }
        
    std::string s = json.toStyledString();
    CopyToMemoryBuffer(target, s);
  }


  namespace
  {
    class MemoryBufferRaii : public boost::noncopyable
    {
    private:
      OrthancPluginMemoryBuffer  buffer_;

    public:
      MemoryBufferRaii()
      {
        buffer_.size = 0;
        buffer_.data = NULL;
      }

      ~MemoryBufferRaii()
      {
        if (buffer_.size != 0)
        {
          free(buffer_.data);
        }
      }

      OrthancPluginMemoryBuffer* GetObject()
      {
        return &buffer_;
      }

      void ToString(std::string& target) const
      {
        if ((buffer_.data == NULL && buffer_.size != 0) ||
            (buffer_.data != NULL && buffer_.size == 0))
        {
          throw OrthancException(ErrorCode_Plugin);
        }
        else
        {
          target.resize(buffer_.size);
        
          if (buffer_.size != 0)
          {
            memcpy(&target[0], buffer_.data, buffer_.size);
          }
        }
      }
    };
  

    class StorageAreaBase : public IStorageArea
    {
    private:
      OrthancPluginStorageCreate create_;
      OrthancPluginStorageRemove remove_;
      PluginsErrorDictionary&    errorDictionary_;

    protected:
      PluginsErrorDictionary& GetErrorDictionary() const
      {
        return errorDictionary_;
      }

      IMemoryBuffer* RangeFromWhole(const std::string& uuid,
                                    FileContentType type,
                                    uint64_t start /* inclusive */,
                                    uint64_t end /* exclusive */)
      {
        if (start > end)
        {
          throw OrthancException(ErrorCode_BadRange);
        }
        else if (start == end)
        {
          return new StringMemoryBuffer;  // Empty
        }
        else
        {
          std::unique_ptr<IMemoryBuffer> whole(Read(uuid, type));

          if (start == 0 &&
              end == whole->GetSize())
          {
            return whole.release();
          }
          else if (end > whole->GetSize())
          {
            throw OrthancException(ErrorCode_BadRange);
          }
          else
          {
            std::string range;
            range.resize(end - start);
            assert(!range.empty());
            
            memcpy(&range[0], reinterpret_cast<const char*>(whole->GetData()) + start, range.size());

            whole.reset(NULL);
            return StringMemoryBuffer::CreateFromSwap(range);
          }
        }
      }      
      
    public:
      StorageAreaBase(OrthancPluginStorageCreate create,
                      OrthancPluginStorageRemove remove,
                      PluginsErrorDictionary&  errorDictionary) : 
        create_(create),
        remove_(remove),
        errorDictionary_(errorDictionary)
      {
        if (create_ == NULL ||
            remove_ == NULL)
        {
          throw OrthancException(ErrorCode_Plugin, "Storage area plugin doesn't implement all the required primitives");
        }
      }

      virtual void Create(const std::string& uuid,
                          const void* content, 
                          size_t size,
                          FileContentType type) ORTHANC_OVERRIDE
      {
        OrthancPluginErrorCode error = create_
          (uuid.c_str(), content, size, Plugins::Convert(type));

        if (error != OrthancPluginErrorCode_Success)
        {
          errorDictionary_.LogError(error, true);
          throw OrthancException(static_cast<ErrorCode>(error));
        }
      }

      virtual void Remove(const std::string& uuid,
                          FileContentType type) ORTHANC_OVERRIDE
      {
        OrthancPluginErrorCode error = remove_
          (uuid.c_str(), Plugins::Convert(type));

        if (error != OrthancPluginErrorCode_Success)
        {
          errorDictionary_.LogError(error, true);
          throw OrthancException(static_cast<ErrorCode>(error));
        }
      }
    };


    class PluginStorageArea : public StorageAreaBase
    {
    private:
      OrthancPluginStorageRead   read_;
      OrthancPluginFree          free_;
      
      void Free(void* buffer) const
      {
        if (buffer != NULL)
        {
          free_(buffer);
        }
      }

    public:
      PluginStorageArea(const _OrthancPluginRegisterStorageArea& callbacks,
                        PluginsErrorDictionary&  errorDictionary) :
        StorageAreaBase(callbacks.create, callbacks.remove, errorDictionary),
        read_(callbacks.read),
        free_(callbacks.free)
      {
        if (read_ == NULL)
        {
          throw OrthancException(ErrorCode_Plugin, "Storage area plugin doesn't implement the \"Read\" primitive");
        }
      }

      virtual IMemoryBuffer* Read(const std::string& uuid,
                                  FileContentType type) ORTHANC_OVERRIDE
      {
        std::unique_ptr<MallocMemoryBuffer> result(new MallocMemoryBuffer);

        void* buffer = NULL;
        int64_t size = 0;

        OrthancPluginErrorCode error = read_
          (&buffer, &size, uuid.c_str(), Plugins::Convert(type));

        if (error == OrthancPluginErrorCode_Success)
        {
          result->Assign(buffer, size, free_);
          return result.release();
        }
        else
        {
          GetErrorDictionary().LogError(error, true);
          throw OrthancException(static_cast<ErrorCode>(error));
        }
      }

      virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                       FileContentType type,
                                       uint64_t start /* inclusive */,
                                       uint64_t end /* exclusive */) ORTHANC_OVERRIDE
      {
        return RangeFromWhole(uuid, type, start, end);
      }

      virtual bool HasReadRange() const ORTHANC_OVERRIDE
      {
        return false;
      }
    };


    // New in Orthanc 1.9.0
    class PluginStorageArea2 : public StorageAreaBase
    {
    private:
      OrthancPluginStorageReadWhole  readWhole_;
      OrthancPluginStorageReadRange  readRange_;

    public:
      PluginStorageArea2(const _OrthancPluginRegisterStorageArea2& callbacks,
                         PluginsErrorDictionary&  errorDictionary) :
        StorageAreaBase(callbacks.create, callbacks.remove, errorDictionary),
        readWhole_(callbacks.readWhole),
        readRange_(callbacks.readRange)
      {
        if (readWhole_ == NULL)
        {
          throw OrthancException(ErrorCode_Plugin, "Storage area plugin doesn't implement the \"ReadWhole\" primitive");
        }
      }

      virtual IMemoryBuffer* Read(const std::string& uuid,
                                  FileContentType type) ORTHANC_OVERRIDE
      {
        std::unique_ptr<MallocMemoryBuffer> result(new MallocMemoryBuffer);

        OrthancPluginMemoryBuffer64 buffer;
        buffer.size = 0;
        buffer.data = NULL;
        
        OrthancPluginErrorCode error = readWhole_(&buffer, uuid.c_str(), Plugins::Convert(type));

        if (error == OrthancPluginErrorCode_Success)
        {
          result->Assign(buffer.data, buffer.size, ::free);
          return result.release();
        }
        else
        {
          GetErrorDictionary().LogError(error, true);
          throw OrthancException(static_cast<ErrorCode>(error));
        }
      }

      virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                       FileContentType type,
                                       uint64_t start /* inclusive */,
                                       uint64_t end /* exclusive */) ORTHANC_OVERRIDE
      {
        if (readRange_ == NULL)
        {
          return RangeFromWhole(uuid, type, start, end);
        }
        else
        {
          if (start > end)
          {
            throw OrthancException(ErrorCode_BadRange);
          }
          else if (start == end)
          {
            return new StringMemoryBuffer;
          }
          else
          {
            std::string range;
            range.resize(end - start);
            assert(!range.empty());

            OrthancPluginMemoryBuffer64 buffer;
            buffer.data = &range[0];
            buffer.size = static_cast<uint64_t>(range.size());

            OrthancPluginErrorCode error =
              readRange_(&buffer, uuid.c_str(), Plugins::Convert(type), start);

            if (error == OrthancPluginErrorCode_Success)
            {
              return StringMemoryBuffer::CreateFromSwap(range);
            }
            else
            {
              GetErrorDictionary().LogError(error, true);
              throw OrthancException(static_cast<ErrorCode>(error));
            }
          }
        }
      }
      
      virtual bool HasReadRange() const ORTHANC_OVERRIDE
      {
        return (readRange_ != NULL);
      }
    };


    class StorageAreaFactory : public boost::noncopyable
    {
    private:
      enum Version
      {
        Version1,
        Version2
      };
      
      SharedLibrary&                      sharedLibrary_;
      Version                             version_;
      _OrthancPluginRegisterStorageArea   callbacks_;
      _OrthancPluginRegisterStorageArea2  callbacks2_;
      PluginsErrorDictionary&             errorDictionary_;

      static void WarnNoReadRange()
      {
        LOG(WARNING) << "Performance warning: The storage area plugin doesn't implement reading of file ranges";
      }
      
    public:
      StorageAreaFactory(SharedLibrary& sharedLibrary,
                         const _OrthancPluginRegisterStorageArea& callbacks,
                         PluginsErrorDictionary&  errorDictionary) :
        sharedLibrary_(sharedLibrary),
        version_(Version1),
        callbacks_(callbacks),
        errorDictionary_(errorDictionary)
      {
        WarnNoReadRange();
      }

      StorageAreaFactory(SharedLibrary& sharedLibrary,
                         const _OrthancPluginRegisterStorageArea2& callbacks,
                         PluginsErrorDictionary&  errorDictionary) :
        sharedLibrary_(sharedLibrary),
        version_(Version2),
        callbacks2_(callbacks),
        errorDictionary_(errorDictionary)
      {
        if (callbacks.readRange == NULL)
        {
          WarnNoReadRange();
        }
      }

      SharedLibrary&  GetSharedLibrary()
      {
        return sharedLibrary_;
      }

      IStorageArea* Create() const
      {
        switch (version_)
        {
          case Version1:
            return new PluginStorageArea(callbacks_, errorDictionary_);

          case Version2:
            return new PluginStorageArea2(callbacks2_, errorDictionary_);

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
    };


    class OrthancPeers : public boost::noncopyable
    {
    private:
      std::vector<std::string>           names_;
      std::vector<WebServiceParameters>  parameters_;

      void CheckIndex(size_t i) const
      {
        assert(names_.size() == parameters_.size());
        if (i >= names_.size())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }
      
    public:
      OrthancPeers()
      {
        OrthancConfiguration::ReaderLock lock;

        std::set<std::string> peers;
        lock.GetConfiguration().GetListOfOrthancPeers(peers);

        names_.reserve(peers.size());
        parameters_.reserve(peers.size());
        
        for (std::set<std::string>::const_iterator
               it = peers.begin(); it != peers.end(); ++it)
        {
          WebServiceParameters peer;
          if (lock.GetConfiguration().LookupOrthancPeer(peer, *it))
          {
            names_.push_back(*it);
            parameters_.push_back(peer);
          }
        }
      }

      size_t GetPeersCount() const
      {
        return names_.size();
      }

      const std::string& GetPeerName(size_t i) const
      {
        CheckIndex(i);
        return names_[i];
      }

      const WebServiceParameters& GetPeerParameters(size_t i) const
      {
        CheckIndex(i);
        return parameters_[i];
      }
    };


    class DicomWebBinaryFormatter : public DicomWebJsonVisitor::IBinaryFormatter
    {
    private:
      OrthancPluginDicomWebBinaryCallback   oldCallback_;
      OrthancPluginDicomWebBinaryCallback2  newCallback_;  // New in Orthanc 1.7.0
      void*                                 newPayload_;   // New in Orthanc 1.7.0
      DicomWebJsonVisitor::BinaryMode       currentMode_;
      std::string                           currentBulkDataUri_;

      static void Setter(OrthancPluginDicomWebNode*       node,
                         OrthancPluginDicomWebBinaryMode  mode,
                         const char*                      bulkDataUri)
      {
        DicomWebBinaryFormatter& that = *reinterpret_cast<DicomWebBinaryFormatter*>(node);

        switch (mode)
        {
          case OrthancPluginDicomWebBinaryMode_Ignore:
            that.currentMode_ = DicomWebJsonVisitor::BinaryMode_Ignore;
            break;
              
          case OrthancPluginDicomWebBinaryMode_InlineBinary:
            that.currentMode_ = DicomWebJsonVisitor::BinaryMode_InlineBinary;
            break;
              
          case OrthancPluginDicomWebBinaryMode_BulkDataUri:
            if (bulkDataUri == NULL)
            {
              throw OrthancException(ErrorCode_NullPointer);
            }              
            
            that.currentBulkDataUri_ = bulkDataUri;
            that.currentMode_ = DicomWebJsonVisitor::BinaryMode_BulkDataUri;
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }
      
    public:
      explicit DicomWebBinaryFormatter(OrthancPluginDicomWebBinaryCallback callback) :
        oldCallback_(callback),
        newCallback_(NULL),
        newPayload_(NULL),
        currentMode_(DicomWebJsonVisitor::BinaryMode_Ignore)
      {
      }
      
      DicomWebBinaryFormatter(OrthancPluginDicomWebBinaryCallback2 callback,
                              void* payload) :
        oldCallback_(NULL),
        newCallback_(callback),
        newPayload_(payload),
        currentMode_(DicomWebJsonVisitor::BinaryMode_Ignore)
      {
      }
      
      virtual DicomWebJsonVisitor::BinaryMode Format(std::string& bulkDataUri,
                                                     const std::vector<DicomTag>& parentTags,
                                                     const std::vector<size_t>& parentIndexes,
                                                     const DicomTag& tag,
                                                     ValueRepresentation vr) ORTHANC_OVERRIDE
      {
        if (oldCallback_ == NULL &&
            newCallback_ == NULL)
        {
          return DicomWebJsonVisitor::BinaryMode_InlineBinary;
        }
        else
        {
          assert(parentTags.size() == parentIndexes.size());
          std::vector<uint16_t> groups(parentTags.size());
          std::vector<uint16_t> elements(parentTags.size());
          std::vector<uint32_t> indexes(parentTags.size());

          for (size_t i = 0; i < parentTags.size(); i++)
          {
            groups[i] = parentTags[i].GetGroup();
            elements[i] = parentTags[i].GetElement();
            indexes[i] = static_cast<uint32_t>(parentIndexes[i]);
          }
          bool empty = parentTags.empty();

          currentMode_ = DicomWebJsonVisitor::BinaryMode_Ignore;

          if (oldCallback_ != NULL)
          {
            oldCallback_(reinterpret_cast<OrthancPluginDicomWebNode*>(this),
                         DicomWebBinaryFormatter::Setter,
                         static_cast<uint32_t>(parentTags.size()),
                         (empty ? NULL : &groups[0]),
                         (empty ? NULL : &elements[0]),
                         (empty ? NULL : &indexes[0]),
                         tag.GetGroup(),
                         tag.GetElement(),
                         Plugins::Convert(vr));
          }
          else
          {
            assert(newCallback_ != NULL);
            newCallback_(reinterpret_cast<OrthancPluginDicomWebNode*>(this),
                         DicomWebBinaryFormatter::Setter,
                         static_cast<uint32_t>(parentTags.size()),
                         (empty ? NULL : &groups[0]),
                         (empty ? NULL : &elements[0]),
                         (empty ? NULL : &indexes[0]),
                         tag.GetGroup(),
                         tag.GetElement(),
                         Plugins::Convert(vr),
                         newPayload_);
          }          

          bulkDataUri = currentBulkDataUri_;          
          return currentMode_;
        }
      }

      void Apply(char** target,
                 bool isJson,
                 const ParsedDicomFile& dicom)
      {
        DicomWebJsonVisitor visitor;
        visitor.SetFormatter(*this);

        dicom.Apply(visitor);

        std::string s;

        if (isJson)
        {
          s = visitor.GetResult().toStyledString();
        }
        else
        {
          visitor.FormatXml(s);
        }

        *target = CopyString(s);
      }

  
      void Apply(char** target,
                 bool isJson,
                 const void* dicom,
                 size_t dicomSize) 
      {
        ParsedDicomFile parsed(dicom, dicomSize);
        Apply(target, isJson, parsed);
      }
    };
  }


  class OrthancPlugins::PImpl
  {
  private:
    boost::mutex   contextMutex_;
    ServerContext* context_;
    
  public:
    class PluginHttpOutput : public boost::noncopyable
    {
    private:
      enum MultipartState
      {
        MultipartState_None,
        MultipartState_FirstPart,
        MultipartState_SecondPart,
        MultipartState_NextParts
      };

      HttpOutput&                 output_;
      std::unique_ptr<std::string>  errorDetails_;
      bool                        logDetails_;
      MultipartState              multipartState_;
      std::string                 multipartSubType_;
      std::string                 multipartContentType_;
      std::string                 multipartFirstPart_;
      std::map<std::string, std::string>  multipartFirstHeaders_;
      
    public:
      explicit PluginHttpOutput(HttpOutput& output) :
        output_(output),
        logDetails_(false),
        multipartState_(MultipartState_None)
      {
      }

      HttpOutput& GetOutput()
      {
        if (multipartState_ == MultipartState_None)
        {
          return output_;
        }
        else
        {
          // Must use "SendMultipartItem()" on multipart streams
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
      }

      void SetErrorDetails(const std::string& details,
                           bool logDetails)
      {
        errorDetails_.reset(new std::string(details));
        logDetails_ = logDetails;
      }

      bool HasErrorDetails() const
      {
        return errorDetails_.get() != NULL;
      }

      bool IsLogDetails() const
      {
        return logDetails_;
      }

      const std::string& GetErrorDetails() const
      {
        if (errorDetails_.get() == NULL)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          return *errorDetails_;
        }
      }

      void StartMultipart(const char* subType,
                          const char* contentType)
      {
        if (multipartState_ != MultipartState_None)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          multipartState_ = MultipartState_FirstPart;
          multipartSubType_ = subType;
          multipartContentType_ = contentType;
        }
      }

      void SendMultipartItem(const void* data,
                             size_t size,
                             const std::map<std::string, std::string>& headers)
      {
        if (size != 0 && data == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        switch (multipartState_)
        {
          case MultipartState_None:
            // Must call "StartMultipart()" before
            throw OrthancException(ErrorCode_BadSequenceOfCalls);

          case MultipartState_FirstPart:
            multipartFirstPart_.assign(reinterpret_cast<const char*>(data), size);
            multipartFirstHeaders_ = headers;
            multipartState_ = MultipartState_SecondPart;
            break;

          case MultipartState_SecondPart:
            // Start an actual stream for chunked transfer as soon as
            // there are more than 2 elements in the multipart stream
            output_.StartMultipart(multipartSubType_, multipartContentType_);
            output_.SendMultipartItem(multipartFirstPart_.c_str(), multipartFirstPart_.size(), 
                                      multipartFirstHeaders_);
            multipartFirstPart_.clear();  // Release memory

            output_.SendMultipartItem(data, size, headers);
            multipartState_ = MultipartState_NextParts;
            break;

          case MultipartState_NextParts:
            output_.SendMultipartItem(data, size, headers);
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }

      void Close(OrthancPluginErrorCode error,
                 PluginsErrorDictionary& dictionary)
      {
        if (error == OrthancPluginErrorCode_Success)
        {
          switch (multipartState_)
          {
            case MultipartState_None:
              assert(!output_.IsWritingMultipart());
              break;

            case MultipartState_FirstPart:   // Multipart started, but no part was sent
            case MultipartState_SecondPart:  // Multipart started, first part is pending
            {
              assert(!output_.IsWritingMultipart());
              std::vector<const void*> parts;
              std::vector<size_t> sizes;
              std::vector<const std::map<std::string, std::string>*> headers;

              if (multipartState_ == MultipartState_SecondPart)
              {
                parts.push_back(multipartFirstPart_.c_str());
                sizes.push_back(multipartFirstPart_.size());
                headers.push_back(&multipartFirstHeaders_);
              }

              output_.AnswerMultipartWithoutChunkedTransfer(multipartSubType_, multipartContentType_,
                                                            parts, sizes, headers);
              break;
            }

            case MultipartState_NextParts:
              assert(output_.IsWritingMultipart());
              output_.CloseMultipart();

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          dictionary.LogError(error, false);

          if (HasErrorDetails())
          {
            throw OrthancException(static_cast<ErrorCode>(error),
                                   GetErrorDetails(),
                                   IsLogDetails());
          }
          else
          {
            throw OrthancException(static_cast<ErrorCode>(error));
          }
        }
      }
    };

    
    class RestCallback : public boost::noncopyable
    {
    private:
      boost::regex              regex_;
      OrthancPluginRestCallback callback_;
      bool                      mutualExclusion_;

      OrthancPluginErrorCode InvokeInternal(PluginHttpOutput& output,
                                            const std::string& flatUri,
                                            const OrthancPluginHttpRequest& request)
      {
        return callback_(reinterpret_cast<OrthancPluginRestOutput*>(&output), 
                         flatUri.c_str(), 
                         &request);
      }

    public:
      RestCallback(const char* regex,
                   OrthancPluginRestCallback callback,
                   bool mutualExclusion) :
        regex_(regex),
        callback_(callback),
        mutualExclusion_(mutualExclusion)
      {
      }

      const boost::regex& GetRegularExpression() const
      {
        return regex_;
      }

      OrthancPluginErrorCode Invoke(boost::recursive_mutex& invokationMutex,
                                    PluginHttpOutput& output,
                                    const std::string& flatUri,
                                    const OrthancPluginHttpRequest& request)
      {
        if (mutualExclusion_)
        {
          boost::recursive_mutex::scoped_lock lock(invokationMutex);
          return InvokeInternal(output, flatUri, request);
        }
        else
        {
          return InvokeInternal(output, flatUri, request);
        }
      }
    };


    class ChunkedRestCallback : public boost::noncopyable
    {
    private:
      _OrthancPluginChunkedRestCallback parameters_;
      boost::regex                      regex_;

    public:
      explicit ChunkedRestCallback(const _OrthancPluginChunkedRestCallback& parameters) :
        parameters_(parameters),
        regex_(parameters.pathRegularExpression)
      {
      }

      const boost::regex& GetRegularExpression() const
      {
        return regex_;
      }

      const _OrthancPluginChunkedRestCallback& GetParameters() const
      {
        return parameters_;
      }
    };



    class StorageCommitmentScp : public IStorageCommitmentFactory
    {
    private:
      class Handler : public IStorageCommitmentFactory::ILookupHandler
      {
      private:
        _OrthancPluginRegisterStorageCommitmentScpCallback  parameters_;
        void*    handler_;

      public:
        Handler(const _OrthancPluginRegisterStorageCommitmentScpCallback& parameters,
                void* handler) :
          parameters_(parameters),
          handler_(handler)
        {
          if (handler == NULL)
          {
            throw OrthancException(ErrorCode_NullPointer);
          }
        }

        virtual ~Handler()
        {
          assert(handler_ != NULL);
          parameters_.destructor(handler_);
          handler_ = NULL;
        }

        virtual StorageCommitmentFailureReason Lookup(
          const std::string& sopClassUid,
          const std::string& sopInstanceUid) ORTHANC_OVERRIDE
        {
          assert(handler_ != NULL);
          OrthancPluginStorageCommitmentFailureReason reason =
            OrthancPluginStorageCommitmentFailureReason_Success;
          OrthancPluginErrorCode error = parameters_.lookup(
            &reason, handler_, sopClassUid.c_str(), sopInstanceUid.c_str());
          if (error == OrthancPluginErrorCode_Success)
          {
            return Plugins::Convert(reason);
          }
          else
          {
            throw OrthancException(static_cast<ErrorCode>(error));
          }
        }
      };
      
      _OrthancPluginRegisterStorageCommitmentScpCallback  parameters_;

    public:
      explicit StorageCommitmentScp(const _OrthancPluginRegisterStorageCommitmentScpCallback& parameters) :
        parameters_(parameters)
      {
      }

      virtual ILookupHandler* CreateStorageCommitment(
        const std::string& jobId,
        const std::string& transactionUid,
        const std::vector<std::string>& sopClassUids,
        const std::vector<std::string>& sopInstanceUids,
        const std::string& remoteAet,
        const std::string& calledAet) ORTHANC_OVERRIDE
      {
        const size_t n = sopClassUids.size();
        
        if (sopInstanceUids.size() != n)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        
        std::vector<const char*> a, b;
        a.resize(n);
        b.resize(n);

        for (size_t i = 0; i < n; i++)
        {
          a[i] = sopClassUids[i].c_str();
          b[i] = sopInstanceUids[i].c_str();
        }

        void* handler = NULL;
        OrthancPluginErrorCode error = parameters_.factory(
          &handler, jobId.c_str(), transactionUid.c_str(),
          a.empty() ? NULL : &a[0], b.empty() ? NULL : &b[0], static_cast<uint32_t>(n),
          remoteAet.c_str(), calledAet.c_str());

        if (error != OrthancPluginErrorCode_Success)
        {
          throw OrthancException(static_cast<ErrorCode>(error));          
        }
        else if (handler == NULL)
        {
          // This plugin won't handle this storage commitment request
          return NULL;
        }
        else
        {
          return new Handler(parameters_, handler);
        }
      }
    };


    class ServerContextLock
    {
    private:
      boost::mutex::scoped_lock  lock_;
      ServerContext* context_;

    public:
      explicit ServerContextLock(PImpl& that) : 
        lock_(that.contextMutex_),
        context_(that.context_)
      {
        if (context_ == NULL)
        {
          throw OrthancException(ErrorCode_DatabaseNotInitialized);
        }
      }

      ServerContext& GetContext()
      {
        assert(context_ != NULL);
        return *context_;
      }
    };


    void SetServerContext(ServerContext* context)
    {
      boost::mutex::scoped_lock lock(contextMutex_);
      context_ = context;
    }


    typedef std::pair<std::string, _OrthancPluginProperty>  Property;
    typedef std::list<RestCallback*>  RestCallbacks;
    typedef std::list<ChunkedRestCallback*>  ChunkedRestCallbacks;
    typedef std::list<OrthancPluginOnStoredInstanceCallback>  OnStoredCallbacks;
    typedef std::list<OrthancPluginOnChangeCallback>  OnChangeCallbacks;
    typedef std::list<OrthancPluginIncomingHttpRequestFilter>  IncomingHttpRequestFilters;
    typedef std::list<OrthancPluginIncomingHttpRequestFilter2>  IncomingHttpRequestFilters2;
    typedef std::list<OrthancPluginIncomingDicomInstanceFilter>  IncomingDicomInstanceFilters;
    typedef std::list<OrthancPluginIncomingCStoreInstanceFilter>  IncomingCStoreInstanceFilters;
    typedef std::list<OrthancPluginDecodeImageCallback>  DecodeImageCallbacks;
    typedef std::list<OrthancPluginTranscoderCallback>  TranscoderCallbacks;
    typedef std::list<OrthancPluginJobsUnserializer>  JobsUnserializers;
    typedef std::list<OrthancPluginRefreshMetricsCallback>  RefreshMetricsCallbacks;
    typedef std::list<StorageCommitmentScp*>  StorageCommitmentScpCallbacks;
    typedef std::map<Property, std::string>  Properties;
    typedef std::list<WebDavCollection*>  WebDavCollections;

    PluginsManager manager_;

    RestCallbacks restCallbacks_;
    ChunkedRestCallbacks chunkedRestCallbacks_;
    OnStoredCallbacks  onStoredCallbacks_;
    OnChangeCallbacks  onChangeCallbacks_;
    OrthancPluginFindCallback  findCallback_;
    OrthancPluginWorklistCallback  worklistCallback_;
    DecodeImageCallbacks  decodeImageCallbacks_;
    TranscoderCallbacks  transcoderCallbacks_;
    JobsUnserializers  jobsUnserializers_;
    _OrthancPluginMoveCallback moveCallbacks_;
    IncomingHttpRequestFilters  incomingHttpRequestFilters_;
    IncomingHttpRequestFilters2 incomingHttpRequestFilters2_;
    IncomingDicomInstanceFilters  incomingDicomInstanceFilters_;
    IncomingCStoreInstanceFilters  incomingCStoreInstanceFilters_;  // New in Orthanc 1.10.0
    OrthancPluginReceivedInstanceCallback  receivedInstanceCallback_;  // New in Orthanc 1.10.0
    RefreshMetricsCallbacks refreshMetricsCallbacks_;
    StorageCommitmentScpCallbacks storageCommitmentScpCallbacks_;
    WebDavCollections webDavCollections_;  // New in Orthanc 1.10.1
    std::unique_ptr<StorageAreaFactory>  storageArea_;
    std::set<std::string> authorizationTokens_;

    boost::recursive_mutex restCallbackInvokationMutex_;
    boost::shared_mutex restCallbackRegistrationMutex_;  // New in Orthanc 1.9.0
    boost::recursive_mutex storedCallbackMutex_;
    boost::recursive_mutex changeCallbackMutex_;
    boost::mutex findCallbackMutex_;
    boost::mutex worklistCallbackMutex_;
    boost::shared_mutex decoderTranscoderMutex_;  // Changed from "boost::mutex" in Orthanc 1.7.0
    boost::mutex jobsUnserializersMutex_;
    boost::mutex refreshMetricsMutex_;
    boost::mutex storageCommitmentScpMutex_;
    boost::recursive_mutex invokeServiceMutex_;
    boost::shared_mutex incomingHttpRequestFilterMutex_;  // New in Orthanc 1.8.2

    Properties properties_;
    int argc_;
    char** argv_;
    std::unique_ptr<OrthancPluginDatabase>  database_;
    std::unique_ptr<OrthancPluginDatabaseV3>  databaseV3_;  // New in Orthanc 1.9.2
    PluginsErrorDictionary  dictionary_;
    std::string databaseServerIdentifier_;   // New in Orthanc 1.9.2
    unsigned int maxDatabaseRetries_;   // New in Orthanc 1.9.2

    explicit PImpl(const std::string& databaseServerIdentifier) : 
      context_(NULL), 
      findCallback_(NULL),
      worklistCallback_(NULL),
      receivedInstanceCallback_(NULL),
      argc_(1),
      argv_(NULL),
      databaseServerIdentifier_(databaseServerIdentifier),
      maxDatabaseRetries_(0)
    {
      memset(&moveCallbacks_, 0, sizeof(moveCallbacks_));
    }
  };


  
  class OrthancPlugins::WorklistHandler : public IWorklistRequestHandler
  {
  private:
    OrthancPlugins&  that_;
    std::unique_ptr<HierarchicalMatcher> matcher_;
    std::unique_ptr<ParsedDicomFile>     filtered_;
    ParsedDicomFile* currentQuery_;

    void Reset()
    {
      matcher_.reset();
      filtered_.reset();
      currentQuery_ = NULL;
    }

  public:
    explicit WorklistHandler(OrthancPlugins& that) : that_(that)
    {
      Reset();
    }

    virtual void Handle(DicomFindAnswers& answers,
                        ParsedDicomFile& query,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        ModalityManufacturer manufacturer) ORTHANC_OVERRIDE
    {
      {
        static const char* LUA_CALLBACK = "IncomingWorklistRequestFilter";

        PImpl::ServerContextLock lock(*that_.pimpl_);
        LuaScripting::Lock lua(lock.GetContext().GetLuaScripting());

        if (!lua.GetLua().IsExistingFunction(LUA_CALLBACK))
        {
          currentQuery_ = &query;
        }
        else
        {
          Json::Value source, origin;
          query.DatasetToJson(source, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);

          OrthancFindRequestHandler::FormatOrigin
            (origin, remoteIp, remoteAet, calledAet, manufacturer);

          LuaFunctionCall call(lua.GetLua(), LUA_CALLBACK);
          call.PushJson(source);
          call.PushJson(origin);

          Json::Value target;
          call.ExecuteToJson(target, true);
          
          filtered_.reset(ParsedDicomFile::CreateFromJson(target, DicomFromJsonFlags_None,
                                                          "" /* no private creator */));
          currentQuery_ = filtered_.get();
        }
      }
      
      matcher_.reset(new HierarchicalMatcher(*currentQuery_));

      {
        boost::mutex::scoped_lock lock(that_.pimpl_->worklistCallbackMutex_);

        if (that_.pimpl_->worklistCallback_)
        {
          OrthancPluginErrorCode error = that_.pimpl_->worklistCallback_
            (reinterpret_cast<OrthancPluginWorklistAnswers*>(&answers),
             reinterpret_cast<const OrthancPluginWorklistQuery*>(this),
             remoteAet.c_str(),
             calledAet.c_str());

          if (error != OrthancPluginErrorCode_Success)
          {
            Reset();
            that_.GetErrorDictionary().LogError(error, true);
            throw OrthancException(static_cast<ErrorCode>(error));
          }
        }

        Reset();
      }
    }

    void GetDicomQuery(OrthancPluginMemoryBuffer& target) const
    {
      if (currentQuery_ == NULL)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

      std::string dicom;
      currentQuery_->SaveToMemoryBuffer(dicom);
      CopyToMemoryBuffer(target, dicom.c_str(), dicom.size());
    }

    bool IsMatch(const void* dicom,
                 size_t size) const
    {
      if (matcher_.get() == NULL)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

      ParsedDicomFile f(dicom, size);
      return matcher_->Match(f);
    }

    void AddAnswer(OrthancPluginWorklistAnswers* answers,
                   const void* dicom,
                   size_t size) const
    {
      if (matcher_.get() == NULL)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

      ParsedDicomFile f(dicom, size);
      std::unique_ptr<ParsedDicomFile> summary(matcher_->Extract(f));
      reinterpret_cast<DicomFindAnswers*>(answers)->Add(*summary);
    }
  };

  
  class OrthancPlugins::FindHandler : public IFindRequestHandler
  {
  private:
    OrthancPlugins&            that_;
    std::unique_ptr<DicomArray>  currentQuery_;

    void Reset()
    {
      currentQuery_.reset(NULL);
    }

  public:
    explicit FindHandler(OrthancPlugins& that) : that_(that)
    {
      Reset();
    }

    virtual void Handle(DicomFindAnswers& answers,
                        const DicomMap& input,
                        const std::list<DicomTag>& sequencesToReturn,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        ModalityManufacturer manufacturer) ORTHANC_OVERRIDE
    {
      DicomMap tmp;
      tmp.Assign(input);

      for (std::list<DicomTag>::const_iterator it = sequencesToReturn.begin(); 
           it != sequencesToReturn.end(); ++it)
      {
        if (!input.HasTag(*it))
        {
          tmp.SetValue(*it, "", false);
        }
      }      

      {
        boost::mutex::scoped_lock lock(that_.pimpl_->findCallbackMutex_);
        currentQuery_.reset(new DicomArray(tmp));

        if (that_.pimpl_->findCallback_)
        {
          OrthancPluginErrorCode error = that_.pimpl_->findCallback_
            (reinterpret_cast<OrthancPluginFindAnswers*>(&answers),
             reinterpret_cast<const OrthancPluginFindQuery*>(this),
             remoteAet.c_str(),
             calledAet.c_str());

          if (error != OrthancPluginErrorCode_Success)
          {
            Reset();
            that_.GetErrorDictionary().LogError(error, true);
            throw OrthancException(static_cast<ErrorCode>(error));
          }
        }

        Reset();
      }
    }

    void Invoke(_OrthancPluginService service,
                const _OrthancPluginFindOperation& operation) const
    {
      if (currentQuery_.get() == NULL)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

      switch (service)
      {
        case _OrthancPluginService_GetFindQuerySize:
          *operation.resultUint32 = currentQuery_->GetSize();
          break;

        case _OrthancPluginService_GetFindQueryTag:
        {
          const DicomTag& tag = currentQuery_->GetElement(operation.index).GetTag();
          *operation.resultGroup = tag.GetGroup();
          *operation.resultElement = tag.GetElement();
          break;
        }

        case _OrthancPluginService_GetFindQueryTagName:
        {
          const DicomElement& element = currentQuery_->GetElement(operation.index);
          *operation.resultString = CopyString(FromDcmtkBridge::GetTagName(element));
          break;
        }

        case _OrthancPluginService_GetFindQueryValue:
        {
          *operation.resultString = CopyString(currentQuery_->GetElement(operation.index).GetValue().GetContent());
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  };
  


  class OrthancPlugins::MoveHandler : public IMoveRequestHandler
  {
  private:
    class Driver : public IMoveRequestIterator
    {
    private:
      void*                   driver_;
      unsigned int            count_;
      unsigned int            pos_;
      OrthancPluginApplyMove  apply_;
      OrthancPluginFreeMove   free_;

    public:
      Driver(void* driver,
             unsigned int count,
             OrthancPluginApplyMove apply,
             OrthancPluginFreeMove free) :
        driver_(driver),
        count_(count),
        pos_(0),
        apply_(apply),
        free_(free)
      {
        if (driver_ == NULL)
        {
          throw OrthancException(ErrorCode_Plugin);
        }
      }

      virtual ~Driver()
      {
        if (driver_ != NULL)
        {
          free_(driver_);
          driver_ = NULL;
        }
      }

      virtual unsigned int GetSubOperationCount() const ORTHANC_OVERRIDE
      {
        return count_;
      }

      virtual Status DoNext() ORTHANC_OVERRIDE
      {
        if (pos_ >= count_)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          OrthancPluginErrorCode error = apply_(driver_);
          if (error != OrthancPluginErrorCode_Success)
          {
            LOG(ERROR) << "Error while doing C-Move from plugin: "
                       << EnumerationToString(static_cast<ErrorCode>(error));
            return Status_Failure;
          }
          else
          {
            pos_++;
            return Status_Success;
          }
        }
      }
    };


    _OrthancPluginMoveCallback  params_;


    static std::string ReadTag(const DicomMap& input,
                               const DicomTag& tag)
    {
      const DicomValue* value = input.TestAndGetValue(tag);
      if (value != NULL &&
          !value->IsBinary() &&
          !value->IsNull())
      {
        return value->GetContent();
      }
      else
      {
        return std::string();
      }
    }
                        


  public:
    explicit MoveHandler(OrthancPlugins& that)
    {
      boost::recursive_mutex::scoped_lock lock(that.pimpl_->invokeServiceMutex_);
      params_ = that.pimpl_->moveCallbacks_;
      
      if (params_.callback == NULL ||
          params_.getMoveSize == NULL ||
          params_.applyMove == NULL ||
          params_.freeMove == NULL)
      {
        throw OrthancException(ErrorCode_Plugin);
      }
    }

    virtual IMoveRequestIterator* Handle(const std::string& targetAet,
                                         const DicomMap& input,
                                         const std::string& originatorIp,
                                         const std::string& originatorAet,
                                         const std::string& calledAet,
                                         uint16_t originatorId) ORTHANC_OVERRIDE
    {
      std::string levelString = ReadTag(input, DICOM_TAG_QUERY_RETRIEVE_LEVEL);
      std::string patientId = ReadTag(input, DICOM_TAG_PATIENT_ID);
      std::string accessionNumber = ReadTag(input, DICOM_TAG_ACCESSION_NUMBER);
      std::string studyInstanceUid = ReadTag(input, DICOM_TAG_STUDY_INSTANCE_UID);
      std::string seriesInstanceUid = ReadTag(input, DICOM_TAG_SERIES_INSTANCE_UID);
      std::string sopInstanceUid = ReadTag(input, DICOM_TAG_SOP_INSTANCE_UID);

      OrthancPluginResourceType level = OrthancPluginResourceType_None;

      if (!levelString.empty())
      {
        level = Plugins::Convert(StringToResourceType(levelString.c_str()));
      }

      void* driver = params_.callback(level,
                                      patientId.empty() ? NULL : patientId.c_str(),
                                      accessionNumber.empty() ? NULL : accessionNumber.c_str(),
                                      studyInstanceUid.empty() ? NULL : studyInstanceUid.c_str(),
                                      seriesInstanceUid.empty() ? NULL : seriesInstanceUid.c_str(),
                                      sopInstanceUid.empty() ? NULL : sopInstanceUid.c_str(),
                                      originatorAet.c_str(),
                                      calledAet.c_str(),
                                      targetAet.c_str(),
                                      originatorId);

      if (driver == NULL)
      {
        throw OrthancException(ErrorCode_Plugin,
                               "Plugin cannot create a driver for an incoming C-MOVE request");
      }

      unsigned int size = params_.getMoveSize(driver);

      return new Driver(driver, size, params_.applyMove, params_.freeMove);
    }
  };



  class OrthancPlugins::HttpClientChunkedRequest : public HttpClient::IRequestBody
  {
  private:
    const _OrthancPluginChunkedHttpClient&  params_;
    PluginsErrorDictionary&                 errorDictionary_;

  public:
    HttpClientChunkedRequest(const _OrthancPluginChunkedHttpClient& params,
                             PluginsErrorDictionary&  errorDictionary) :
      params_(params),
      errorDictionary_(errorDictionary)
    {
    }

    virtual bool ReadNextChunk(std::string& chunk) ORTHANC_OVERRIDE
    {
      if (params_.requestIsDone(params_.request))
      {
        return false;
      }
      else
      {
        size_t size = params_.requestChunkSize(params_.request);

        chunk.resize(size);
        
        if (size != 0)
        {
          const void* data = params_.requestChunkData(params_.request);
          memcpy(&chunk[0], data, size);
        }

        OrthancPluginErrorCode error = params_.requestNext(params_.request);
        
        if (error != OrthancPluginErrorCode_Success)
        {
          errorDictionary_.LogError(error, true);
          throw OrthancException(static_cast<ErrorCode>(error));
        }
        else
        {
          return true;
        }
      }
    }
  };


  class OrthancPlugins::HttpClientChunkedAnswer : public HttpClient::IAnswer
  {
  private:
    const _OrthancPluginChunkedHttpClient&  params_;
    PluginsErrorDictionary&                 errorDictionary_;

  public:
    HttpClientChunkedAnswer(const _OrthancPluginChunkedHttpClient& params,
                            PluginsErrorDictionary&  errorDictionary) :
      params_(params),
      errorDictionary_(errorDictionary)
    {
    }

    virtual void AddHeader(const std::string& key,
                           const std::string& value) ORTHANC_OVERRIDE
    {
      OrthancPluginErrorCode error = params_.answerAddHeader(params_.answer, key.c_str(), value.c_str());
        
      if (error != OrthancPluginErrorCode_Success)
      {
        errorDictionary_.LogError(error, true);
        throw OrthancException(static_cast<ErrorCode>(error));
      }
    }
      
    virtual void AddChunk(const void* data,
                          size_t size) ORTHANC_OVERRIDE
    {
      OrthancPluginErrorCode error = params_.answerAddChunk(params_.answer, data, size);
        
      if (error != OrthancPluginErrorCode_Success)
      {
        errorDictionary_.LogError(error, true);
        throw OrthancException(static_cast<ErrorCode>(error));
      }
    }
  };


  OrthancPlugins::OrthancPlugins(const std::string& databaseServerIdentifier)
  {
    /* Sanity check of the compiler */
    if (sizeof(int32_t) != sizeof(OrthancPluginErrorCode) ||
        sizeof(int32_t) != sizeof(OrthancPluginHttpMethod) ||
        sizeof(int32_t) != sizeof(_OrthancPluginService) ||
        sizeof(int32_t) != sizeof(_OrthancPluginProperty) ||
        sizeof(int32_t) != sizeof(OrthancPluginPixelFormat) ||
        sizeof(int32_t) != sizeof(OrthancPluginContentType) ||
        sizeof(int32_t) != sizeof(OrthancPluginResourceType) ||
        sizeof(int32_t) != sizeof(OrthancPluginChangeType) ||
        sizeof(int32_t) != sizeof(OrthancPluginImageFormat) ||
        sizeof(int32_t) != sizeof(OrthancPluginCompressionType) ||
        sizeof(int32_t) != sizeof(OrthancPluginValueRepresentation) ||
        sizeof(int32_t) != sizeof(OrthancPluginDicomToJsonFlags) ||
        sizeof(int32_t) != sizeof(OrthancPluginDicomToJsonFormat) ||
        sizeof(int32_t) != sizeof(OrthancPluginCreateDicomFlags) ||
        sizeof(int32_t) != sizeof(_OrthancPluginDatabaseAnswerType) ||
        sizeof(int32_t) != sizeof(OrthancPluginIdentifierConstraint) ||
        sizeof(int32_t) != sizeof(OrthancPluginInstanceOrigin) ||
        sizeof(int32_t) != sizeof(OrthancPluginJobStepStatus) ||
        sizeof(int32_t) != sizeof(OrthancPluginConstraintType) ||
        sizeof(int32_t) != sizeof(OrthancPluginMetricsType) ||
        sizeof(int32_t) != sizeof(OrthancPluginDicomWebBinaryMode) ||
        sizeof(int32_t) != sizeof(OrthancPluginStorageCommitmentFailureReason) ||
        sizeof(int32_t) != sizeof(OrthancPluginReceivedInstanceAction) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_IncludeBinary) != static_cast<int>(DicomToJsonFlags_IncludeBinary) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_IncludePrivateTags) != static_cast<int>(DicomToJsonFlags_IncludePrivateTags) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_IncludeUnknownTags) != static_cast<int>(DicomToJsonFlags_IncludeUnknownTags) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_IncludePixelData) != static_cast<int>(DicomToJsonFlags_IncludePixelData) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_ConvertBinaryToNull) != static_cast<int>(DicomToJsonFlags_ConvertBinaryToNull) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_ConvertBinaryToAscii) != static_cast<int>(DicomToJsonFlags_ConvertBinaryToAscii) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_StopAfterPixelData) != static_cast<int>(DicomToJsonFlags_StopAfterPixelData) ||
        static_cast<int>(OrthancPluginDicomToJsonFlags_SkipGroupLengths) != static_cast<int>(DicomToJsonFlags_SkipGroupLengths) ||
        static_cast<int>(OrthancPluginCreateDicomFlags_DecodeDataUriScheme) != static_cast<int>(DicomFromJsonFlags_DecodeDataUriScheme) ||
        static_cast<int>(OrthancPluginCreateDicomFlags_GenerateIdentifiers) != static_cast<int>(DicomFromJsonFlags_GenerateIdentifiers))

    {
      throw OrthancException(ErrorCode_Plugin);
    }

    pimpl_.reset(new PImpl(databaseServerIdentifier));
    pimpl_->manager_.RegisterServiceProvider(*this);
  }

  
  void OrthancPlugins::SetServerContext(ServerContext& context)
  {
    pimpl_->SetServerContext(&context);
  }


  void OrthancPlugins::ResetServerContext()
  {
    pimpl_->SetServerContext(NULL);
  }

  
  OrthancPlugins::~OrthancPlugins()
  {
    for (PImpl::RestCallbacks::iterator it = pimpl_->restCallbacks_.begin(); 
         it != pimpl_->restCallbacks_.end(); ++it)
    {
      delete *it;
    }

    for (PImpl::ChunkedRestCallbacks::iterator it = pimpl_->chunkedRestCallbacks_.begin(); 
         it != pimpl_->chunkedRestCallbacks_.end(); ++it)
    {
      delete *it;
    }

    for (PImpl::StorageCommitmentScpCallbacks::iterator
           it = pimpl_->storageCommitmentScpCallbacks_.begin(); 
         it != pimpl_->storageCommitmentScpCallbacks_.end(); ++it)
    {
      delete *it;
    }

    for (PImpl::WebDavCollections::iterator it = pimpl_->webDavCollections_.begin();
         it != pimpl_->webDavCollections_.end(); ++it)
    {
      delete *it;
    }
  }


  static void ArgumentsToPlugin(std::vector<const char*>& keys,
                                std::vector<const char*>& values,
                                const HttpToolbox::Arguments& arguments)
  {
    keys.resize(arguments.size());
    values.resize(arguments.size());

    size_t pos = 0;
    for (HttpToolbox::Arguments::const_iterator 
           it = arguments.begin(); it != arguments.end(); ++it)
    {
      keys[pos] = it->first.c_str();
      values[pos] = it->second.c_str();
      pos++;
    }
  }


  static void ArgumentsToPlugin(std::vector<const char*>& keys,
                                std::vector<const char*>& values,
                                const HttpToolbox::GetArguments& arguments)
  {
    keys.resize(arguments.size());
    values.resize(arguments.size());

    for (size_t i = 0; i < arguments.size(); i++)
    {
      keys[i] = arguments[i].first.c_str();
      values[i] = arguments[i].second.c_str();
    }
  }


  namespace
  {
    class RestCallbackMatcher : public boost::noncopyable
    {
    private:
      std::string               flatUri_;
      std::vector<std::string>  groups_;
      std::vector<const char*>  cgroups_;
      
    public:
      explicit RestCallbackMatcher(const UriComponents& uri) :
        flatUri_(Toolbox::FlattenUri(uri))
      {
      }

      bool IsMatch(const boost::regex& re)
      {
        // Check whether the regular expression associated to this
        // callback matches the URI
        boost::cmatch what;

        if (boost::regex_match(flatUri_.c_str(), what, re))
        {
          // Extract the value of the free parameters of the regular expression
          if (what.size() > 1)
          {
            groups_.resize(what.size() - 1);
            cgroups_.resize(what.size() - 1);
            for (size_t i = 1; i < what.size(); i++)
            {
              groups_[i - 1] = what[i];
              cgroups_[i - 1] = groups_[i - 1].c_str();
            }
          }

          return true;
        }
        else
        {
          // Not a match
          return false;
        }
      }

      uint32_t GetGroupsCount() const
      {
        return cgroups_.size();
      }

      const char* const* GetGroups() const
      {
        return cgroups_.empty() ? NULL : &cgroups_[0];
      }

      const std::string& GetFlatUri() const
      {
        return flatUri_;
      }
    };


    // WARNING - The lifetime of this internal object must be smaller
    // than "matcher", "headers" and "getArguments" objects
    class HttpRequestConverter
    {
    private:
      std::vector<const char*>    getKeys_;
      std::vector<const char*>    getValues_;
      std::vector<const char*>    headersKeys_;
      std::vector<const char*>    headersValues_;
      OrthancPluginHttpRequest    converted_;

    public:
      HttpRequestConverter(const RestCallbackMatcher& matcher,
                           HttpMethod method,
                           const HttpToolbox::Arguments& headers)
      {
        memset(&converted_, 0, sizeof(OrthancPluginHttpRequest));

        ArgumentsToPlugin(headersKeys_, headersValues_, headers);
        assert(headersKeys_.size() == headersValues_.size());

        switch (method)
        {
          case HttpMethod_Get:
            converted_.method = OrthancPluginHttpMethod_Get;
            break;

          case HttpMethod_Post:
            converted_.method = OrthancPluginHttpMethod_Post;
            break;

          case HttpMethod_Delete:
            converted_.method = OrthancPluginHttpMethod_Delete;
            break;

          case HttpMethod_Put:
            converted_.method = OrthancPluginHttpMethod_Put;
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        converted_.groups = matcher.GetGroups();
        converted_.groupsCount = matcher.GetGroupsCount();
        converted_.getCount = 0;
        converted_.getKeys = NULL;
        converted_.getValues = NULL;
        converted_.body = NULL;
        converted_.bodySize = 0;
        converted_.headersCount = headers.size();
       
        if (headers.size() > 0)
        {
          converted_.headersKeys = &headersKeys_[0];
          converted_.headersValues = &headersValues_[0];
        }
      }

      void SetGetArguments(const HttpToolbox::GetArguments& getArguments)
      {
        ArgumentsToPlugin(getKeys_, getValues_, getArguments);
        assert(getKeys_.size() == getValues_.size());

        converted_.getCount = getArguments.size();

        if (getArguments.size() > 0)
        {
          converted_.getKeys = &getKeys_[0];
          converted_.getValues = &getValues_[0];
        }
      }

      OrthancPluginHttpRequest& GetRequest()
      {
        return converted_;
      }
    };
  }


  static std::string GetAllowedMethods(_OrthancPluginChunkedRestCallback parameters)
  {
    std::string s;

    if (parameters.getHandler != NULL)
    {
      s += "GET";
    }

    if (parameters.postHandler != NULL)
    {
      if (!s.empty())
      {
        s+= ",";
      }
      
      s += "POST";
    }

    if (parameters.deleteHandler != NULL)
    {
      if (!s.empty())
      {
        s+= ",";
      }
      
      s += "DELETE";
    }

    if (parameters.putHandler != NULL)
    {
      if (!s.empty())
      {
        s+= ",";
      }
      
      s += "PUT";
    }

    return s;
  }


  bool OrthancPlugins::HandleChunkedGetDelete(HttpOutput& output,
                                              HttpMethod method,
                                              const UriComponents& uri,
                                              const HttpToolbox::Arguments& headers,
                                              const HttpToolbox::GetArguments& getArguments)
  {
    RestCallbackMatcher matcher(uri);

    PImpl::ChunkedRestCallback* callback = NULL;

    // Loop over the callbacks registered by the plugins
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->restCallbackRegistrationMutex_);
    for (PImpl::ChunkedRestCallbacks::const_iterator it = pimpl_->chunkedRestCallbacks_.begin(); 
         it != pimpl_->chunkedRestCallbacks_.end(); ++it)
    {
      if (matcher.IsMatch((*it)->GetRegularExpression()))
      {
        callback = *it;
        break;
      }
    }

    if (callback == NULL)
    {
      return false;
    }
    else
    {
      CLOG(INFO, PLUGINS) << "Delegating HTTP request to plugin for URI: " << matcher.GetFlatUri();

      OrthancPluginRestCallback handler;

      switch (method)
      {
        case HttpMethod_Get:
          handler = callback->GetParameters().getHandler;
          break;

        case HttpMethod_Delete:
          handler = callback->GetParameters().deleteHandler;
          break;

        default:
          handler = NULL;
          break;
      }

      if (handler == NULL)
      {
        output.SendMethodNotAllowed(GetAllowedMethods(callback->GetParameters()));
      }
      else
      {
        HttpRequestConverter converter(matcher, method, headers);
        converter.SetGetArguments(getArguments);
      
        PImpl::PluginHttpOutput pluginOutput(output);
        
        OrthancPluginErrorCode error = handler(
          reinterpret_cast<OrthancPluginRestOutput*>(&pluginOutput), 
          matcher.GetFlatUri().c_str(), &converter.GetRequest());
        
        pluginOutput.Close(error, GetErrorDictionary());
      }
      
      return true;
    }
  }


  bool OrthancPlugins::Handle(HttpOutput& output,
                              RequestOrigin /*origin*/,
                              const char* /*remoteIp*/,
                              const char* /*username*/,
                              HttpMethod method,
                              const UriComponents& uri,
                              const HttpToolbox::Arguments& headers,
                              const HttpToolbox::GetArguments& getArguments,
                              const void* bodyData,
                              size_t bodySize)
  {
    RestCallbackMatcher matcher(uri);

    PImpl::RestCallback* callback = NULL;

    // Loop over the callbacks registered by the plugins
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->restCallbackRegistrationMutex_);
    for (PImpl::RestCallbacks::const_iterator it = pimpl_->restCallbacks_.begin(); 
         it != pimpl_->restCallbacks_.end(); ++it)
    {
      if (matcher.IsMatch((*it)->GetRegularExpression()))
      {
        callback = *it;
        break;
      }
    }

    if (callback == NULL)
    {
      // Callback not found, try to find a chunked callback
      return HandleChunkedGetDelete(output, method, uri, headers, getArguments);
    }

    CLOG(INFO, PLUGINS) << "Delegating HTTP request to plugin for URI: " << matcher.GetFlatUri();

    HttpRequestConverter converter(matcher, method, headers);
    converter.SetGetArguments(getArguments);
    converter.GetRequest().body = bodyData;
    converter.GetRequest().bodySize = bodySize;

    PImpl::PluginHttpOutput pluginOutput(output);

    assert(callback != NULL);
    OrthancPluginErrorCode error = callback->Invoke
      (pimpl_->restCallbackInvokationMutex_, pluginOutput, matcher.GetFlatUri(), converter.GetRequest());

    pluginOutput.Close(error, GetErrorDictionary());
    return true;
  }


  class OrthancPlugins::IDicomInstance : public boost::noncopyable
  {
  public:
    virtual ~IDicomInstance()
    {
    }

    virtual bool CanBeFreed() const = 0;

    virtual const DicomInstanceToStore& GetInstance() const = 0;
  };


  class OrthancPlugins::DicomInstanceFromCallback : public IDicomInstance
  {
  private:
    const DicomInstanceToStore&  instance_;

  public:
    explicit DicomInstanceFromCallback(const DicomInstanceToStore& instance) :
      instance_(instance)
    {
    }

    virtual bool CanBeFreed() const ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual const DicomInstanceToStore& GetInstance() const ORTHANC_OVERRIDE
    {
      return instance_;
    };
  };


  class OrthancPlugins::DicomInstanceFromBuffer : public IDicomInstance
  {
  private:
    std::string                            buffer_;
    std::unique_ptr<DicomInstanceToStore>  instance_;

  public:
    DicomInstanceFromBuffer(const void* buffer,
                            size_t size)
    {
      buffer_.assign(reinterpret_cast<const char*>(buffer), size);

      instance_.reset(DicomInstanceToStore::CreateFromBuffer(buffer_));
      instance_->SetOrigin(DicomInstanceOrigin::FromPlugins());
    }

    virtual bool CanBeFreed() const ORTHANC_OVERRIDE
    {
      return true;
    }

    virtual const DicomInstanceToStore& GetInstance() const ORTHANC_OVERRIDE
    {
      return *instance_;
    };
  };


  class OrthancPlugins::DicomInstanceFromTranscoded : public IDicomInstance
  {
  private:
    std::unique_ptr<ParsedDicomFile>       parsed_;
    std::unique_ptr<DicomInstanceToStore>  instance_;

  public:
    explicit DicomInstanceFromTranscoded(IDicomTranscoder::DicomImage& transcoded) :
      parsed_(transcoded.ReleaseAsParsedDicomFile())
    {
      if (parsed_.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      
      instance_.reset(DicomInstanceToStore::CreateFromParsedDicomFile(*parsed_));
      instance_->SetOrigin(DicomInstanceOrigin::FromPlugins());
    }

    virtual bool CanBeFreed() const ORTHANC_OVERRIDE
    {
      return true;
    }

    virtual const DicomInstanceToStore& GetInstance() const ORTHANC_OVERRIDE
    {
      return *instance_;
    };
  };


  void OrthancPlugins::SignalStoredInstance(const std::string& instanceId,
                                            const DicomInstanceToStore& instance,
                                            const Json::Value& simplifiedTags)
  {
    DicomInstanceFromCallback wrapped(instance);
    
    boost::recursive_mutex::scoped_lock lock(pimpl_->storedCallbackMutex_);

    for (PImpl::OnStoredCallbacks::const_iterator
           callback = pimpl_->onStoredCallbacks_.begin(); 
         callback != pimpl_->onStoredCallbacks_.end(); ++callback)
    {
      OrthancPluginErrorCode error = (*callback) (
        reinterpret_cast<OrthancPluginDicomInstance*>(&wrapped),
        instanceId.c_str());

      if (error != OrthancPluginErrorCode_Success)
      {
        GetErrorDictionary().LogError(error, true);
        throw OrthancException(static_cast<ErrorCode>(error));
      }
    }
  }


  bool OrthancPlugins::FilterIncomingInstance(const DicomInstanceToStore& instance,
                                              const Json::Value& simplified)
  {
    DicomInstanceFromCallback wrapped(instance);
    
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    
    for (PImpl::IncomingDicomInstanceFilters::const_iterator
           filter = pimpl_->incomingDicomInstanceFilters_.begin();
         filter != pimpl_->incomingDicomInstanceFilters_.end(); ++filter)
    {
      int32_t allowed = (*filter) (reinterpret_cast<const OrthancPluginDicomInstance*>(&wrapped));

      if (allowed == 0)
      {
        return false;
      }
      else if (allowed != 1)
      {
        // The callback is only allowed to answer 0 or 1
        throw OrthancException(ErrorCode_Plugin);
      }
    }

    return true;
  }



  bool OrthancPlugins::FilterIncomingCStoreInstance(uint16_t& dimseStatus,
                                                    const DicomInstanceToStore& instance,
                                                    const Json::Value& simplified)
  {
    DicomInstanceFromCallback wrapped(instance);
    
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    
    for (PImpl::IncomingCStoreInstanceFilters::const_iterator
           filter = pimpl_->incomingCStoreInstanceFilters_.begin();
         filter != pimpl_->incomingCStoreInstanceFilters_.end(); ++filter)
    {
      int32_t result = (*filter) (&dimseStatus, reinterpret_cast<const OrthancPluginDicomInstance*>(&wrapped));

      if (result == 0)
      {
        // The instance must be discarded
        return false;
      }
      else if (result == 1)
      {
        // The instance is accepted
        return true;
      }
      else
      {
        // Error
        throw OrthancException(ErrorCode_Plugin);
      }
    }

    return true;  // By default, the instance is accepted
  }


  OrthancPluginReceivedInstanceAction OrthancPlugins::ApplyReceivedInstanceCallbacks(
    MallocMemoryBuffer& modified,
    const void* receivedDicom,
    size_t receivedDicomSize,
    RequestOrigin origin)
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);

    if (pimpl_->receivedInstanceCallback_ == NULL)
    {
      return OrthancPluginReceivedInstanceAction_KeepAsIs;
    }
    else
    {
      OrthancPluginReceivedInstanceAction action;
      
      {
        OrthancPluginMemoryBuffer64 buffer;
        buffer.size = 0;
        buffer.data = NULL;

        action = (*pimpl_->receivedInstanceCallback_) (&buffer, receivedDicom, receivedDicomSize, Plugins::Convert(origin));
        modified.Assign(buffer.data, buffer.size, ::free);
      }

      return action;
    }
  }

  void OrthancPlugins::SignalChangeInternal(OrthancPluginChangeType changeType,
                                            OrthancPluginResourceType resourceType,
                                            const char* resource)
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->changeCallbackMutex_);

    for (std::list<OrthancPluginOnChangeCallback>::const_iterator 
           callback = pimpl_->onChangeCallbacks_.begin(); 
         callback != pimpl_->onChangeCallbacks_.end(); ++callback)
    {
      OrthancPluginErrorCode error = (*callback) (changeType, resourceType, resource);

      if (error != OrthancPluginErrorCode_Success)
      {
        GetErrorDictionary().LogError(error, true);
        throw OrthancException(static_cast<ErrorCode>(error));
      }
    }
  }



  void OrthancPlugins::SignalChange(const ServerIndexChange& change)
  {
    SignalChangeInternal(Plugins::Convert(change.GetChangeType()),
                         Plugins::Convert(change.GetResourceType()),
                         change.GetPublicId().c_str());
  }



  void OrthancPlugins::RegisterRestCallback(const void* parameters,
                                            bool mutualExclusion)
  {
    const _OrthancPluginRestCallback& p = 
      *reinterpret_cast<const _OrthancPluginRestCallback*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered a REST callback "
                        << (mutualExclusion ? "with" : "without")
                        << " mutual exclusion on: " 
                        << p.pathRegularExpression;

    {
      boost::unique_lock<boost::shared_mutex> lock(pimpl_->restCallbackRegistrationMutex_);
      pimpl_->restCallbacks_.push_back(new PImpl::RestCallback(p.pathRegularExpression, p.callback, mutualExclusion));
    }
  }


  void OrthancPlugins::RegisterChunkedRestCallback(const void* parameters)
  {
    const _OrthancPluginChunkedRestCallback& p = 
      *reinterpret_cast<const _OrthancPluginChunkedRestCallback*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered a REST callback for chunked streams on: " 
                        << p.pathRegularExpression;

    {
      boost::unique_lock<boost::shared_mutex> lock(pimpl_->restCallbackRegistrationMutex_);
      pimpl_->chunkedRestCallbacks_.push_back(new PImpl::ChunkedRestCallback(p));
    }
  }


  void OrthancPlugins::RegisterOnStoredInstanceCallback(const void* parameters)
  {
    const _OrthancPluginOnStoredInstanceCallback& p = 
      *reinterpret_cast<const _OrthancPluginOnStoredInstanceCallback*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered an OnStoredInstance callback";
    pimpl_->onStoredCallbacks_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterOnChangeCallback(const void* parameters)
  {
    const _OrthancPluginOnChangeCallback& p = 
      *reinterpret_cast<const _OrthancPluginOnChangeCallback*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered an OnChange callback";
    pimpl_->onChangeCallbacks_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterWorklistCallback(const void* parameters)
  {
    const _OrthancPluginWorklistCallback& p = 
      *reinterpret_cast<const _OrthancPluginWorklistCallback*>(parameters);

    boost::mutex::scoped_lock lock(pimpl_->worklistCallbackMutex_);

    if (pimpl_->worklistCallback_ != NULL)
    {
      throw OrthancException(ErrorCode_Plugin,
                             "Can only register one plugin to handle modality worklists");
    }
    else
    {
      CLOG(INFO, PLUGINS) << "Plugin has registered a callback to handle modality worklists";
      pimpl_->worklistCallback_ = p.callback;
    }
  }


  void OrthancPlugins::RegisterFindCallback(const void* parameters)
  {
    const _OrthancPluginFindCallback& p = 
      *reinterpret_cast<const _OrthancPluginFindCallback*>(parameters);

    boost::mutex::scoped_lock lock(pimpl_->findCallbackMutex_);

    if (pimpl_->findCallback_ != NULL)
    {
      throw OrthancException(ErrorCode_Plugin,
                             "Can only register one plugin to handle C-FIND requests");
    }
    else
    {
      CLOG(INFO, PLUGINS) << "Plugin has registered a callback to handle C-FIND requests";
      pimpl_->findCallback_ = p.callback;
    }
  }


  void OrthancPlugins::RegisterMoveCallback(const void* parameters)
  {
    // invokeServiceMutex_ is assumed to be locked

    const _OrthancPluginMoveCallback& p = 
      *reinterpret_cast<const _OrthancPluginMoveCallback*>(parameters);

    if (pimpl_->moveCallbacks_.callback != NULL)
    {
      throw OrthancException(ErrorCode_Plugin,
                             "Can only register one plugin to handle C-MOVE requests");
    }
    else
    {
      CLOG(INFO, PLUGINS) << "Plugin has registered a callback to handle C-MOVE requests";
      pimpl_->moveCallbacks_ = p;
    }
  }


  void OrthancPlugins::RegisterDecodeImageCallback(const void* parameters)
  {
    const _OrthancPluginDecodeImageCallback& p = 
      *reinterpret_cast<const _OrthancPluginDecodeImageCallback*>(parameters);

    boost::unique_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);

    pimpl_->decodeImageCallbacks_.push_back(p.callback);
    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to decode DICOM images (" 
                        << pimpl_->decodeImageCallbacks_.size() << " decoder(s) now active)";
  }


  void OrthancPlugins::RegisterTranscoderCallback(const void* parameters)
  {
    const _OrthancPluginTranscoderCallback& p = 
      *reinterpret_cast<const _OrthancPluginTranscoderCallback*>(parameters);

    boost::unique_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);

    pimpl_->transcoderCallbacks_.push_back(p.callback);
    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to transcode DICOM images (" 
                        << pimpl_->transcoderCallbacks_.size() << " transcoder(s) now active)";
  }


  void OrthancPlugins::RegisterJobsUnserializer(const void* parameters)
  {
    const _OrthancPluginJobsUnserializer& p = 
      *reinterpret_cast<const _OrthancPluginJobsUnserializer*>(parameters);

    boost::mutex::scoped_lock lock(pimpl_->jobsUnserializersMutex_);

    pimpl_->jobsUnserializers_.push_back(p.unserializer);
    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to unserialize jobs (" 
                        << pimpl_->jobsUnserializers_.size() << " unserializer(s) now active)";
  }


  void OrthancPlugins::RegisterIncomingHttpRequestFilter(const void* parameters)
  {
    const _OrthancPluginIncomingHttpRequestFilter& p = 
      *reinterpret_cast<const _OrthancPluginIncomingHttpRequestFilter*>(parameters);

    boost::unique_lock<boost::shared_mutex> lock(pimpl_->incomingHttpRequestFilterMutex_);
    
    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to filter incoming HTTP requests";
    pimpl_->incomingHttpRequestFilters_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterIncomingHttpRequestFilter2(const void* parameters)
  {
    const _OrthancPluginIncomingHttpRequestFilter2& p = 
      *reinterpret_cast<const _OrthancPluginIncomingHttpRequestFilter2*>(parameters);

    boost::unique_lock<boost::shared_mutex> lock(pimpl_->incomingHttpRequestFilterMutex_);
    
    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to filter incoming HTTP requests";
    pimpl_->incomingHttpRequestFilters2_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterIncomingDicomInstanceFilter(const void* parameters)
  {
    const _OrthancPluginIncomingDicomInstanceFilter& p = 
      *reinterpret_cast<const _OrthancPluginIncomingDicomInstanceFilter*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to filter incoming DICOM instances";
    pimpl_->incomingDicomInstanceFilters_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterIncomingCStoreInstanceFilter(const void* parameters)
  {
    const _OrthancPluginIncomingCStoreInstanceFilter& p = 
      *reinterpret_cast<const _OrthancPluginIncomingCStoreInstanceFilter*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to filter incoming C-Store DICOM instances";
    pimpl_->incomingCStoreInstanceFilters_.push_back(p.callback);
  }

  void OrthancPlugins::RegisterReceivedInstanceCallback(const void* parameters)
  {
    const _OrthancPluginReceivedInstanceCallback& p = 
      *reinterpret_cast<const _OrthancPluginReceivedInstanceCallback*>(parameters);

    if (pimpl_->receivedInstanceCallback_ != NULL)
    {
      throw OrthancException(ErrorCode_Plugin,
                             "Can only register one plugin callback to process received instances");
    }
    else
    {
      CLOG(INFO, PLUGINS) << "Plugin has registered a received instance callback";
      pimpl_->receivedInstanceCallback_ = p.callback;
    }
  }

  void OrthancPlugins::RegisterRefreshMetricsCallback(const void* parameters)
  {
    const _OrthancPluginRegisterRefreshMetricsCallback& p = 
      *reinterpret_cast<const _OrthancPluginRegisterRefreshMetricsCallback*>(parameters);

    boost::mutex::scoped_lock lock(pimpl_->refreshMetricsMutex_);

    CLOG(INFO, PLUGINS) << "Plugin has registered a callback to refresh its metrics";
    pimpl_->refreshMetricsCallbacks_.push_back(p.callback);
  }


  void OrthancPlugins::RegisterStorageCommitmentScpCallback(const void* parameters)
  {
    const _OrthancPluginRegisterStorageCommitmentScpCallback& p = 
      *reinterpret_cast<const _OrthancPluginRegisterStorageCommitmentScpCallback*>(parameters);

    boost::mutex::scoped_lock lock(pimpl_->storageCommitmentScpMutex_);
    CLOG(INFO, PLUGINS) << "Plugin has registered a storage commitment callback";

    pimpl_->storageCommitmentScpCallbacks_.push_back(new PImpl::StorageCommitmentScp(p));
  }


  void OrthancPlugins::AnswerBuffer(const void* parameters)
  {
    const _OrthancPluginAnswerBuffer& p = 
      *reinterpret_cast<const _OrthancPluginAnswerBuffer*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.SetContentType(p.mimeType);
    translatedOutput.Answer(p.answer, p.answerSize);
  }


  void OrthancPlugins::Redirect(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.Redirect(p.argument);
  }


  void OrthancPlugins::SendHttpStatusCode(const void* parameters)
  {
    const _OrthancPluginSendHttpStatusCode& p = 
      *reinterpret_cast<const _OrthancPluginSendHttpStatusCode*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.SendStatus(static_cast<HttpStatus>(p.status));
  }


  void OrthancPlugins::SendHttpStatus(const void* parameters)
  {
    const _OrthancPluginSendHttpStatus& p = 
      *reinterpret_cast<const _OrthancPluginSendHttpStatus*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    HttpStatus status = static_cast<HttpStatus>(p.status);

    if (p.bodySize > 0 && p.body != NULL)
    {
      translatedOutput.SendStatus(status, p.body, p.bodySize);
    }
    else
    {
      translatedOutput.SendStatus(status);
    }
  }


  void OrthancPlugins::SendUnauthorized(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.SendUnauthorized(p.argument);
  }


  void OrthancPlugins::SendMethodNotAllowed(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.SendMethodNotAllowed(p.argument);
  }


  void OrthancPlugins::SetCookie(const void* parameters)
  {
    const _OrthancPluginSetHttpHeader& p = 
      *reinterpret_cast<const _OrthancPluginSetHttpHeader*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.SetCookie(p.key, p.value);
  }


  void OrthancPlugins::SetHttpHeader(const void* parameters)
  {
    const _OrthancPluginSetHttpHeader& p = 
      *reinterpret_cast<const _OrthancPluginSetHttpHeader*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();
    translatedOutput.AddHeader(p.key, p.value);
  }


  void OrthancPlugins::SetHttpErrorDetails(const void* parameters)
  {
    const _OrthancPluginSetHttpErrorDetails& p = 
      *reinterpret_cast<const _OrthancPluginSetHttpErrorDetails*>(parameters);

    PImpl::PluginHttpOutput* output =
      reinterpret_cast<PImpl::PluginHttpOutput*>(p.output);
    output->SetErrorDetails(p.details, (p.log != 0));
  }


  void OrthancPlugins::CompressAndAnswerPngImage(const void* parameters)
  {
    // Bridge for backward compatibility with Orthanc <= 0.9.3
    const _OrthancPluginCompressAndAnswerPngImage& p = 
      *reinterpret_cast<const _OrthancPluginCompressAndAnswerPngImage*>(parameters);

    _OrthancPluginCompressAndAnswerImage p2;
    p2.output = p.output;
    p2.imageFormat = OrthancPluginImageFormat_Png;
    p2.pixelFormat = p.format;
    p2.width = p.width;
    p2.height = p.height;
    p2.pitch = p.height;
    p2.buffer = p.buffer;
    p2.quality = 0;

    CompressAndAnswerImage(&p2);
  }


  void OrthancPlugins::CompressAndAnswerImage(const void* parameters)
  {
    const _OrthancPluginCompressAndAnswerImage& p = 
      *reinterpret_cast<const _OrthancPluginCompressAndAnswerImage*>(parameters);

    HttpOutput& translatedOutput = reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->GetOutput();

    ImageAccessor accessor;
    accessor.AssignReadOnly(Plugins::Convert(p.pixelFormat), p.width, p.height, p.pitch, p.buffer);

    std::string compressed;

    switch (p.imageFormat)
    {
      case OrthancPluginImageFormat_Png:
      {
        PngWriter writer;
        IImageWriter::WriteToMemory(writer, compressed, accessor);
        translatedOutput.SetContentType(MimeType_Png);
        break;
      }

      case OrthancPluginImageFormat_Jpeg:
      {
        JpegWriter writer;
        writer.SetQuality(p.quality);
        IImageWriter::WriteToMemory(writer, compressed, accessor);
        translatedOutput.SetContentType(MimeType_Jpeg);
        break;
      }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    translatedOutput.Answer(compressed);
  }


  void OrthancPlugins::GetDicomForInstance(const void* parameters)
  {
    const _OrthancPluginGetDicomForInstance& p = 
      *reinterpret_cast<const _OrthancPluginGetDicomForInstance*>(parameters);

    std::string dicom;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      lock.GetContext().ReadDicom(dicom, p.instanceId);
    }

    CopyToMemoryBuffer(*p.target, dicom);
  }


  void OrthancPlugins::RestApiGet(const void* parameters,
                                  bool afterPlugins)
  {
    const _OrthancPluginRestApiGet& p = 
      *reinterpret_cast<const _OrthancPluginRestApiGet*>(parameters);
        
    CLOG(INFO, PLUGINS) << "Plugin making REST GET call on URI " << p.uri
                        << (afterPlugins ? " (after plugins)" : " (built-in API)");

    IHttpHandler* handler;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      handler = &lock.GetContext().GetHttpHandler().RestrictToOrthancRestApi(!afterPlugins);
    }

    std::map<std::string, std::string> httpHeaders;

    std::string result;
    if (IHttpHandler::SimpleGet(result, NULL, *handler, RequestOrigin_Plugins, p.uri, httpHeaders) == HttpStatus_200_Ok)
    {
      CopyToMemoryBuffer(*p.target, result);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void OrthancPlugins::RestApiGet2(const void* parameters)
  {
    const _OrthancPluginRestApiGet2& p = 
      *reinterpret_cast<const _OrthancPluginRestApiGet2*>(parameters);
        
    CLOG(INFO, PLUGINS) << "Plugin making REST GET call on URI " << p.uri
                        << (p.afterPlugins ? " (after plugins)" : " (built-in API)");

    HttpToolbox::Arguments headers;

    for (uint32_t i = 0; i < p.headersCount; i++)
    {
      std::string name(p.headersKeys[i]);
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      headers[name] = p.headersValues[i];
    }

    IHttpHandler* handler;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      handler = &lock.GetContext().GetHttpHandler().RestrictToOrthancRestApi(!p.afterPlugins);
    }
      
    std::string result;
    if (IHttpHandler::SimpleGet(result, NULL, *handler, RequestOrigin_Plugins, p.uri, headers) == HttpStatus_200_Ok)
    {
      CopyToMemoryBuffer(*p.target, result);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void OrthancPlugins::RestApiPostPut(bool isPost, 
                                      const void* parameters,
                                      bool afterPlugins)
  {
    const _OrthancPluginRestApiPostPut& p = 
      *reinterpret_cast<const _OrthancPluginRestApiPostPut*>(parameters);

    CLOG(INFO, PLUGINS) << "Plugin making REST " << EnumerationToString(isPost ? HttpMethod_Post : HttpMethod_Put)
                        << " call on URI " << p.uri << (afterPlugins ? " (after plugins)" : " (built-in API)");

    IHttpHandler* handler;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      handler = &lock.GetContext().GetHttpHandler().RestrictToOrthancRestApi(!afterPlugins);
    }
      
    std::map<std::string, std::string> httpHeaders;

    std::string result;
    if (isPost ? 
        IHttpHandler::SimplePost(result, NULL, *handler, RequestOrigin_Plugins, p.uri,
                                 p.body, p.bodySize, httpHeaders) == HttpStatus_200_Ok :
        IHttpHandler::SimplePut(result, NULL, *handler, RequestOrigin_Plugins, p.uri,
                                p.body, p.bodySize, httpHeaders) == HttpStatus_200_Ok)
    {
      CopyToMemoryBuffer(*p.target, result);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void OrthancPlugins::RestApiDelete(const void* parameters,
                                     bool afterPlugins)
  {
    const char* uri = reinterpret_cast<const char*>(parameters);
    CLOG(INFO, PLUGINS) << "Plugin making REST DELETE call on URI " << uri
                        << (afterPlugins ? " (after plugins)" : " (built-in API)");

    IHttpHandler* handler;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      handler = &lock.GetContext().GetHttpHandler().RestrictToOrthancRestApi(!afterPlugins);
    }
      
    std::map<std::string, std::string> httpHeaders;

    if (IHttpHandler::SimpleDelete(NULL, *handler, RequestOrigin_Plugins, uri, httpHeaders) != HttpStatus_200_Ok)
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void OrthancPlugins::LookupResource(_OrthancPluginService service,
                                      const void* parameters)
  {
    const _OrthancPluginRetrieveDynamicString& p = 
      *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters);

    /**
     * The enumeration below only uses the tags that are indexed in
     * the Orthanc database. It reflects the
     * "CandidateResources::ApplyFilter()" method of the
     * "OrthancFindRequestHandler" class.
     **/

    DicomTag tag(0, 0);
    ResourceType level;
    switch (service)
    {
      case _OrthancPluginService_LookupPatient:
        tag = DICOM_TAG_PATIENT_ID;
        level = ResourceType_Patient;
        break;

      case _OrthancPluginService_LookupStudy:
        tag = DICOM_TAG_STUDY_INSTANCE_UID;
        level = ResourceType_Study;
        break;

      case _OrthancPluginService_LookupStudyWithAccessionNumber:
        tag = DICOM_TAG_ACCESSION_NUMBER;
        level = ResourceType_Study;
        break;

      case _OrthancPluginService_LookupSeries:
        tag = DICOM_TAG_SERIES_INSTANCE_UID;
        level = ResourceType_Series;
        break;

      case _OrthancPluginService_LookupInstance:
        tag = DICOM_TAG_SOP_INSTANCE_UID;
        level = ResourceType_Instance;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::vector<std::string> result;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      lock.GetContext().GetIndex().LookupIdentifierExact(result, level, tag, p.argument);
    }

    if (result.size() == 1)
    {
      *p.result = CopyString(result[0]);
    }
    else
    {
      if (result.size() > 1)
      {
        LOG(WARNING) << "LookupResource(): Multiple resources match the query (instead of 0 or 1), which indicates "
                     << "your DICOM database breaks the DICOM model of the real world";
      }
      
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  static void AccessInstanceMetadataInternal(bool checkExistence,
                                             const _OrthancPluginAccessDicomInstance& params,
                                             const DicomInstanceToStore& instance)
  {
    MetadataType metadata;

    try
    {
      metadata = StringToMetadata(params.key);
    }
    catch (OrthancException&)
    {
      // Unknown metadata
      if (checkExistence)
      {
        *params.resultInt64 = -1;
      }
      else
      {
        *params.resultString = NULL;
      }

      return;
    }

    ServerIndex::MetadataMap::const_iterator it = 
      instance.GetMetadata().find(std::make_pair(ResourceType_Instance, metadata));

    if (checkExistence)
    {
      if (it != instance.GetMetadata().end())
      {
        *params.resultInt64 = 1;
      }
      else
      {
        *params.resultInt64 = 0;
      }
    }
    else
    {
      if (it != instance.GetMetadata().end())
      {      
        *params.resultString = it->second.c_str();
      }
      else
      {
        // Error: Missing metadata
        *params.resultString = NULL;
      }
    }
  }


  void OrthancPlugins::AccessDicomInstance(_OrthancPluginService service,
                                           const void* parameters)
  {
    const _OrthancPluginAccessDicomInstance& p = 
      *reinterpret_cast<const _OrthancPluginAccessDicomInstance*>(parameters);

    if (p.instance == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    const DicomInstanceToStore& instance =
      reinterpret_cast<const IDicomInstance*>(p.instance)->GetInstance();

    switch (service)
    {
      case _OrthancPluginService_GetInstanceRemoteAet:
        *p.resultString = instance.GetOrigin().GetRemoteAetC();
        return;

      case _OrthancPluginService_GetInstanceSize:
        *p.resultInt64 = instance.GetBufferSize();
        return;

      case _OrthancPluginService_GetInstanceData:
        *p.resultString = reinterpret_cast<const char*>(instance.GetBufferData());
        return;

      case _OrthancPluginService_HasInstanceMetadata:
        AccessInstanceMetadataInternal(true, p, instance);
        return;

      case _OrthancPluginService_GetInstanceMetadata:
        AccessInstanceMetadataInternal(false, p, instance);
        return;

      case _OrthancPluginService_GetInstanceJson:
      case _OrthancPluginService_GetInstanceSimplifiedJson:
      {
        Json::Value dicomAsJson;
        instance.GetDicomAsJson(dicomAsJson);
        
        std::string s;
        if (service == _OrthancPluginService_GetInstanceJson)
        {
          Toolbox::WriteStyledJson(s, dicomAsJson);
        }
        else
        {
          Json::Value simplified;
          Toolbox::SimplifyDicomAsJson(simplified, dicomAsJson, DicomToJsonFormat_Human);
          Toolbox::WriteStyledJson(s, simplified);
        }

        *p.resultStringToFree = CopyString(s);
        return;
      }

      case _OrthancPluginService_GetInstanceOrigin:   // New in Orthanc 0.9.5
        *p.resultOrigin = Plugins::Convert(instance.GetOrigin().GetRequestOrigin());
        return;

      case _OrthancPluginService_GetInstanceTransferSyntaxUid:   // New in Orthanc 1.6.1
      {
        DicomTransferSyntax s;
        if (instance.LookupTransferSyntax(s))
        {
          *p.resultStringToFree = CopyString(GetTransferSyntaxUid(s));
        }
        else
        {
          *p.resultStringToFree = CopyString("");
        }
        
        return;
      }

      case _OrthancPluginService_HasInstancePixelData:   // New in Orthanc 1.6.1
        *p.resultInt64 = instance.HasPixelData();
        return;

      case _OrthancPluginService_GetInstanceFramesCount:  // New in Orthanc 1.7.0
        *p.resultInt64 = instance.GetFramesCount();
        return;
        
      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  void OrthancPlugins::BufferCompression(const void* parameters)
  {
    const _OrthancPluginBufferCompression& p = 
      *reinterpret_cast<const _OrthancPluginBufferCompression*>(parameters);

    std::string result;

    {
      std::unique_ptr<DeflateBaseCompressor> compressor;

      switch (p.compression)
      {
        case OrthancPluginCompressionType_Zlib:
        {
          compressor.reset(new ZlibCompressor);
          compressor->SetPrefixWithUncompressedSize(false);
          break;
        }

        case OrthancPluginCompressionType_ZlibWithSize:
        {
          compressor.reset(new ZlibCompressor);
          compressor->SetPrefixWithUncompressedSize(true);
          break;
        }

        case OrthancPluginCompressionType_Gzip:
        {
          compressor.reset(new GzipCompressor);
          compressor->SetPrefixWithUncompressedSize(false);
          break;
        }

        case OrthancPluginCompressionType_GzipWithSize:
        {
          compressor.reset(new GzipCompressor);
          compressor->SetPrefixWithUncompressedSize(true);
          break;
        }

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      if (p.uncompress)
      {
        compressor->Uncompress(result, p.source, p.size);
      }
      else
      {
        compressor->Compress(result, p.source, p.size);
      }
    }

    CopyToMemoryBuffer(*p.target, result);
  }


  static OrthancPluginImage* ReturnImage(std::unique_ptr<ImageAccessor>& image)
  {
    // Images returned to plugins are assumed to be writeable. If the
    // input image is read-only, we return a copy so that it can be modified.

    if (image.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    if (image->IsReadOnly())
    {
      std::unique_ptr<Image> copy(new Image(image->GetFormat(), image->GetWidth(), image->GetHeight(), false));
      ImageProcessing::Copy(*copy, *image);
      image.reset(NULL);
      return reinterpret_cast<OrthancPluginImage*>(copy.release());
    }
    else
    {
      return reinterpret_cast<OrthancPluginImage*>(image.release());
    }
  }


  void OrthancPlugins::AccessDicomInstance2(_OrthancPluginService service,
                                            const void* parameters)
  {
    const _OrthancPluginAccessDicomInstance2& p = 
      *reinterpret_cast<const _OrthancPluginAccessDicomInstance2*>(parameters);

    if (p.instance == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    const DicomInstanceToStore& instance =
      reinterpret_cast<const IDicomInstance*>(p.instance)->GetInstance();

    switch (service)
    {
      case _OrthancPluginService_GetInstanceFramesCount:
        *p.targetUint32 = instance.GetFramesCount();
        return;
        
      case _OrthancPluginService_GetInstanceRawFrame:
      {
        if (p.targetBuffer == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        p.targetBuffer->data = NULL;
        p.targetBuffer->size = 0;
        
        MimeType mime;
        std::string frame;
        instance.GetParsedDicomFile().GetRawFrame(frame, mime, p.frameIndex);
        CopyToMemoryBuffer(*p.targetBuffer, frame);
        return;
      }
        
      case _OrthancPluginService_GetInstanceDecodedFrame:
      {
        if (p.targetImage == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        std::unique_ptr<ImageAccessor> decoded;
        {
          PImpl::ServerContextLock lock(*pimpl_);
          decoded.reset(lock.GetContext().DecodeDicomFrame(instance, p.frameIndex));
        }
        
        *(p.targetImage) = ReturnImage(decoded);
        return;
      }
        
      case _OrthancPluginService_SerializeDicomInstance:
      {
        if (p.targetBuffer == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        p.targetBuffer->data = NULL;
        p.targetBuffer->size = 0;
        CopyToMemoryBuffer(*p.targetBuffer, instance.GetBufferData(), instance.GetBufferSize());
        return;
      }

      case _OrthancPluginService_GetInstanceAdvancedJson:
      {
        if (p.targetStringToFree == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        
        Json::Value json;
        instance.DatasetToJson(json, Plugins::Convert(p.format), 
                               static_cast<DicomToJsonFlags>(p.flags), p.maxStringLength);

        std::string s;
        Toolbox::WriteFastJson(s, json);
        *p.targetStringToFree = CopyString(s);        
        return;
      }
      
      case _OrthancPluginService_GetInstanceDicomWebJson:
      case _OrthancPluginService_GetInstanceDicomWebXml:
      {
        if (p.targetStringToFree == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        DicomWebBinaryFormatter formatter(p.dicomWebCallback, p.dicomWebPayload);
        formatter.Apply(p.targetStringToFree,
                        (service == _OrthancPluginService_GetInstanceDicomWebJson),
                        instance.GetParsedDicomFile());
        return;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  void OrthancPlugins::UncompressImage(const void* parameters)
  {
    const _OrthancPluginUncompressImage& p = *reinterpret_cast<const _OrthancPluginUncompressImage*>(parameters);

    std::unique_ptr<ImageAccessor> image;

    switch (p.format)
    {
      case OrthancPluginImageFormat_Png:
      {
        image.reset(new PngReader);
        reinterpret_cast<PngReader&>(*image).ReadFromMemory(p.data, p.size);
        break;
      }

      case OrthancPluginImageFormat_Jpeg:
      {
        image.reset(new JpegReader);
        reinterpret_cast<JpegReader&>(*image).ReadFromMemory(p.data, p.size);
        break;
      }

      case OrthancPluginImageFormat_Dicom:
      {
        PImpl::ServerContextLock lock(*pimpl_);
        image.reset(lock.GetContext().DecodeDicomFrame(p.data, p.size, 0));
        break;
      }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    *(p.target) = ReturnImage(image);
  }


  void OrthancPlugins::CompressImage(const void* parameters)
  {
    const _OrthancPluginCompressImage& p = *reinterpret_cast<const _OrthancPluginCompressImage*>(parameters);

    std::string compressed;

    ImageAccessor accessor;
    accessor.AssignReadOnly(Plugins::Convert(p.pixelFormat), p.width, p.height, p.pitch, p.buffer);

    switch (p.imageFormat)
    {
      case OrthancPluginImageFormat_Png:
      {
        PngWriter writer;
        IImageWriter::WriteToMemory(writer, compressed, accessor);
        break;
      }

      case OrthancPluginImageFormat_Jpeg:
      {
        JpegWriter writer;
        writer.SetQuality(p.quality);
        IImageWriter::WriteToMemory(writer, compressed, accessor);
        break;
      }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    CopyToMemoryBuffer(*p.target, compressed.size() > 0 ? compressed.c_str() : NULL, compressed.size());
  }


  static void SetupHttpClient(HttpClient& client,
                              const _OrthancPluginCallHttpClient2& parameters)
  {
    client.SetUrl(parameters.url);
    client.SetConvertHeadersToLowerCase(false);

    if (parameters.timeout != 0)
    {
      client.SetTimeout(parameters.timeout);
    }

    if (parameters.username != NULL && 
        parameters.password != NULL)
    {
      client.SetCredentials(parameters.username, parameters.password);
    }

    if (parameters.certificateFile != NULL)
    {
      std::string certificate(parameters.certificateFile);
      std::string key, password;

      if (parameters.certificateKeyFile)
      {
        key.assign(parameters.certificateKeyFile);
      }

      if (parameters.certificateKeyPassword)
      {
        password.assign(parameters.certificateKeyPassword);
      }

      client.SetClientCertificate(certificate, key, password);
    }

    client.SetPkcs11Enabled(parameters.pkcs11 ? true : false);

    for (uint32_t i = 0; i < parameters.headersCount; i++)
    {
      if (parameters.headersKeys[i] == NULL ||
          parameters.headersValues[i] == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      client.AddHeader(parameters.headersKeys[i], parameters.headersValues[i]);
    }

    switch (parameters.method)
    {
      case OrthancPluginHttpMethod_Get:
        client.SetMethod(HttpMethod_Get);
        break;

      case OrthancPluginHttpMethod_Post:
        client.SetMethod(HttpMethod_Post);
        client.SetExternalBody(parameters.body, parameters.bodySize);
        break;

      case OrthancPluginHttpMethod_Put:
        client.SetMethod(HttpMethod_Put);
        client.SetExternalBody(parameters.body, parameters.bodySize);
        break;

      case OrthancPluginHttpMethod_Delete:
        client.SetMethod(HttpMethod_Delete);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void ExecuteHttpClientWithoutChunkedBody(uint16_t& httpStatus,
                                                  OrthancPluginMemoryBuffer* answerBody,
                                                  OrthancPluginMemoryBuffer* answerHeaders,
                                                  HttpClient& client)
  {
    std::string body;
    HttpClient::HttpHeaders headers;

    bool success = client.Apply(body, headers);

    // The HTTP request has succeeded
    httpStatus = static_cast<uint16_t>(client.GetLastStatus());

    if (!success)
    {
      HttpClient::ThrowException(client.GetLastStatus());
    }

    // Copy the HTTP headers of the answer, if the plugin requested them
    if (answerHeaders != NULL)
    {
      CopyDictionary(*answerHeaders, headers);
    }

    // Copy the body of the answer if it makes sense
    if (client.GetMethod() != HttpMethod_Delete)
    {
      try
      {
        if (answerBody != NULL)
        {
          CopyToMemoryBuffer(*answerBody, body);
        }
      }
      catch (OrthancException&)
      {
        if (answerHeaders != NULL)
        {
          free(answerHeaders->data);
        }
        throw;
      }
    }
  }


  void OrthancPlugins::CallHttpClient(const void* parameters)
  {
    const _OrthancPluginCallHttpClient& p = *reinterpret_cast<const _OrthancPluginCallHttpClient*>(parameters);

    HttpClient client;

    {    
      _OrthancPluginCallHttpClient2 converted;
      memset(&converted, 0, sizeof(converted));

      converted.answerBody = NULL;
      converted.answerHeaders = NULL;
      converted.httpStatus = NULL;
      converted.method = p.method;
      converted.url = p.url;
      converted.headersCount = 0;
      converted.headersKeys = NULL;
      converted.headersValues = NULL;
      converted.body = p.body;
      converted.bodySize = p.bodySize;
      converted.username = p.username;
      converted.password = p.password;
      converted.timeout = 0;  // Use default timeout
      converted.certificateFile = NULL;
      converted.certificateKeyFile = NULL;
      converted.certificateKeyPassword = NULL;
      converted.pkcs11 = false;

      SetupHttpClient(client, converted);
    }

    uint16_t status;
    ExecuteHttpClientWithoutChunkedBody(status, p.target, NULL, client);
  }


  void OrthancPlugins::CallHttpClient2(const void* parameters)
  {
    const _OrthancPluginCallHttpClient2& p = *reinterpret_cast<const _OrthancPluginCallHttpClient2*>(parameters);
    
    if (p.httpStatus == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    HttpClient client;

    if (p.method == OrthancPluginHttpMethod_Post ||
        p.method == OrthancPluginHttpMethod_Put)
    {
      client.SetExternalBody(p.body, p.bodySize);
    }
    
    SetupHttpClient(client, p);
    ExecuteHttpClientWithoutChunkedBody(*p.httpStatus, p.answerBody, p.answerHeaders, client);
  }


  void OrthancPlugins::ChunkedHttpClient(const void* parameters)
  {
    const _OrthancPluginChunkedHttpClient& p =
      *reinterpret_cast<const _OrthancPluginChunkedHttpClient*>(parameters);
        
    if (p.httpStatus == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    HttpClient client;

    {
      _OrthancPluginCallHttpClient2 converted;
      memset(&converted, 0, sizeof(converted));

      converted.answerBody = NULL;
      converted.answerHeaders = NULL;
      converted.httpStatus = NULL;
      converted.method = p.method;
      converted.url = p.url;
      converted.headersCount = p.headersCount;
      converted.headersKeys = p.headersKeys;
      converted.headersValues = p.headersValues;
      converted.body = NULL;
      converted.bodySize = 0;
      converted.username = p.username;
      converted.password = p.password;
      converted.timeout = p.timeout;
      converted.certificateFile = p.certificateFile;
      converted.certificateKeyFile = p.certificateKeyFile;
      converted.certificateKeyPassword = p.certificateKeyPassword;
      converted.pkcs11 = p.pkcs11;

      SetupHttpClient(client, converted);
    }
    
    HttpClientChunkedRequest body(p, pimpl_->dictionary_);
    client.SetBody(body);

    HttpClientChunkedAnswer answer(p, pimpl_->dictionary_);

    bool success = client.Apply(answer);

    *p.httpStatus = static_cast<uint16_t>(client.GetLastStatus());

    if (!success)
    {
      HttpClient::ThrowException(client.GetLastStatus());
    }
  }


  void OrthancPlugins::CallRestApi(const void* parameters)
  {
    const _OrthancPluginCallRestApi& p = *reinterpret_cast<const _OrthancPluginCallRestApi*>(parameters);
    
    if (p.httpStatus == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    const char* methodString;
    switch (p.method)
    {
      case OrthancPluginHttpMethod_Get:
        methodString = "GET";
        break;

      case OrthancPluginHttpMethod_Post:
        methodString = "POST";
        break;

      case OrthancPluginHttpMethod_Put:
        methodString = "PUT";
        break;

      case OrthancPluginHttpMethod_Delete:
        methodString = "DELETE";
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    CLOG(INFO, PLUGINS) << "Plugin making REST " << methodString << " call to URI " << p.uri
                        << (p.afterPlugins ? " (after plugins)" : " (built-in API)");

    HttpToolbox::Arguments headers;

    for (uint32_t i = 0; i < p.headersCount; i++)
    {
      std::string name(p.headersKeys[i]);
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      headers[name] = p.headersValues[i];
    }

    IHttpHandler* handler;

    {
      PImpl::ServerContextLock lock(*pimpl_);
      handler = &lock.GetContext().GetHttpHandler().RestrictToOrthancRestApi(!p.afterPlugins);
    }
    
    std::string answerBody;
    std::map<std::string, std::string> answerHeaders;
    HttpStatus status;

    switch (p.method)
    {
      case OrthancPluginHttpMethod_Get:
        status = IHttpHandler::SimpleGet(
          answerBody, &answerHeaders, *handler, RequestOrigin_Plugins, p.uri, headers);
        break;

      case OrthancPluginHttpMethod_Post:
        status = IHttpHandler::SimplePost(
          answerBody, &answerHeaders, *handler, RequestOrigin_Plugins, p.uri, p.body, p.bodySize, headers);
        break;

      case OrthancPluginHttpMethod_Put:
        status = IHttpHandler::SimplePut(
          answerBody, &answerHeaders, *handler, RequestOrigin_Plugins, p.uri, p.body, p.bodySize, headers);
        break;

      case OrthancPluginHttpMethod_Delete:
        status = IHttpHandler::SimpleDelete(
          &answerHeaders, *handler, RequestOrigin_Plugins, p.uri, headers);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    *p.httpStatus = static_cast<uint16_t>(status);

    if (p.answerHeaders != NULL)
    {
      CopyDictionary(*p.answerHeaders, answerHeaders);
    }

    try
    {
      if (p.answerBody != NULL)
      {
        CopyToMemoryBuffer(*p.answerBody, answerBody);
      }
    }
    catch (OrthancException&)
    {
      if (p.answerHeaders != NULL)
      {
        free(p.answerHeaders->data);
      }
      throw;
    }
  }


  void OrthancPlugins::CallPeerApi(const void* parameters)
  {
    const _OrthancPluginCallPeerApi& p = *reinterpret_cast<const _OrthancPluginCallPeerApi*>(parameters);
    const OrthancPeers& peers = *reinterpret_cast<const OrthancPeers*>(p.peers);

    HttpClient client(peers.GetPeerParameters(p.peerIndex), p.uri);
    client.SetConvertHeadersToLowerCase(false);

    if (p.timeout != 0)
    {
      client.SetTimeout(p.timeout);
    }

    for (uint32_t i = 0; i < p.additionalHeadersCount; i++)
    {
      if (p.additionalHeadersKeys[i] == NULL ||
          p.additionalHeadersValues[i] == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      client.AddHeader(p.additionalHeadersKeys[i], p.additionalHeadersValues[i]);
    }

    switch (p.method)
    {
      case OrthancPluginHttpMethod_Get:
        client.SetMethod(HttpMethod_Get);
        break;

      case OrthancPluginHttpMethod_Post:
        client.SetMethod(HttpMethod_Post);
        client.SetExternalBody(p.body, p.bodySize);
        break;

      case OrthancPluginHttpMethod_Put:
        client.SetMethod(HttpMethod_Put);
        client.SetExternalBody(p.body, p.bodySize);
        break;

      case OrthancPluginHttpMethod_Delete:
        client.SetMethod(HttpMethod_Delete);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    std::string body;
    HttpClient::HttpHeaders headers;

    bool success = client.Apply(body, headers);

    // The HTTP request has succeeded
    *p.httpStatus = static_cast<uint16_t>(client.GetLastStatus());

    if (!success)
    {
      HttpClient::ThrowException(client.GetLastStatus());
    }

    // Copy the HTTP headers of the answer, if the plugin requested them
    if (p.answerHeaders != NULL)
    {
      CopyDictionary(*p.answerHeaders, headers);
    }

    // Copy the body of the answer if it makes sense
    if (p.method != OrthancPluginHttpMethod_Delete)
    {
      try
      {
        if (p.answerBody != NULL)
        {
          CopyToMemoryBuffer(*p.answerBody, body);
        }
      }
      catch (OrthancException&)
      {
        if (p.answerHeaders != NULL)
        {
          free(p.answerHeaders->data);
        }
        throw;
      }
    }
  }


  void OrthancPlugins::ConvertPixelFormat(const void* parameters)
  {
    const _OrthancPluginConvertPixelFormat& p = *reinterpret_cast<const _OrthancPluginConvertPixelFormat*>(parameters);
    const ImageAccessor& source = *reinterpret_cast<const ImageAccessor*>(p.source);

    std::unique_ptr<ImageAccessor> target(new Image(Plugins::Convert(p.targetFormat), source.GetWidth(), source.GetHeight(), false));
    ImageProcessing::Convert(*target, source);

    *(p.target) = ReturnImage(target);
  }



  void OrthancPlugins::GetFontInfo(const void* parameters)
  {
    const _OrthancPluginGetFontInfo& p = *reinterpret_cast<const _OrthancPluginGetFontInfo*>(parameters);

    {
      OrthancConfiguration::ReaderLock lock;

      const Font& font = lock.GetConfiguration().GetFontRegistry().GetFont(p.fontIndex);

      if (p.name != NULL)
      {
        *(p.name) = font.GetName().c_str();
      }
      else if (p.size != NULL)
      {
        *(p.size) = font.GetSize();
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  void OrthancPlugins::DrawText(const void* parameters)
  {
    const _OrthancPluginDrawText& p = *reinterpret_cast<const _OrthancPluginDrawText*>(parameters);

    {
      OrthancConfiguration::ReaderLock lock;
      const Font& font = lock.GetConfiguration().GetFontRegistry().GetFont(p.fontIndex);

      ImageAccessor& target = *reinterpret_cast<ImageAccessor*>(p.image);
      font.Draw(target, p.utf8Text, p.x, p.y, p.r, p.g, p.b);
    }
  }


  void OrthancPlugins::ApplyDicomToJson(_OrthancPluginService service,
                                        const void* parameters)
  {
    const _OrthancPluginDicomToJson& p =
      *reinterpret_cast<const _OrthancPluginDicomToJson*>(parameters);

    std::unique_ptr<ParsedDicomFile> dicom;

    if (service == _OrthancPluginService_DicomBufferToJson)
    {
      dicom.reset(new ParsedDicomFile(p.buffer, p.size));
    }
    else
    {
      if (p.instanceId == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      std::string content;

      {
        PImpl::ServerContextLock lock(*pimpl_);
        lock.GetContext().ReadDicom(content, p.instanceId);
      }

      dicom.reset(new ParsedDicomFile(content));
    }

    Json::Value json;
    dicom->DatasetToJson(json, Plugins::Convert(p.format), 
                         static_cast<DicomToJsonFlags>(p.flags), p.maxStringLength);

    std::string s;
    Toolbox::WriteFastJson(s, json);
    *p.result = CopyString(s);
  }
        

  void OrthancPlugins::ApplyCreateDicom(const _OrthancPluginCreateDicom& parameters,
                                        const char* privateCreatorC)
  {
    Json::Value json;

    if (parameters.json == NULL)
    {
      json = Json::objectValue;
    }
    else if (!Toolbox::ReadJson(json, parameters.json))
    {
      throw OrthancException(ErrorCode_BadJson);
    }

    std::string dicom;

    {
      // Fix issue 168 (Plugins can't read private tags from the
      // configuration file)
      // https://bugs.orthanc-server.com/show_bug.cgi?id=168
      std::string privateCreator;

      if (privateCreatorC == NULL)
      {
        OrthancConfiguration::ReaderLock lock;
        privateCreator = lock.GetConfiguration().GetDefaultPrivateCreator();
      }
      else
      {
        // New in Orthanc 1.9.0
        privateCreator.assign(privateCreatorC);
      }
      
      std::unique_ptr<ParsedDicomFile> file
        (ParsedDicomFile::CreateFromJson(json, static_cast<DicomFromJsonFlags>(parameters.flags),
                                         privateCreator));

      if (parameters.pixelData)
      {
        file->EmbedImage(*reinterpret_cast<const ImageAccessor*>(parameters.pixelData));
      }

      file->SaveToMemoryBuffer(dicom);
    }

    CopyToMemoryBuffer(*parameters.target, dicom);
  }


  void OrthancPlugins::ComputeHash(_OrthancPluginService service,
                                   const void* parameters)
  {
    const _OrthancPluginComputeHash& p =
      *reinterpret_cast<const _OrthancPluginComputeHash*>(parameters);
 
    std::string hash;
    switch (service)
    {
      case _OrthancPluginService_ComputeMd5:
        Toolbox::ComputeMD5(hash, p.buffer, p.size);
        break;

      case _OrthancPluginService_ComputeSha1:
        Toolbox::ComputeSHA1(hash, p.buffer, p.size);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
   
    *p.result = CopyString(hash);
  }


  void OrthancPlugins::GetTagName(const void* parameters)
  {
    const _OrthancPluginGetTagName& p =
      *reinterpret_cast<const _OrthancPluginGetTagName*>(parameters);

    std::string privateCreator;
    
    if (p.privateCreator != NULL)
    {
      privateCreator = p.privateCreator;
    }
   
    DicomTag tag(p.group, p.element);
    *p.result = CopyString(FromDcmtkBridge::GetTagName(tag, privateCreator));
  }


  void OrthancPlugins::ApplyCreateImage(_OrthancPluginService service,
                                        const void* parameters)
  {
    const _OrthancPluginCreateImage& p =
      *reinterpret_cast<const _OrthancPluginCreateImage*>(parameters);

    std::unique_ptr<ImageAccessor> result;

    switch (service)
    {
      case _OrthancPluginService_CreateImage:
        result.reset(new Image(Plugins::Convert(p.format), p.width, p.height, false));
        break;

      case _OrthancPluginService_CreateImageAccessor:
        result.reset(new ImageAccessor);
        result->AssignWritable(Plugins::Convert(p.format), p.width, p.height, p.pitch, p.buffer);
        break;

      case _OrthancPluginService_DecodeDicomImage:
      {
        PImpl::ServerContextLock lock(*pimpl_);
        result.reset(lock.GetContext().DecodeDicomFrame(p.constBuffer, p.bufferSize, p.frameIndex));
        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    *(p.target) = ReturnImage(result);
  }


  void OrthancPlugins::ApplySendMultipartItem(const void* parameters)
  {
    // An exception might be raised in this function if the
    // connection was closed by the HTTP client.
    const _OrthancPluginAnswerBuffer& p =
      *reinterpret_cast<const _OrthancPluginAnswerBuffer*>(parameters);

    std::map<std::string, std::string> headers;  // No custom headers
    reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->SendMultipartItem(p.answer, p.answerSize, headers);
  }


  void OrthancPlugins::ApplySendMultipartItem2(const void* parameters)
  {
    // An exception might be raised in this function if the
    // connection was closed by the HTTP client.
    const _OrthancPluginSendMultipartItem2& p =
      *reinterpret_cast<const _OrthancPluginSendMultipartItem2*>(parameters);
    
    std::map<std::string, std::string> headers;
    for (uint32_t i = 0; i < p.headersCount; i++)
    {
      headers[p.headersKeys[i]] = p.headersValues[i];
    }
    
    reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->SendMultipartItem(p.answer, p.answerSize, headers);
  }
      

  void OrthancPlugins::DatabaseAnswer(const void* parameters)
  {
    const _OrthancPluginDatabaseAnswer& p =
      *reinterpret_cast<const _OrthancPluginDatabaseAnswer*>(parameters);

    if (pimpl_->database_.get() != NULL)
    {
      pimpl_->database_->AnswerReceived(p);
    }
    else
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "Cannot invoke this service without a custom database back-end");
    }
  }


  namespace
  {
    class DictionaryReadLocker
    {
    private:
      const DcmDataDictionary& dictionary_;

    public:
      DictionaryReadLocker() : dictionary_(dcmDataDict.rdlock())
      {
      }

      ~DictionaryReadLocker()
      {
#if DCMTK_VERSION_NUMBER >= 364
        dcmDataDict.rdunlock();
#else
        dcmDataDict.unlock();
#endif
      }

      const DcmDataDictionary* operator->()
      {
        return &dictionary_;
      }
    };
  }


  void OrthancPlugins::ApplyLookupDictionary(const void* parameters)
  {
    const _OrthancPluginLookupDictionary& p =
      *reinterpret_cast<const _OrthancPluginLookupDictionary*>(parameters);

    DicomTag tag(FromDcmtkBridge::ParseTag(p.name));
    DcmTagKey tag2(tag.GetGroup(), tag.GetElement());

    DictionaryReadLocker locker;
    const DcmDictEntry* entry = NULL;

    if (tag.IsPrivate())
    {
      // Fix issue 168 (Plugins can't read private tags from the
      // configuration file)
      // https://bugs.orthanc-server.com/show_bug.cgi?id=168
      std::string privateCreator;
      {
        OrthancConfiguration::ReaderLock lock;
        privateCreator = lock.GetConfiguration().GetDefaultPrivateCreator();
      }

      entry = locker->findEntry(tag2, privateCreator.c_str());
    }
    else
    {
      entry = locker->findEntry(tag2, NULL);
    }

    if (entry == NULL)
    {
      throw OrthancException(ErrorCode_UnknownDicomTag);
    }
    else
    {
      p.target->group = entry->getKey().getGroup();
      p.target->element = entry->getKey().getElement();
      p.target->vr = Plugins::Convert(FromDcmtkBridge::Convert(entry->getEVR()));
      p.target->minMultiplicity = static_cast<uint32_t>(entry->getVMMin());
      p.target->maxMultiplicity = (entry->getVMMax() == DcmVariableVM ? 0 : static_cast<uint32_t>(entry->getVMMax()));
    }
  }


  bool OrthancPlugins::InvokeSafeService(SharedLibrary& plugin,
                                         _OrthancPluginService service,
                                         const void* parameters)
  {
    // Services that can be run without mutual exclusion

    switch (service)
    {
      case _OrthancPluginService_GetOrthancPath:
      {
        std::string s = SystemToolbox::GetPathToExecutable();
        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters)->result = CopyString(s);
        return true;
      }

      case _OrthancPluginService_GetOrthancDirectory:
      {
        std::string s = SystemToolbox::GetDirectoryOfExecutable();
        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters)->result = CopyString(s);
        return true;
      }

      case _OrthancPluginService_GetConfigurationPath:
      {
        std::string s;

        {
          OrthancConfiguration::ReaderLock lock;
          s = lock.GetConfiguration().GetConfigurationAbsolutePath();
        }

        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters)->result = CopyString(s);
        return true;
      }

      case _OrthancPluginService_GetConfiguration:
      {
        std::string s;

        {
          OrthancConfiguration::ReaderLock lock;
          lock.GetConfiguration().Format(s);
        }

        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters)->result = CopyString(s);
        return true;
      }

      case _OrthancPluginService_BufferCompression:
        BufferCompression(parameters);
        return true;

      case _OrthancPluginService_AnswerBuffer:
        AnswerBuffer(parameters);
        return true;

      case _OrthancPluginService_CompressAndAnswerPngImage:
        CompressAndAnswerPngImage(parameters);
        return true;

      case _OrthancPluginService_CompressAndAnswerImage:
        CompressAndAnswerImage(parameters);
        return true;

      case _OrthancPluginService_GetDicomForInstance:
        GetDicomForInstance(parameters);
        return true;

      case _OrthancPluginService_RestApiGet:
        RestApiGet(parameters, false);
        return true;

      case _OrthancPluginService_RestApiGetAfterPlugins:
        RestApiGet(parameters, true);
        return true;

      case _OrthancPluginService_RestApiGet2:
        RestApiGet2(parameters);
        return true;

      case _OrthancPluginService_RestApiPost:
        RestApiPostPut(true, parameters, false);
        return true;

      case _OrthancPluginService_RestApiPostAfterPlugins:
        RestApiPostPut(true, parameters, true);
        return true;

      case _OrthancPluginService_RestApiDelete:
        RestApiDelete(parameters, false);
        return true;

      case _OrthancPluginService_RestApiDeleteAfterPlugins:
        RestApiDelete(parameters, true);
        return true;

      case _OrthancPluginService_RestApiPut:
        RestApiPostPut(false, parameters, false);
        return true;

      case _OrthancPluginService_RestApiPutAfterPlugins:
        RestApiPostPut(false, parameters, true);
        return true;

      case _OrthancPluginService_Redirect:
        Redirect(parameters);
        return true;

      case _OrthancPluginService_SendUnauthorized:
        SendUnauthorized(parameters);
        return true;

      case _OrthancPluginService_SendMethodNotAllowed:
        SendMethodNotAllowed(parameters);
        return true;

      case _OrthancPluginService_SendHttpStatus:
        SendHttpStatus(parameters);
        return true;

      case _OrthancPluginService_SendHttpStatusCode:
        SendHttpStatusCode(parameters);
        return true;

      case _OrthancPluginService_SetCookie:
        SetCookie(parameters);
        return true;

      case _OrthancPluginService_SetHttpHeader:
        SetHttpHeader(parameters);
        return true;

      case _OrthancPluginService_SetHttpErrorDetails:
        SetHttpErrorDetails(parameters);
        return true;

      case _OrthancPluginService_LookupPatient:
      case _OrthancPluginService_LookupStudy:
      case _OrthancPluginService_LookupStudyWithAccessionNumber:
      case _OrthancPluginService_LookupSeries:
      case _OrthancPluginService_LookupInstance:
        LookupResource(service, parameters);
        return true;

      case _OrthancPluginService_GetInstanceRemoteAet:
      case _OrthancPluginService_GetInstanceSize:
      case _OrthancPluginService_GetInstanceData:
      case _OrthancPluginService_GetInstanceJson:
      case _OrthancPluginService_GetInstanceSimplifiedJson:
      case _OrthancPluginService_HasInstanceMetadata:
      case _OrthancPluginService_GetInstanceMetadata:
      case _OrthancPluginService_GetInstanceOrigin:
      case _OrthancPluginService_GetInstanceTransferSyntaxUid:
      case _OrthancPluginService_HasInstancePixelData:
        AccessDicomInstance(service, parameters);
        return true;

      case _OrthancPluginService_GetInstanceFramesCount:
      case _OrthancPluginService_GetInstanceRawFrame:
      case _OrthancPluginService_GetInstanceDecodedFrame:
      case _OrthancPluginService_SerializeDicomInstance:
      case _OrthancPluginService_GetInstanceAdvancedJson:
      case _OrthancPluginService_GetInstanceDicomWebJson:
      case _OrthancPluginService_GetInstanceDicomWebXml:
        AccessDicomInstance2(service, parameters);
        return true;

      case _OrthancPluginService_SetGlobalProperty:
      {
        const _OrthancPluginGlobalProperty& p = 
          *reinterpret_cast<const _OrthancPluginGlobalProperty*>(parameters);
        if (p.property < 1024)
        {
          return false;
        }
        else
        {
          // TODO - Plugins can only access global properties of their
          // own Orthanc server (no access to the shared global properties)
          PImpl::ServerContextLock lock(*pimpl_);
          lock.GetContext().GetIndex().SetGlobalProperty(static_cast<GlobalProperty>(p.property),
                                                         false /* not shared */, p.value);
          return true;
        }
      }

      case _OrthancPluginService_GetGlobalProperty:
      {
        const _OrthancPluginGlobalProperty& p = 
          *reinterpret_cast<const _OrthancPluginGlobalProperty*>(parameters);

        std::string result;

        {
          // TODO - Plugins can only access global properties of their
          // own Orthanc server (no access to the shared global properties)
          PImpl::ServerContextLock lock(*pimpl_);
          result = lock.GetContext().GetIndex().GetGlobalProperty(static_cast<GlobalProperty>(p.property),
                                                                  false /* not shared */, p.value);
        }

        *(p.result) = CopyString(result);
        return true;
      }

      case _OrthancPluginService_GetExpectedDatabaseVersion:
      {
        const _OrthancPluginReturnSingleValue& p =
          *reinterpret_cast<const _OrthancPluginReturnSingleValue*>(parameters);
        *(p.resultUint32) = ORTHANC_DATABASE_VERSION;
        return true;
      }

      case _OrthancPluginService_StartMultipartAnswer:
      {
        const _OrthancPluginStartMultipartAnswer& p =
          *reinterpret_cast<const _OrthancPluginStartMultipartAnswer*>(parameters);
        reinterpret_cast<PImpl::PluginHttpOutput*>(p.output)->StartMultipart(p.subType, p.contentType);
        return true;
      }

      case _OrthancPluginService_SendMultipartItem:
        ApplySendMultipartItem(parameters);
        return true;

      case _OrthancPluginService_SendMultipartItem2:
        ApplySendMultipartItem2(parameters);
        return true;

      case _OrthancPluginService_ReadFile:
      {
        const _OrthancPluginReadFile& p =
          *reinterpret_cast<const _OrthancPluginReadFile*>(parameters);

        std::string content;
        SystemToolbox::ReadFile(content, p.path);
        CopyToMemoryBuffer(*p.target, content.size() > 0 ? content.c_str() : NULL, content.size());

        return true;
      }

      case _OrthancPluginService_WriteFile:
      {
        const _OrthancPluginWriteFile& p =
          *reinterpret_cast<const _OrthancPluginWriteFile*>(parameters);
        SystemToolbox::WriteFile(p.data, p.size, p.path, true /* run fsync() */);
        return true;
      }

      case _OrthancPluginService_GetErrorDescription:
      {
        const _OrthancPluginGetErrorDescription& p =
          *reinterpret_cast<const _OrthancPluginGetErrorDescription*>(parameters);
        *(p.target) = EnumerationToString(static_cast<ErrorCode>(p.error));
        return true;
      }

      case _OrthancPluginService_GetImagePixelFormat:
      {
        const _OrthancPluginGetImageInfo& p = *reinterpret_cast<const _OrthancPluginGetImageInfo*>(parameters);
        *(p.resultPixelFormat) = Plugins::Convert(reinterpret_cast<const ImageAccessor*>(p.image)->GetFormat());
        return true;
      }

      case _OrthancPluginService_GetImageWidth:
      {
        const _OrthancPluginGetImageInfo& p = *reinterpret_cast<const _OrthancPluginGetImageInfo*>(parameters);
        *(p.resultUint32) = reinterpret_cast<const ImageAccessor*>(p.image)->GetWidth();
        return true;
      }

      case _OrthancPluginService_GetImageHeight:
      {
        const _OrthancPluginGetImageInfo& p = *reinterpret_cast<const _OrthancPluginGetImageInfo*>(parameters);
        *(p.resultUint32) = reinterpret_cast<const ImageAccessor*>(p.image)->GetHeight();
        return true;
      }

      case _OrthancPluginService_GetImagePitch:
      {
        const _OrthancPluginGetImageInfo& p = *reinterpret_cast<const _OrthancPluginGetImageInfo*>(parameters);
        *(p.resultUint32) = reinterpret_cast<const ImageAccessor*>(p.image)->GetPitch();
        return true;
      }

      case _OrthancPluginService_GetImageBuffer:
      {
        const _OrthancPluginGetImageInfo& p = *reinterpret_cast<const _OrthancPluginGetImageInfo*>(parameters);
        *(p.resultBuffer) = const_cast<void*>(reinterpret_cast<const ImageAccessor*>(p.image)->GetConstBuffer());
        return true;
      }

      case _OrthancPluginService_FreeImage:
      {
        const _OrthancPluginFreeImage& p = *reinterpret_cast<const _OrthancPluginFreeImage*>(parameters);

        if (p.image != NULL)
        {
          delete reinterpret_cast<ImageAccessor*>(p.image);
        }

        return true;
      }

      case _OrthancPluginService_UncompressImage:
        UncompressImage(parameters);
        return true;

      case _OrthancPluginService_CompressImage:
        CompressImage(parameters);
        return true;

      case _OrthancPluginService_CallHttpClient:
        CallHttpClient(parameters);
        return true;

      case _OrthancPluginService_CallHttpClient2:
        CallHttpClient2(parameters);
        return true;

      case _OrthancPluginService_ChunkedHttpClient:
        ChunkedHttpClient(parameters);
        return true;

      case _OrthancPluginService_CallRestApi:
        CallRestApi(parameters);
        return true;

      case _OrthancPluginService_ConvertPixelFormat:
        ConvertPixelFormat(parameters);
        return true;

      case _OrthancPluginService_GetFontsCount:
      {
        const _OrthancPluginReturnSingleValue& p =
          *reinterpret_cast<const _OrthancPluginReturnSingleValue*>(parameters);

        {
          OrthancConfiguration::ReaderLock lock;
          *(p.resultUint32) = lock.GetConfiguration().GetFontRegistry().GetSize();
        }

        return true;
      }

      case _OrthancPluginService_GetFontInfo:
        GetFontInfo(parameters);
        return true;

      case _OrthancPluginService_DrawText:
        DrawText(parameters);
        return true;

      case _OrthancPluginService_StorageAreaCreate:
      {
        const _OrthancPluginStorageAreaCreate& p =
          *reinterpret_cast<const _OrthancPluginStorageAreaCreate*>(parameters);
        IStorageArea& storage = *reinterpret_cast<IStorageArea*>(p.storageArea);
        storage.Create(p.uuid, p.content, static_cast<size_t>(p.size), Plugins::Convert(p.type));
        return true;
      }

      case _OrthancPluginService_StorageAreaRead:
      {
        const _OrthancPluginStorageAreaRead& p =
          *reinterpret_cast<const _OrthancPluginStorageAreaRead*>(parameters);
        IStorageArea& storage = *reinterpret_cast<IStorageArea*>(p.storageArea);
        std::unique_ptr<IMemoryBuffer> content(storage.Read(p.uuid, Plugins::Convert(p.type)));
        CopyToMemoryBuffer(*p.target, content->GetData(), content->GetSize());
        return true;
      }

      case _OrthancPluginService_StorageAreaRemove:
      {
        const _OrthancPluginStorageAreaRemove& p =
          *reinterpret_cast<const _OrthancPluginStorageAreaRemove*>(parameters);
        IStorageArea& storage = *reinterpret_cast<IStorageArea*>(p.storageArea);
        storage.Remove(p.uuid, Plugins::Convert(p.type));
        return true;
      }

      case _OrthancPluginService_DicomBufferToJson:
      case _OrthancPluginService_DicomInstanceToJson:
        ApplyDicomToJson(service, parameters);
        return true;

      case _OrthancPluginService_CreateDicom:
      {
        const _OrthancPluginCreateDicom& p =
          *reinterpret_cast<const _OrthancPluginCreateDicom*>(parameters);
        ApplyCreateDicom(p, NULL);
        return true;
      }

      case _OrthancPluginService_CreateDicom2:
      {
        // New in Orthanc 1.9.0
        const _OrthancPluginCreateDicom2& p =
          *reinterpret_cast<const _OrthancPluginCreateDicom2*>(parameters);
        ApplyCreateDicom(p.createDicom, p.privateCreator);
        return true;
      }

      case _OrthancPluginService_WorklistAddAnswer:
      {
        const _OrthancPluginWorklistAnswersOperation& p =
          *reinterpret_cast<const _OrthancPluginWorklistAnswersOperation*>(parameters);
        reinterpret_cast<const WorklistHandler*>(p.query)->AddAnswer(p.answers, p.dicom, p.size);
        return true;
      }

      case _OrthancPluginService_WorklistMarkIncomplete:
      {
        const _OrthancPluginWorklistAnswersOperation& p =
          *reinterpret_cast<const _OrthancPluginWorklistAnswersOperation*>(parameters);
        reinterpret_cast<DicomFindAnswers*>(p.answers)->SetComplete(false);
        return true;
      }

      case _OrthancPluginService_WorklistIsMatch:
      {
        const _OrthancPluginWorklistQueryOperation& p =
          *reinterpret_cast<const _OrthancPluginWorklistQueryOperation*>(parameters);
        *p.isMatch = reinterpret_cast<const WorklistHandler*>(p.query)->IsMatch(p.dicom, p.size);
        return true;
      }

      case _OrthancPluginService_WorklistGetDicomQuery:
      {
        const _OrthancPluginWorklistQueryOperation& p =
          *reinterpret_cast<const _OrthancPluginWorklistQueryOperation*>(parameters);
        reinterpret_cast<const WorklistHandler*>(p.query)->GetDicomQuery(*p.target);
        return true;
      }

      case _OrthancPluginService_FindAddAnswer:
      {
        const _OrthancPluginFindOperation& p =
          *reinterpret_cast<const _OrthancPluginFindOperation*>(parameters);
        reinterpret_cast<DicomFindAnswers*>(p.answers)->Add(p.dicom, p.size);
        return true;
      }

      case _OrthancPluginService_FindMarkIncomplete:
      {
        const _OrthancPluginFindOperation& p =
          *reinterpret_cast<const _OrthancPluginFindOperation*>(parameters);
        reinterpret_cast<DicomFindAnswers*>(p.answers)->SetComplete(false);
        return true;
      }

      case _OrthancPluginService_GetFindQuerySize:
      case _OrthancPluginService_GetFindQueryTag:
      case _OrthancPluginService_GetFindQueryTagName:
      case _OrthancPluginService_GetFindQueryValue:
      {
        const _OrthancPluginFindOperation& p =
          *reinterpret_cast<const _OrthancPluginFindOperation*>(parameters);
        reinterpret_cast<const FindHandler*>(p.query)->Invoke(service, p);
        return true;
      }

      case _OrthancPluginService_CreateImage:
      case _OrthancPluginService_CreateImageAccessor:
      case _OrthancPluginService_DecodeDicomImage:
        ApplyCreateImage(service, parameters);
        return true;

      case _OrthancPluginService_ComputeMd5:
      case _OrthancPluginService_ComputeSha1:
        ComputeHash(service, parameters);
        return true;

      case _OrthancPluginService_LookupDictionary:
        ApplyLookupDictionary(parameters);
        return true;

      case _OrthancPluginService_GenerateUuid:
      {
        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters)->result = 
          CopyString(Toolbox::GenerateUuid());
        return true;
      }

      case _OrthancPluginService_CreateFindMatcher:
      {
        const _OrthancPluginCreateFindMatcher& p =
          *reinterpret_cast<const _OrthancPluginCreateFindMatcher*>(parameters);
        ParsedDicomFile query(p.query, p.size);
        *(p.target) = reinterpret_cast<OrthancPluginFindMatcher*>(new HierarchicalMatcher(query));
        return true;
      }

      case _OrthancPluginService_FreeFindMatcher:
      {
        const _OrthancPluginFreeFindMatcher& p =
          *reinterpret_cast<const _OrthancPluginFreeFindMatcher*>(parameters);

        if (p.matcher != NULL)
        {
          delete reinterpret_cast<HierarchicalMatcher*>(p.matcher);
        }

        return true;
      }

      case _OrthancPluginService_FindMatcherIsMatch:
      {
        const _OrthancPluginFindMatcherIsMatch& p =
          *reinterpret_cast<const _OrthancPluginFindMatcherIsMatch*>(parameters);

        if (p.matcher == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        else
        {
          ParsedDicomFile query(p.dicom, p.size);
          *p.isMatch = reinterpret_cast<const HierarchicalMatcher*>(p.matcher)->Match(query) ? 1 : 0;
          return true;
        }
      }

      case _OrthancPluginService_GetPeers:
      {
        const _OrthancPluginGetPeers& p =
          *reinterpret_cast<const _OrthancPluginGetPeers*>(parameters);
        *(p.peers) = reinterpret_cast<OrthancPluginPeers*>(new OrthancPeers);
        return true;
      }

      case _OrthancPluginService_FreePeers:
      {
        const _OrthancPluginFreePeers& p =
          *reinterpret_cast<const _OrthancPluginFreePeers*>(parameters);

        if (p.peers != NULL)
        {
          delete reinterpret_cast<OrthancPeers*>(p.peers);
        }
        
        return true;
      }

      case _OrthancPluginService_GetPeersCount:
      {
        const _OrthancPluginGetPeersCount& p =
          *reinterpret_cast<const _OrthancPluginGetPeersCount*>(parameters);

        if (p.peers == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        else
        {
          *(p.target) = reinterpret_cast<const OrthancPeers*>(p.peers)->GetPeersCount();
          return true;
        }
      }

      case _OrthancPluginService_GetPeerName:
      {
        const _OrthancPluginGetPeerProperty& p =
          *reinterpret_cast<const _OrthancPluginGetPeerProperty*>(parameters);

        if (p.peers == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        else
        {
          *(p.target) = reinterpret_cast<const OrthancPeers*>(p.peers)->GetPeerName(p.peerIndex).c_str();
          return true;
        }
      }

      case _OrthancPluginService_GetPeerUrl:
      {
        const _OrthancPluginGetPeerProperty& p =
          *reinterpret_cast<const _OrthancPluginGetPeerProperty*>(parameters);

        if (p.peers == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        else
        {
          *(p.target) = reinterpret_cast<const OrthancPeers*>(p.peers)->GetPeerParameters(p.peerIndex).GetUrl().c_str();
          return true;
        }
      }

      case _OrthancPluginService_GetPeerUserProperty:
      {
        const _OrthancPluginGetPeerProperty& p =
          *reinterpret_cast<const _OrthancPluginGetPeerProperty*>(parameters);

        if (p.peers == NULL ||
            p.userProperty == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
        else
        {
          const WebServiceParameters::Dictionary& properties = 
            reinterpret_cast<const OrthancPeers*>(p.peers)->GetPeerParameters(p.peerIndex).GetUserProperties();

          WebServiceParameters::Dictionary::const_iterator found =
            properties.find(p.userProperty);

          if (found == properties.end())
          {
            *(p.target) = NULL;
          }
          else
          {
            *(p.target) = found->second.c_str();
          }

          return true;
        }
      }

      case _OrthancPluginService_CallPeerApi:
        CallPeerApi(parameters);
        return true;

      case _OrthancPluginService_CreateJob:
      {
        const _OrthancPluginCreateJob& p =
          *reinterpret_cast<const _OrthancPluginCreateJob*>(parameters);
        *(p.target) = reinterpret_cast<OrthancPluginJob*>(new PluginsJob(p));
        return true;
      }

      case _OrthancPluginService_FreeJob:
      {
        const _OrthancPluginFreeJob& p =
          *reinterpret_cast<const _OrthancPluginFreeJob*>(parameters);

        if (p.job != NULL)
        {
          delete reinterpret_cast<PluginsJob*>(p.job);
        }

        return true;
      }

      case _OrthancPluginService_SubmitJob:
      {
        const _OrthancPluginSubmitJob& p =
          *reinterpret_cast<const _OrthancPluginSubmitJob*>(parameters);

        std::string uuid;

        PImpl::ServerContextLock lock(*pimpl_);
        lock.GetContext().GetJobsEngine().GetRegistry().Submit
          (uuid, reinterpret_cast<PluginsJob*>(p.job), p.priority);
        
        *p.resultId = CopyString(uuid);

        return true;
      }

      case _OrthancPluginService_AutodetectMimeType:
      {
        const _OrthancPluginRetrieveStaticString& p =
          *reinterpret_cast<const _OrthancPluginRetrieveStaticString*>(parameters);
        *p.result = EnumerationToString(SystemToolbox::AutodetectMimeType(p.argument));
        return true;
      }

      case _OrthancPluginService_SetMetricsValue:
      {
        const _OrthancPluginSetMetricsValue& p =
          *reinterpret_cast<const _OrthancPluginSetMetricsValue*>(parameters);

        MetricsType type;
        switch (p.type)
        {
          case OrthancPluginMetricsType_Default:
            type = MetricsType_Default;
            break;

          case OrthancPluginMetricsType_Timer:
            type = MetricsType_MaxOver10Seconds;
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        
        {
          PImpl::ServerContextLock lock(*pimpl_);
          lock.GetContext().GetMetricsRegistry().SetValue(p.name, p.value, type);
        }

        return true;
      }

      case _OrthancPluginService_EncodeDicomWebJson:
      case _OrthancPluginService_EncodeDicomWebXml:
      {
        const _OrthancPluginEncodeDicomWeb& p =
          *reinterpret_cast<const _OrthancPluginEncodeDicomWeb*>(parameters);

        DicomWebBinaryFormatter formatter(p.callback);
        formatter.Apply(p.target,
                        (service == _OrthancPluginService_EncodeDicomWebJson),
                        p.dicom, p.dicomSize);
        return true;
      }

      case _OrthancPluginService_EncodeDicomWebJson2:
      case _OrthancPluginService_EncodeDicomWebXml2:
      {
        const _OrthancPluginEncodeDicomWeb2& p =
          *reinterpret_cast<const _OrthancPluginEncodeDicomWeb2*>(parameters);

        DicomWebBinaryFormatter formatter(p.callback, p.payload);
        formatter.Apply(p.target,
                        (service == _OrthancPluginService_EncodeDicomWebJson2),
                        p.dicom, p.dicomSize);
        return true;
      }

      case _OrthancPluginService_GetTagName:
        GetTagName(parameters);
        return true;

      case _OrthancPluginService_CreateDicomInstance:
      {
        const _OrthancPluginCreateDicomInstance& p =
          *reinterpret_cast<const _OrthancPluginCreateDicomInstance*>(parameters);
        *(p.target) = reinterpret_cast<OrthancPluginDicomInstance*>(
          new DicomInstanceFromBuffer(p.buffer, p.size));
        return true;
      }
        
      case _OrthancPluginService_FreeDicomInstance:
      {
        const _OrthancPluginFreeDicomInstance& p =
          *reinterpret_cast<const _OrthancPluginFreeDicomInstance*>(parameters);

        if (p.dicom != NULL)
        {
          IDicomInstance* obj = reinterpret_cast<IDicomInstance*>(p.dicom);
          
          if (obj->CanBeFreed())
          {
            delete obj;
          }
          else
          {
            throw OrthancException(ErrorCode_Plugin, "Cannot free a DICOM instance provided to a callback");
          }
        }

        return true;
      }

      case _OrthancPluginService_TranscodeDicomInstance:
      {
        const _OrthancPluginCreateDicomInstance& p =
          *reinterpret_cast<const _OrthancPluginCreateDicomInstance*>(parameters);

        DicomTransferSyntax transferSyntax;
        if (p.transferSyntax == NULL ||
            !LookupTransferSyntax(transferSyntax, p.transferSyntax))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange, "Unsupported transfer syntax: " +
                                 std::string(p.transferSyntax == NULL ? "(null)" : p.transferSyntax));
        }
        else
        {
          std::set<DicomTransferSyntax> syntaxes;
          syntaxes.insert(transferSyntax);

          IDicomTranscoder::DicomImage source;
          source.SetExternalBuffer(p.buffer, p.size);

          IDicomTranscoder::DicomImage transcoded;
          bool success;
          
          {
            PImpl::ServerContextLock lock(*pimpl_);
            success = lock.GetContext().Transcode(
              transcoded, source, syntaxes, true /* allow new sop */);
          }

          if (success)
          {
            *(p.target) = reinterpret_cast<OrthancPluginDicomInstance*>(
              new DicomInstanceFromTranscoded(transcoded));
            return true;
          }
          else
          {
            throw OrthancException(ErrorCode_NotImplemented, "Cannot transcode image");
          }
        }
      }

      case _OrthancPluginService_CreateMemoryBuffer:
      {
        const _OrthancPluginCreateMemoryBuffer& p =
          *reinterpret_cast<const _OrthancPluginCreateMemoryBuffer*>(parameters);

        p.target->data = NULL;
        p.target->size = 0;
        
        if (p.size != 0)
        {
          p.target->data = malloc(p.size);
          if (p.target->data == NULL)
          {
            throw OrthancException(ErrorCode_NotEnoughMemory);
          }

          p.target->size = p.size;
        }          
        
        return true;
      }

      case _OrthancPluginService_CreateMemoryBuffer64:
      {
        const _OrthancPluginCreateMemoryBuffer64& p =
          *reinterpret_cast<const _OrthancPluginCreateMemoryBuffer64*>(parameters);

        p.target->data = NULL;
        p.target->size = 0;
        
        if (p.size != 0)
        {
          p.target->data = malloc(p.size);
          if (p.target->data == NULL)
          {
            throw OrthancException(ErrorCode_NotEnoughMemory);
          }

          p.target->size = p.size;
        }          

        return true;
      }
        
      case _OrthancPluginService_RegisterIncomingHttpRequestFilter:
        RegisterIncomingHttpRequestFilter(parameters);
        return true;

      case _OrthancPluginService_RegisterIncomingHttpRequestFilter2:
        RegisterIncomingHttpRequestFilter2(parameters);
        return true;

      default:
        return false;
    }
  }



  bool OrthancPlugins::InvokeProtectedService(SharedLibrary& plugin,
                                              _OrthancPluginService service,
                                              const void* parameters)
  {
    // Services that must be run in mutual exclusion. Guideline:
    // Whenever "pimpl_" is directly accessed by the service, it
    // should be listed here.
    
    switch (service)
    {
      case _OrthancPluginService_RegisterRestCallback:
        RegisterRestCallback(parameters, true);
        return true;

      case _OrthancPluginService_RegisterRestCallbackNoLock:
        RegisterRestCallback(parameters, false);
        return true;

      case _OrthancPluginService_RegisterChunkedRestCallback:
        RegisterChunkedRestCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterOnStoredInstanceCallback:
        RegisterOnStoredInstanceCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterOnChangeCallback:
        RegisterOnChangeCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterWorklistCallback:
        RegisterWorklistCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterFindCallback:
        RegisterFindCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterMoveCallback:
        RegisterMoveCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterDecodeImageCallback:
        RegisterDecodeImageCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterTranscoderCallback:
        RegisterTranscoderCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterJobsUnserializer:
        RegisterJobsUnserializer(parameters);
        return true;

      case _OrthancPluginService_RegisterIncomingDicomInstanceFilter:
        RegisterIncomingDicomInstanceFilter(parameters);
        return true;

      case _OrthancPluginService_RegisterIncomingCStoreInstanceFilter:
        RegisterIncomingCStoreInstanceFilter(parameters);
        return true;

      case _OrthancPluginService_RegisterReceivedInstanceCallback:
        RegisterReceivedInstanceCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterRefreshMetricsCallback:
        RegisterRefreshMetricsCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterStorageCommitmentScpCallback:
        RegisterStorageCommitmentScpCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterStorageArea:
      case _OrthancPluginService_RegisterStorageArea2:
      {
        CLOG(INFO, PLUGINS) << "Plugin has registered a custom storage area";
        
        if (pimpl_->storageArea_.get() == NULL)
        {
          if (service == _OrthancPluginService_RegisterStorageArea)
          {
            const _OrthancPluginRegisterStorageArea& p = 
              *reinterpret_cast<const _OrthancPluginRegisterStorageArea*>(parameters);
            pimpl_->storageArea_.reset(new StorageAreaFactory(plugin, p, GetErrorDictionary()));
          }
          else if (service == _OrthancPluginService_RegisterStorageArea2)
          {
            const _OrthancPluginRegisterStorageArea2& p = 
              *reinterpret_cast<const _OrthancPluginRegisterStorageArea2*>(parameters);
            pimpl_->storageArea_.reset(new StorageAreaFactory(plugin, p, GetErrorDictionary()));
          }
          else
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          throw OrthancException(ErrorCode_StorageAreaAlreadyRegistered);
        }

        return true;
      }

      case _OrthancPluginService_SetPluginProperty:
      {
        const _OrthancPluginSetPluginProperty& p = 
          *reinterpret_cast<const _OrthancPluginSetPluginProperty*>(parameters);
        pimpl_->properties_[std::make_pair(p.plugin, p.property)] = p.value;
        return true;
      }

      case _OrthancPluginService_GetCommandLineArgumentsCount:
      {
        const _OrthancPluginReturnSingleValue& p =
          *reinterpret_cast<const _OrthancPluginReturnSingleValue*>(parameters);
        *(p.resultUint32) = pimpl_->argc_ - 1;
        return true;
      }

      case _OrthancPluginService_GetCommandLineArgument:
      {
        const _OrthancPluginGlobalProperty& p =
          *reinterpret_cast<const _OrthancPluginGlobalProperty*>(parameters);
        
        if (p.property + 1 > pimpl_->argc_)
        {
          return false;
        }
        else
        {
          std::string arg = std::string(pimpl_->argv_[p.property + 1]);
          *(p.result) = CopyString(arg);
          return true;
        }
      }

      case _OrthancPluginService_RegisterDatabaseBackend:
      {
        LOG(WARNING) << "Performance warning: Plugin has registered a custom database back-end with an old API";
        LOG(WARNING) << "The database backend has *no* support for revisions of metadata and attachments";

        const _OrthancPluginRegisterDatabaseBackend& p =
          *reinterpret_cast<const _OrthancPluginRegisterDatabaseBackend*>(parameters);

        if (pimpl_->database_.get() == NULL &&
            pimpl_->databaseV3_.get() == NULL)
        {
          pimpl_->database_.reset(new OrthancPluginDatabase(plugin, GetErrorDictionary(), 
                                                            *p.backend, NULL, 0, p.payload));
        }
        else
        {
          throw OrthancException(ErrorCode_DatabaseBackendAlreadyRegistered);
        }

        *(p.result) = reinterpret_cast<OrthancPluginDatabaseContext*>(pimpl_->database_.get());

        return true;
      }

      case _OrthancPluginService_RegisterDatabaseBackendV2:
      {
        LOG(WARNING) << "Performance warning: Plugin has registered a custom database back-end with an old API";
        LOG(WARNING) << "The database backend has *no* support for revisions of metadata and attachments";

        const _OrthancPluginRegisterDatabaseBackendV2& p =
          *reinterpret_cast<const _OrthancPluginRegisterDatabaseBackendV2*>(parameters);

        if (pimpl_->database_.get() == NULL &&
            pimpl_->databaseV3_.get() == NULL)
        {
          pimpl_->database_.reset(new OrthancPluginDatabase(plugin, GetErrorDictionary(),
                                                            *p.backend, p.extensions,
                                                            p.extensionsSize, p.payload));
        }
        else
        {
          throw OrthancException(ErrorCode_DatabaseBackendAlreadyRegistered);
        }

        *(p.result) = reinterpret_cast<OrthancPluginDatabaseContext*>(pimpl_->database_.get());

        return true;
      }

      case _OrthancPluginService_RegisterDatabaseBackendV3:
      {
        CLOG(INFO, PLUGINS) << "Plugin has registered a custom database back-end";

        const _OrthancPluginRegisterDatabaseBackendV3& p =
          *reinterpret_cast<const _OrthancPluginRegisterDatabaseBackendV3*>(parameters);

        if (pimpl_->database_.get() == NULL &&
            pimpl_->databaseV3_.get() == NULL)
        {
          pimpl_->databaseV3_.reset(new OrthancPluginDatabaseV3(plugin, GetErrorDictionary(), p.backend,
                                                                p.backendSize, p.database, pimpl_->databaseServerIdentifier_));
          pimpl_->maxDatabaseRetries_ = p.maxDatabaseRetries;
        }
        else
        {
          throw OrthancException(ErrorCode_DatabaseBackendAlreadyRegistered);
        }

        return true;
      }

      case _OrthancPluginService_DatabaseAnswer:
        throw OrthancException(ErrorCode_InternalError);   // Implemented before locking (*)

      case _OrthancPluginService_RegisterErrorCode:
      {
        const _OrthancPluginRegisterErrorCode& p =
          *reinterpret_cast<const _OrthancPluginRegisterErrorCode*>(parameters);
        *(p.target) = pimpl_->dictionary_.Register(plugin, p.code, p.httpStatus, p.message);
        return true;
      }

      case _OrthancPluginService_RegisterDictionaryTag:
      {
        const _OrthancPluginRegisterDictionaryTag& p =
          *reinterpret_cast<const _OrthancPluginRegisterDictionaryTag*>(parameters);
        FromDcmtkBridge::RegisterDictionaryTag(DicomTag(p.group, p.element),
                                               Plugins::Convert(p.vr), p.name,
                                               p.minMultiplicity, p.maxMultiplicity, "");
        return true;
      }

      case _OrthancPluginService_RegisterPrivateDictionaryTag:
      {
        const _OrthancPluginRegisterPrivateDictionaryTag& p =
          *reinterpret_cast<const _OrthancPluginRegisterPrivateDictionaryTag*>(parameters);
        FromDcmtkBridge::RegisterDictionaryTag(DicomTag(p.group, p.element),
                                               Plugins::Convert(p.vr), p.name,
                                               p.minMultiplicity, p.maxMultiplicity, p.privateCreator);
        return true;
      }

      case _OrthancPluginService_ReconstructMainDicomTags:
      {
        const _OrthancPluginReconstructMainDicomTags& p =
          *reinterpret_cast<const _OrthancPluginReconstructMainDicomTags*>(parameters);

        if (pimpl_->database_.get() == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin,
                                 "The service ReconstructMainDicomTags can only be invoked by custom database plugins");
        }

        VoidDatabaseListener listener;
        
        {
          IStorageArea& storage = *reinterpret_cast<IStorageArea*>(p.storageArea);

          std::unique_ptr<IDatabaseWrapper::ITransaction> transaction(
            pimpl_->database_->StartTransaction(TransactionType_ReadWrite, listener));
          ServerToolbox::ReconstructMainDicomTags(*transaction, storage, Plugins::Convert(p.level));
          transaction->Commit(0);
        }

        return true;
      }

      case _OrthancPluginService_GenerateRestApiAuthorizationToken:
      {
        const _OrthancPluginRetrieveDynamicString& p = 
          *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(parameters);
        const std::string token = Toolbox::GenerateUuid();

        pimpl_->authorizationTokens_.insert(token);
        *p.result = CopyString("Bearer " + token);

        return true;
      }

      case _OrthancPluginService_RegisterWebDavCollection:
      {
        CLOG(INFO, PLUGINS) << "Plugin has registered a WebDAV collection";
        const _OrthancPluginRegisterWebDavCollection& p =
          *reinterpret_cast<const _OrthancPluginRegisterWebDavCollection*>(parameters);
        pimpl_->webDavCollections_.push_back(new WebDavCollection(GetErrorDictionary(), p));
        return true;
      }

      case _OrthancPluginService_GetDatabaseServerIdentifier:
      {
        const _OrthancPluginRetrieveStaticString& p =
          *reinterpret_cast<const _OrthancPluginRetrieveStaticString*>(parameters);

        *p.result = pimpl_->databaseServerIdentifier_.c_str();

        return true;
      }

      default:
      {
        // This service is unknown to the Orthanc plugin engine
        return false;
      }
    }
  }



  bool OrthancPlugins::InvokeService(SharedLibrary& plugin,
                                     _OrthancPluginService service,
                                     const void* parameters)
  {
    CLOG(TRACE, PLUGINS) << "Calling service " << service << " from plugin " << plugin.GetPath();

    if (service == _OrthancPluginService_DatabaseAnswer)
    {
      // This case solves a deadlock at (*) reported by James Webster
      // on 2015-10-27 that was present in versions of Orthanc <=
      // 0.9.4 and related to database plugins implementing a custom
      // index. The problem was that locking the database is already
      // ensured by the "ServerIndex" class if the invoked service is
      // "DatabaseAnswer".
      DatabaseAnswer(parameters);
      return true;
    }

    if (InvokeSafeService(plugin, service, parameters))
    {
      // The invoked service does not require locking
      return true;
    }
    else
    {
      // The invoked service requires locking
      boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);   // (*)
      return InvokeProtectedService(plugin, service, parameters);
    }
  }


  bool OrthancPlugins::HasStorageArea() const
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    return pimpl_->storageArea_.get() != NULL;
  }
  
  bool OrthancPlugins::HasDatabaseBackend() const
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    return (pimpl_->database_.get() != NULL ||
            pimpl_->databaseV3_.get() != NULL);
  }


  IStorageArea* OrthancPlugins::CreateStorageArea()
  {
    if (!HasStorageArea())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return pimpl_->storageArea_->Create();
    }
  }


  const SharedLibrary& OrthancPlugins::GetStorageAreaLibrary() const
  {
    if (!HasStorageArea())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return pimpl_->storageArea_->GetSharedLibrary();
    }
  }


  IDatabaseWrapper& OrthancPlugins::GetDatabaseBackend()
  {
    if (pimpl_->database_.get() != NULL)
    {
      return *pimpl_->database_;
    }
    else if (pimpl_->databaseV3_.get() != NULL)
    {
      return *pimpl_->databaseV3_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const SharedLibrary& OrthancPlugins::GetDatabaseBackendLibrary() const
  {
    if (pimpl_->database_.get() != NULL)
    {
      return pimpl_->database_->GetSharedLibrary();
    }
    else if (pimpl_->databaseV3_.get() != NULL)
    {
      return pimpl_->databaseV3_->GetSharedLibrary();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const char* OrthancPlugins::GetProperty(const char* plugin,
                                          _OrthancPluginProperty property) const
  {
    PImpl::Property p = std::make_pair(plugin, property);
    PImpl::Properties::const_iterator it = pimpl_->properties_.find(p);

    if (it == pimpl_->properties_.end())
    {
      return NULL;
    }
    else
    {
      return it->second.c_str();
    }
  }


  void OrthancPlugins::SetCommandLineArguments(int argc, char* argv[])
  {
    if (argc < 1 || argv == NULL)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    pimpl_->argc_ = argc;
    pimpl_->argv_ = argv;
  }


  PluginsManager& OrthancPlugins::GetManager()
  {
    return pimpl_->manager_;
  }


  const PluginsManager& OrthancPlugins::GetManager() const
  {
    return pimpl_->manager_;
  }


  PluginsErrorDictionary&  OrthancPlugins::GetErrorDictionary()
  {
    return pimpl_->dictionary_;
  }


  IWorklistRequestHandler* OrthancPlugins::ConstructWorklistRequestHandler()
  {
    if (HasWorklistHandler())
    {
      return new WorklistHandler(*this);
    }
    else
    {
      return NULL;
    }
  }


  bool OrthancPlugins::HasWorklistHandler()
  {
    boost::mutex::scoped_lock lock(pimpl_->worklistCallbackMutex_);
    return pimpl_->worklistCallback_ != NULL;
  }


  IFindRequestHandler* OrthancPlugins::ConstructFindRequestHandler()
  {
    if (HasFindHandler())
    {
      return new FindHandler(*this);
    }
    else
    {
      return NULL;
    }
  }


  bool OrthancPlugins::HasFindHandler()
  {
    boost::mutex::scoped_lock lock(pimpl_->findCallbackMutex_);
    return pimpl_->findCallback_ != NULL;
  }


  IMoveRequestHandler* OrthancPlugins::ConstructMoveRequestHandler()
  {
    if (HasMoveHandler())
    {
      return new MoveHandler(*this);
    }
    else
    {
      return NULL;
    }
  }


  bool OrthancPlugins::HasMoveHandler()
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    return pimpl_->moveCallbacks_.callback != NULL;
  }


  bool OrthancPlugins::HasCustomImageDecoder()
  {
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);
    return !pimpl_->decodeImageCallbacks_.empty();
  }


  bool OrthancPlugins::HasCustomTranscoder()
  {
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);
    return !pimpl_->transcoderCallbacks_.empty();
  }


  ImageAccessor* OrthancPlugins::Decode(const void* dicom,
                                        size_t size,
                                        unsigned int frame)
  {
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);

    for (PImpl::DecodeImageCallbacks::const_iterator
           decoder = pimpl_->decodeImageCallbacks_.begin();
         decoder != pimpl_->decodeImageCallbacks_.end(); ++decoder)
    {
      OrthancPluginImage* pluginImage = NULL;
      if ((*decoder) (&pluginImage, dicom, size, frame) == OrthancPluginErrorCode_Success &&
          pluginImage != NULL)
      {
        return reinterpret_cast<ImageAccessor*>(pluginImage);
      }
    }

    return NULL;
  }

  
  bool OrthancPlugins::IsAllowed(HttpMethod method,
                                 const char* uri,
                                 const char* ip,
                                 const char* username,
                                 const HttpToolbox::Arguments& httpHeaders,
                                 const HttpToolbox::GetArguments& getArguments)
  {
    OrthancPluginHttpMethod cMethod = Plugins::Convert(method);

    std::vector<const char*> httpKeys(httpHeaders.size());
    std::vector<const char*> httpValues(httpHeaders.size());

    size_t pos = 0;
    for (HttpToolbox::Arguments::const_iterator
           it = httpHeaders.begin(); it != httpHeaders.end(); ++it, pos++)
    {
      httpKeys[pos] = it->first.c_str();
      httpValues[pos] = it->second.c_str();
    }

    std::vector<const char*> getKeys(getArguments.size());
    std::vector<const char*> getValues(getArguments.size());

    for (size_t i = 0; i < getArguments.size(); i++)
    {
      getKeys[i] = getArguments[i].first.c_str();
      getValues[i] = getArguments[i].second.c_str();
    }

    {
      boost::shared_lock<boost::shared_mutex> lock(pimpl_->incomingHttpRequestFilterMutex_);
    
      // Improved callback with support for GET arguments, since Orthanc 1.3.0
      for (PImpl::IncomingHttpRequestFilters2::const_iterator
             filter = pimpl_->incomingHttpRequestFilters2_.begin();
           filter != pimpl_->incomingHttpRequestFilters2_.end(); ++filter)
      {
        int32_t allowed = (*filter) (cMethod, uri, ip,
                                     httpKeys.size(),
                                     httpKeys.empty() ? NULL : &httpKeys[0],
                                     httpValues.empty() ? NULL : &httpValues[0],
                                     getKeys.size(),
                                     getKeys.empty() ? NULL : &getKeys[0],
                                     getValues.empty() ? NULL : &getValues[0]);

        if (allowed == 0)
        {
          return false;
        }
        else if (allowed != 1)
        {
          // The callback is only allowed to answer 0 or 1
          throw OrthancException(ErrorCode_Plugin);
        }
      }

      for (PImpl::IncomingHttpRequestFilters::const_iterator
             filter = pimpl_->incomingHttpRequestFilters_.begin();
           filter != pimpl_->incomingHttpRequestFilters_.end(); ++filter)
      {
        int32_t allowed = (*filter) (cMethod, uri, ip, httpKeys.size(),
                                     httpKeys.empty() ? NULL : &httpKeys[0],
                                     httpValues.empty() ? NULL : &httpValues[0]);

        if (allowed == 0)
        {
          return false;
        }
        else if (allowed != 1)
        {
          // The callback is only allowed to answer 0 or 1
          throw OrthancException(ErrorCode_Plugin);
        }
      }
    }

    return true;
  }


  IJob* OrthancPlugins::UnserializeJob(const std::string& type,
                                       const Json::Value& value)
  {
    const std::string serialized = value.toStyledString();

    boost::mutex::scoped_lock lock(pimpl_->jobsUnserializersMutex_);

    for (PImpl::JobsUnserializers::iterator 
           unserializer = pimpl_->jobsUnserializers_.begin();
         unserializer != pimpl_->jobsUnserializers_.end(); ++unserializer)
    {
      OrthancPluginJob* job = (*unserializer) (type.c_str(), serialized.c_str());
      if (job != NULL)
      {
        return reinterpret_cast<PluginsJob*>(job);
      }
    }

    return NULL;
  }


  void OrthancPlugins::RefreshMetrics()
  {
    boost::mutex::scoped_lock lock(pimpl_->refreshMetricsMutex_);

    for (PImpl::RefreshMetricsCallbacks::iterator 
           it = pimpl_->refreshMetricsCallbacks_.begin();
         it != pimpl_->refreshMetricsCallbacks_.end(); ++it)
    {
      if (*it != NULL)
      {
        (*it) ();
      }
    }
  }


  class OrthancPlugins::HttpServerChunkedReader : public IHttpHandler::IChunkedRequestReader
  {
  private:
    OrthancPluginServerChunkedRequestReader*  reader_;
    _OrthancPluginChunkedRestCallback         parameters_;
    PluginsErrorDictionary&                   errorDictionary_;

  public:
    HttpServerChunkedReader(OrthancPluginServerChunkedRequestReader* reader,
                            const _OrthancPluginChunkedRestCallback& parameters,
                            PluginsErrorDictionary& errorDictionary) :
      reader_(reader),
      parameters_(parameters),
      errorDictionary_(errorDictionary)
    {
      assert(reader_ != NULL);
    }

    virtual ~HttpServerChunkedReader()
    {
      assert(reader_ != NULL);
      parameters_.finalize(reader_);
    }

    virtual void AddBodyChunk(const void* data,
                              size_t size) ORTHANC_OVERRIDE
    {
      if (static_cast<uint32_t>(size) != size)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory, ERROR_MESSAGE_64BIT);
      }

      assert(reader_ != NULL);
      parameters_.addChunk(reader_, data, size);
    }    
    
    virtual void Execute(HttpOutput& output) ORTHANC_OVERRIDE
    {
      assert(reader_ != NULL);

      PImpl::PluginHttpOutput pluginOutput(output);

      OrthancPluginErrorCode error = parameters_.execute(
        reader_, reinterpret_cast<OrthancPluginRestOutput*>(&pluginOutput));

      pluginOutput.Close(error, errorDictionary_);
    }
  };


  bool OrthancPlugins::CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                                  RequestOrigin origin,
                                                  const char* remoteIp,
                                                  const char* username,
                                                  HttpMethod method,
                                                  const UriComponents& uri,
                                                  const HttpToolbox::Arguments& headers)
  {
    if (method != HttpMethod_Post &&
        method != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    RestCallbackMatcher matcher(uri);

    PImpl::ChunkedRestCallback* callback = NULL;

    // Loop over the callbacks registered by the plugins
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->restCallbackRegistrationMutex_);
    for (PImpl::ChunkedRestCallbacks::const_iterator it = pimpl_->chunkedRestCallbacks_.begin(); 
         it != pimpl_->chunkedRestCallbacks_.end(); ++it)
    {
      if (matcher.IsMatch((*it)->GetRegularExpression()))
      {
        callback = *it;
        break;
      }
    }

    if (callback == NULL)
    {
      // Callback not found
      return false;
    }
    else
    {
      OrthancPluginServerChunkedRequestReaderFactory handler;

      switch (method)
      {
        case HttpMethod_Post:
          handler = callback->GetParameters().postHandler;
          break;

        case HttpMethod_Put:
          handler = callback->GetParameters().putHandler;
          break;

        default:
          handler = NULL;
          break;
      }

      if (handler == NULL)
      {
        return false;
      }
      else
      {
        CLOG(INFO, PLUGINS) << "Delegating chunked HTTP request to plugin for URI: " << matcher.GetFlatUri();

        HttpRequestConverter converter(matcher, method, headers);
        converter.GetRequest().body = NULL;
        converter.GetRequest().bodySize = 0;

        OrthancPluginServerChunkedRequestReader* reader = NULL;
    
        OrthancPluginErrorCode errorCode = handler(
          &reader, matcher.GetFlatUri().c_str(), &converter.GetRequest());
    
        if (errorCode != OrthancPluginErrorCode_Success)
        {
          throw OrthancException(static_cast<ErrorCode>(errorCode));
        }
        else if (reader == NULL)
        {
          // The plugin has not created a reader for chunked body
          return false;
        }
        else
        {
          target.reset(new HttpServerChunkedReader(reader, callback->GetParameters(), GetErrorDictionary()));
          return true;
        }
      }
    }
  }


  IStorageCommitmentFactory::ILookupHandler* OrthancPlugins::CreateStorageCommitment(
    const std::string& jobId,
    const std::string& transactionUid,
    const std::vector<std::string>& sopClassUids,
    const std::vector<std::string>& sopInstanceUids,
    const std::string& remoteAet,
    const std::string& calledAet)
  {
    boost::mutex::scoped_lock lock(pimpl_->storageCommitmentScpMutex_);

    for (PImpl::StorageCommitmentScpCallbacks::iterator
           it = pimpl_->storageCommitmentScpCallbacks_.begin(); 
         it != pimpl_->storageCommitmentScpCallbacks_.end(); ++it)
    {
      assert(*it != NULL);
      IStorageCommitmentFactory::ILookupHandler* handler = (*it)->CreateStorageCommitment
        (jobId, transactionUid, sopClassUids, sopInstanceUids, remoteAet, calledAet);

      if (handler != NULL)
      {
        return handler;
      }
    } 
    
    return NULL;
  }


  bool OrthancPlugins::TranscodeBuffer(std::string& target,
                                       const void* buffer,
                                       size_t size,
                                       const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                       bool allowNewSopInstanceUid)
  {
    boost::shared_lock<boost::shared_mutex> lock(pimpl_->decoderTranscoderMutex_);

    if (pimpl_->transcoderCallbacks_.empty())
    {
      return false;
    }

    std::vector<const char*> uids;
    uids.reserve(allowedSyntaxes.size());
    for (std::set<DicomTransferSyntax>::const_iterator
           it = allowedSyntaxes.begin(); it != allowedSyntaxes.end(); ++it)
    {
      uids.push_back(GetTransferSyntaxUid(*it));
    }
    
    for (PImpl::TranscoderCallbacks::const_iterator
           transcoder = pimpl_->transcoderCallbacks_.begin();
         transcoder != pimpl_->transcoderCallbacks_.end(); ++transcoder)
    {
      MemoryBufferRaii a;

      if ((*transcoder) (a.GetObject(), buffer, size, uids.empty() ? NULL : &uids[0],
                         static_cast<uint32_t>(uids.size()), allowNewSopInstanceUid) ==
          OrthancPluginErrorCode_Success)
      {
        a.ToString(target);
        return true;
      }
    }

    return false;
  }


  bool OrthancPlugins::IsValidAuthorizationToken(const std::string& token) const
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    return (pimpl_->authorizationTokens_.find(token) != pimpl_->authorizationTokens_.end());
  }

  
  unsigned int OrthancPlugins::GetMaxDatabaseRetries() const
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);
    return pimpl_->maxDatabaseRetries_;
  }


  void OrthancPlugins::RegisterWebDavCollections(HttpServer& target)
  {
    boost::recursive_mutex::scoped_lock lock(pimpl_->invokeServiceMutex_);

    while (!pimpl_->webDavCollections_.empty())
    {
      WebDavCollection* collection = pimpl_->webDavCollections_.front();
      assert(collection != NULL);

      UriComponents components;
      Toolbox::SplitUriComponents(components, collection->GetUri());
      target.Register(components, collection);
      
      pimpl_->webDavCollections_.pop_front();
    }
  }
}
