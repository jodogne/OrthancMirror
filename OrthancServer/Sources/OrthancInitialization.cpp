/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include "PrecompiledHeadersServer.h"

#if defined(_WIN32)
// "Please include winsock2.h before windows.h"
#  include <winsock2.h>
#endif

#if !defined(HAVE_MALLOPT)
#  error Macro HAVE_MALLOPT must be defined
#endif

#if HAVE_MALLOPT == 1
#  include <malloc.h>
#endif

#include "OrthancInitialization.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/FileStorage/FilesystemStorage.h"
#include "../../OrthancFramework/Sources/HttpClient.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"

#include "Database/SQLiteDatabaseWrapper.h"
#include "OrthancConfiguration.h"

#include <OrthancServerResources.h>

#include <dcmtk/dcmnet/dul.h>     // For dcmDisableGethostbyaddr()
#include <dcmtk/dcmnet/diutil.h>  // For DCM_dcmnetLogger



namespace Orthanc
{
  static void RegisterUserMetadata(const Json::Value& config)
  {
    if (config.isMember("UserMetadata"))
    {
      const Json::Value& parameter = config["UserMetadata"];

      Json::Value::Members members = parameter.getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        const std::string& name = members[i];

        if (!parameter[name].isInt())
        {
          throw OrthancException(ErrorCode_BadParameterType,
                                 "Not a number in this user-defined metadata: " + name);
        }

        int metadata = parameter[name].asInt();        

        LOG(INFO) << "Registering user-defined metadata: " << name << " (index " 
                  << metadata << ")";

        try
        {
          RegisterUserMetadata(metadata, name);
        }
        catch (OrthancException&)
        {
          LOG(ERROR) << "Cannot register this user-defined metadata: " << name;
          throw;
        }
      }
    }
  }


  static void RegisterUserContentType(const Json::Value& config)
  {
    if (config.isMember("UserContentType"))
    {
      const Json::Value& parameter = config["UserContentType"];

      Json::Value::Members members = parameter.getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        const std::string& name = members[i];
        std::string mime = MIME_BINARY;

        const Json::Value& value = parameter[name];
        int contentType;

        if (value.isArray() &&
            value.size() == 2 &&
            value[0].isInt() &&
            value[1].isString())
        {
          contentType = value[0].asInt();
          mime = value[1].asString();
        }
        else if (value.isInt())
        {
          contentType = value.asInt();
        }
        else
        {
          throw OrthancException(ErrorCode_BadParameterType,
                                 "Not a number in this user-defined attachment type: " + name);
        }

        LOG(INFO) << "Registering user-defined attachment type: " << name << " (index " 
                  << contentType << ") with MIME type \"" << mime << "\"";

        try
        {
          RegisterUserContentType(contentType, name, mime);
        }
        catch (OrthancException&)
        {
          throw;
        }
      }
    }
  }


  static void LoadCustomDictionary(const Json::Value& configuration)
  {
    if (configuration.type() != Json::objectValue ||
        !configuration.isMember("Dictionary") ||
        configuration["Dictionary"].type() != Json::objectValue)
    {
      return;
    }

    Json::Value::Members tags(configuration["Dictionary"].getMemberNames());

    for (Json::Value::ArrayIndex i = 0; i < tags.size(); i++)
    {
      const Json::Value& content = configuration["Dictionary"][tags[i]];
      if (content.type() != Json::arrayValue ||
          content.size() < 2 ||
          content.size() > 5 ||
          content[0].type() != Json::stringValue ||
          content[1].type() != Json::stringValue ||
          (content.size() >= 3 && content[2].type() != Json::intValue) ||
          (content.size() >= 4 && content[3].type() != Json::intValue) ||
          (content.size() >= 5 && content[4].type() != Json::stringValue))
      {
        throw OrthancException(ErrorCode_BadFileFormat, "The definition of the '" + tags[i] + "' dictionary entry is invalid.");
      }

      DicomTag tag(FromDcmtkBridge::ParseTag(tags[i]));
      ValueRepresentation vr = StringToValueRepresentation(content[0].asString(), true);
      std::string name = content[1].asString();
      unsigned int minMultiplicity = (content.size() >= 2) ? content[2].asUInt() : 1;
      unsigned int maxMultiplicity = (content.size() >= 3) ? content[3].asUInt() : 1;
      std::string privateCreator = (content.size() >= 4) ? content[4].asString() : "";

      FromDcmtkBridge::RegisterDictionaryTag(tag, vr, name, minMultiplicity, maxMultiplicity, privateCreator);
    }
  }


  static void ConfigurePkcs11(const Json::Value& config)
  {
    if (config.type() != Json::objectValue ||
        !config.isMember("Module") ||
        config["Module"].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "No path to the PKCS#11 module (DLL or .so) is provided "
                             "for HTTPS client authentication");
    }

    std::string pin;
    if (config.isMember("Pin"))
    {
      if (config["Pin"].type() == Json::stringValue)
      {
        pin = config["Pin"].asString();
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "The PIN number in the PKCS#11 configuration must be a string");
      }
    }

    bool verbose = false;
    if (config.isMember("Verbose"))
    {
      if (config["Verbose"].type() == Json::booleanValue)
      {
        verbose = config["Verbose"].asBool();
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "The Verbose option in the PKCS#11 configuration must be a Boolean");
      }
    }

    HttpClient::InitializePkcs11(config["Module"].asString(), pin, verbose);
  }



  void OrthancInitialize(const char* configurationFile)
  {
    static const char* LOCALE = "Locale";
    static const char* PKCS11 = "Pkcs11";
    static const char* DEFAULT_ENCODING = "DefaultEncoding";
    static const char* MALLOC_ARENA_MAX = "MallocArenaMax";
    
    OrthancConfiguration::WriterLock lock;

    InitializeServerEnumerations();

    // Read the user-provided configuration
    lock.GetConfiguration().Read(configurationFile);

    {
      std::string locale;
      
      if (lock.GetJson().isMember(LOCALE))
      {
        locale = lock.GetConfiguration().GetStringParameter(LOCALE, "");
      }
      
      bool loadPrivate = lock.GetConfiguration().GetBooleanParameter("LoadPrivateDictionary", true);
      Orthanc::InitializeFramework(locale, loadPrivate);
    }

    // The Orthanc framework is now initialized

    if (lock.GetJson().isMember(DEFAULT_ENCODING))
    {
      std::string encoding = lock.GetConfiguration().GetStringParameter(DEFAULT_ENCODING, "");
      SetDefaultDicomEncoding(StringToEncoding(encoding.c_str()));
    }
    else
    {
      SetDefaultDicomEncoding(ORTHANC_DEFAULT_DICOM_ENCODING);
    }

    if (lock.GetJson().isMember(PKCS11))
    {
      ConfigurePkcs11(lock.GetJson()[PKCS11]);
    }

    RegisterUserMetadata(lock.GetJson());
    RegisterUserContentType(lock.GetJson());

    LoadCustomDictionary(lock.GetJson());

    lock.GetConfiguration().RegisterFont(ServerResources::FONT_UBUNTU_MONO_BOLD_16);

#if HAVE_MALLOPT == 1
    // New in Orthanc 1.8.2
    // https://book.orthanc-server.com/faq/scalability.html#controlling-memory-usage
    unsigned int maxArena = lock.GetConfiguration().GetUnsignedIntegerParameter(MALLOC_ARENA_MAX, 5);
    if (maxArena != 0)
    {
      // https://man7.org/linux/man-pages/man3/mallopt.3.html
      LOG(INFO) << "Calling mallopt(M_ARENA_MAX, " << maxArena << ")";
      if (mallopt(M_ARENA_MAX, maxArena) != 1 /* success */)
      {
        throw OrthancException(ErrorCode_InternalError, "The call to mallopt(M_ARENA_MAX, " +
                               boost::lexical_cast<std::string>(maxArena) + ") has failed");
      }
    }
#else
    if (lock.GetJson().isMember(MALLOC_ARENA_MAX))
    {
      LOG(INFO) << "Your platform does not support mallopt(), ignoring configuration option \""
                << MALLOC_ARENA_MAX << "\"";
    }
#endif
  }



  void OrthancFinalize()
  {
    OrthancConfiguration::WriterLock lock;
    Orthanc::FinalizeFramework();
  }


  static IDatabaseWrapper* CreateSQLiteWrapper()
  {
    OrthancConfiguration::ReaderLock lock;

    std::string storageDirectoryStr = 
      lock.GetConfiguration().GetStringParameter("StorageDirectory", "OrthancStorage");

    // Open the database
    boost::filesystem::path indexDirectory = lock.GetConfiguration().InterpretStringParameterAsPath(
      lock.GetConfiguration().GetStringParameter("IndexDirectory", storageDirectoryStr));

    LOG(WARNING) << "SQLite index directory: " << indexDirectory;

    try
    {
      boost::filesystem::create_directories(indexDirectory);
    }
    catch (boost::filesystem::filesystem_error&)
    {
    }

    return new SQLiteDatabaseWrapper(indexDirectory.string() + "/index");
  }


  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules

    class FilesystemStorageWithoutDicom : public IStorageArea
    {
    private:
      FilesystemStorage storage_;

    public:
      FilesystemStorageWithoutDicom(const std::string& path,
                                    bool fsyncOnWrite) :
        storage_(path, fsyncOnWrite)
      {
      }

      virtual void Create(const std::string& uuid,
                          const void* content, 
                          size_t size,
                          FileContentType type) ORTHANC_OVERRIDE
      {
        if (type != FileContentType_Dicom)
        {
          storage_.Create(uuid, content, size, type);
        }
      }

      virtual IMemoryBuffer* Read(const std::string& uuid,
                                  FileContentType type) ORTHANC_OVERRIDE
      {
        if (type != FileContentType_Dicom)
        {
          return storage_.Read(uuid, type);
        }
        else
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
      }

      virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                       FileContentType type,
                                       uint64_t start /* inclusive */,
                                       uint64_t end /* exclusive */) ORTHANC_OVERRIDE
      {
        if (type != FileContentType_Dicom)
        {
          return storage_.ReadRange(uuid, type, start, end);
        }
        else
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
      }

      virtual bool HasReadRange() const ORTHANC_OVERRIDE
      {
        return storage_.HasReadRange();
      }

      virtual void Remove(const std::string& uuid,
                          FileContentType type) ORTHANC_OVERRIDE
      {
        if (type != FileContentType_Dicom)
        {
          storage_.Remove(uuid, type);
        }
      }
    };
  }


  static IStorageArea* CreateFilesystemStorage()
  {
    OrthancConfiguration::ReaderLock lock;

    std::string storageDirectoryStr = 
      lock.GetConfiguration().GetStringParameter("StorageDirectory", "OrthancStorage");

    boost::filesystem::path storageDirectory = 
      lock.GetConfiguration().InterpretStringParameterAsPath(storageDirectoryStr);

    LOG(WARNING) << "Storage directory: " << storageDirectory;

    // New in Orthanc 1.7.4
    bool fsyncOnWrite = lock.GetConfiguration().GetBooleanParameter("SyncStorageArea", true);

    if (lock.GetConfiguration().GetBooleanParameter("StoreDicom", true))
    {
      return new FilesystemStorage(storageDirectory.string(), fsyncOnWrite);
    }
    else
    {
      LOG(WARNING) << "The DICOM files will not be stored, Orthanc running in index-only mode";
      return new FilesystemStorageWithoutDicom(storageDirectory.string(), fsyncOnWrite);
    }
  }


  IDatabaseWrapper* CreateDatabaseWrapper()
  {
    return CreateSQLiteWrapper();
  }


  IStorageArea* CreateStorageArea()
  {
    return CreateFilesystemStorage();
  }


  static void SetDcmtkVerbosity(Verbosity verbosity)
  {
    // INFO_LOG_LEVEL was the DCMTK log level in Orthanc <= 1.8.0    
    // https://support.dcmtk.org/docs-dcmrt/classOFLogger.html#ae20bf2616f15313c1f089da2eefb8245

    OFLogger::LogLevel dataLevel, networkLevel;

    switch (verbosity)
    {
      case Verbosity_Default:
        // Turn off logging in DCMTK core
        dataLevel = OFLogger::OFF_LOG_LEVEL;
        networkLevel = OFLogger::OFF_LOG_LEVEL;
        break;

      case Verbosity_Verbose:
        dataLevel = OFLogger::INFO_LOG_LEVEL;
        networkLevel = OFLogger::INFO_LOG_LEVEL;
        break;

      case Verbosity_Trace:
        dataLevel = OFLogger::INFO_LOG_LEVEL;  // DEBUG here makes DCMTK too verbose to be useful
        networkLevel = OFLogger::DEBUG_LOG_LEVEL;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    OFLog::configure(dataLevel);
    assert(dcmtk::log4cplus::Logger::getRoot().getChainedLogLevel() == dataLevel);
    
    DCM_dcmdataLogger.setLogLevel(dataLevel);    // This seems to be implied by "OFLog::configure()"
    DCM_dcmnetLogger.setLogLevel(networkLevel);  // This will display PDU in DICOM networking
  }


  void SetGlobalVerbosity(Verbosity verbosity)
  {
    SetDcmtkVerbosity(verbosity);
    
    switch (verbosity)
    {
      case Verbosity_Default:
        Logging::EnableInfoLevel(false);
        Logging::EnableTraceLevel(false);
        break;

      case Verbosity_Verbose:
        Logging::EnableInfoLevel(true);
        Logging::EnableTraceLevel(false);
        break;

      case Verbosity_Trace:
        Logging::EnableInfoLevel(true);
        Logging::EnableTraceLevel(true);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  
  Verbosity GetGlobalVerbosity()
  {
    if (Logging::IsTraceLevelEnabled())
    {
      return Verbosity_Trace;
    }
    else if (Logging::IsInfoLevelEnabled())
    {
      return Verbosity_Verbose;
    }
    else
    {
      return Verbosity_Default;
    }
  }

  
  void SetCategoryVerbosity(Logging::LogCategory category,
                            Verbosity verbosity)
  {
    switch (verbosity)
    {
      case Verbosity_Default:
        Logging::SetCategoryEnabled(Logging::LogLevel_INFO, category, false);
        Logging::SetCategoryEnabled(Logging::LogLevel_TRACE, category, false);
        break;

      case Verbosity_Verbose:
        Logging::SetCategoryEnabled(Logging::LogLevel_INFO, category, true);
        Logging::SetCategoryEnabled(Logging::LogLevel_TRACE, category, false);
        break;

      case Verbosity_Trace:
        Logging::SetCategoryEnabled(Logging::LogLevel_INFO, category, true);
        Logging::SetCategoryEnabled(Logging::LogLevel_TRACE, category, true);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (category == Logging::LogCategory_DICOM)
    {
      SetDcmtkVerbosity(verbosity);
    }
  }
  

  Verbosity GetCategoryVerbosity(Logging::LogCategory category)
  {
    if (Logging::IsCategoryEnabled(Logging::LogLevel_TRACE, category))
    {
      return Verbosity_Trace;
    }
    else if (Logging::IsCategoryEnabled(Logging::LogLevel_INFO, category))
    {
      return Verbosity_Verbose;
    }
    else
    {
      return Verbosity_Default;
    }
  }
}
