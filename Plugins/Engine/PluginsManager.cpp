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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "PluginsManager.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif

#include "../../Core/HttpServer/HttpOutput.h"
#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"

#include <cassert>
#include <memory>
#include <boost/filesystem.hpp>

#ifdef WIN32
#define PLUGIN_EXTENSION ".dll"
#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#define PLUGIN_EXTENSION ".so"
#elif defined(__APPLE__) && defined(__MACH__)
#define PLUGIN_EXTENSION ".dylib"
#else
#error Support your platform here
#endif


namespace Orthanc
{
  PluginsManager::Plugin::Plugin(PluginsManager& pluginManager,
                                 const std::string& path) : 
    library_(path),
    pluginManager_(pluginManager)
  {
    memset(&context_, 0, sizeof(context_));
    context_.pluginsManager = this;
    context_.orthancVersion = ORTHANC_VERSION;
    context_.Free = ::free;
    context_.InvokeService = InvokeService;
  }


  static void CallInitialize(SharedLibrary& plugin,
                             const OrthancPluginContext& context)
  {
    typedef int32_t (*Initialize) (const OrthancPluginContext*);

#if defined(_WIN32)
    Initialize initialize = (Initialize) plugin.GetFunction("OrthancPluginInitialize");
#else
    /**
     * gcc would complain about "ISO C++ forbids casting between
     * pointer-to-function and pointer-to-object" without the trick
     * below, that is known as "the POSIX.1-2003 (Technical Corrigendum
     * 1) workaround". See the man page of "dlsym()".
     * http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
     * http://stackoverflow.com/a/14543811/881731
     **/

    Initialize initialize;
    *(void **) (&initialize) = plugin.GetFunction("OrthancPluginInitialize");
#endif

    assert(initialize != NULL);
    int32_t error = initialize(&context);

    if (error != 0)
    {
      LOG(ERROR) << "Error while initializing plugin " << plugin.GetPath()
                 << " (code " << error << ")";
      throw OrthancException(ErrorCode_SharedLibrary);
    }
  }


  static void CallFinalize(SharedLibrary& plugin)
  {
    typedef void (*Finalize) ();

#if defined(_WIN32)
    Finalize finalize = (Finalize) plugin.GetFunction("OrthancPluginFinalize");
#else
    Finalize finalize;
    *(void **) (&finalize) = plugin.GetFunction("OrthancPluginFinalize");
#endif

    assert(finalize != NULL);
    finalize();
  }


  static const char* CallGetName(SharedLibrary& plugin)
  {
    typedef const char* (*GetName) ();

#if defined(_WIN32)
    GetName getName = (GetName) plugin.GetFunction("OrthancPluginGetName");
#else
    GetName getName;
    *(void **) (&getName) = plugin.GetFunction("OrthancPluginGetName");
#endif

    assert(getName != NULL);
    return getName();
  }


  static const char* CallGetVersion(SharedLibrary& plugin)
  {
    typedef const char* (*GetVersion) ();

#if defined(_WIN32)
    GetVersion getVersion = (GetVersion) plugin.GetFunction("OrthancPluginGetVersion");
#else
    GetVersion getVersion;
    *(void **) (&getVersion) = plugin.GetFunction("OrthancPluginGetVersion");
#endif

    assert(getVersion != NULL);
    return getVersion();
  }


  OrthancPluginErrorCode PluginsManager::InvokeService(OrthancPluginContext* context,
                                                       _OrthancPluginService service, 
                                                       const void* params)
  {
    switch (service)
    {
      case _OrthancPluginService_LogError:
        LOG(ERROR) << reinterpret_cast<const char*>(params);
        return OrthancPluginErrorCode_Success;

      case _OrthancPluginService_LogWarning:
        LOG(WARNING) << reinterpret_cast<const char*>(params);
        return OrthancPluginErrorCode_Success;

      case _OrthancPluginService_LogInfo:
        LOG(INFO) << reinterpret_cast<const char*>(params);
        return OrthancPluginErrorCode_Success;

      default:
        break;
    }

    Plugin* that = reinterpret_cast<Plugin*>(context->pluginsManager);

    for (std::list<IPluginServiceProvider*>::iterator
           it = that->GetPluginManager().serviceProviders_.begin(); 
         it != that->GetPluginManager().serviceProviders_.end(); ++it)
    {
      try
      {
        if ((*it)->InvokeService(that->GetSharedLibrary(), service, params))
        {
          return OrthancPluginErrorCode_Success;
        }
      }
      catch (OrthancException& e)
      {
        // This service provider has failed
        if (e.GetErrorCode() != ErrorCode_UnknownResource)  // This error code is valid in plugins
        {
          LOG(ERROR) << "Exception while invoking plugin service " << service << ": " << e.What();
        }

        return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
      }
    }

    LOG(ERROR) << "Plugin invoking unknown service: " << service;
    return OrthancPluginErrorCode_UnknownPluginService;
  }


  PluginsManager::PluginsManager()
  {
  }

  PluginsManager::~PluginsManager()
  {
    for (Plugins::iterator it = plugins_.begin(); it != plugins_.end(); ++it)
    {
      if (it->second != NULL)
      {
        LOG(WARNING) << "Unregistering plugin '" << it->first
                     << "' (version " << it->second->GetVersion() << ")";

        CallFinalize(it->second->GetSharedLibrary());
        delete it->second;
      }
    }
  }


  static bool IsOrthancPlugin(SharedLibrary& library)
  {
    return (library.HasFunction("OrthancPluginInitialize") &&
            library.HasFunction("OrthancPluginFinalize") &&
            library.HasFunction("OrthancPluginGetName") &&
            library.HasFunction("OrthancPluginGetVersion"));
  }

  
  void PluginsManager::RegisterPlugin(const std::string& path)
  {
    if (!boost::filesystem::exists(path))
    {
      LOG(ERROR) << "Inexistent path to plugins: " << path;
      return;
    }

    if (boost::filesystem::is_directory(path))
    {
      ScanFolderForPlugins(path, false);
      return;
    }

    std::auto_ptr<Plugin> plugin(new Plugin(*this, path));

    if (!IsOrthancPlugin(plugin->GetSharedLibrary()))
    {
      LOG(ERROR) << "Plugin " << plugin->GetSharedLibrary().GetPath()
                 << " does not declare the proper entry functions";
      throw OrthancException(ErrorCode_SharedLibrary);
    }

    std::string name(CallGetName(plugin->GetSharedLibrary()));
    if (plugins_.find(name) != plugins_.end())
    {
      LOG(ERROR) << "Plugin '" << name << "' already registered";
      throw OrthancException(ErrorCode_SharedLibrary);
    }

    plugin->SetVersion(CallGetVersion(plugin->GetSharedLibrary()));
    LOG(WARNING) << "Registering plugin '" << name
                 << "' (version " << plugin->GetVersion() << ")";

    CallInitialize(plugin->GetSharedLibrary(), plugin->GetContext());

    plugins_[name] = plugin.release();
  }


  void PluginsManager::ScanFolderForPlugins(const std::string& folder,
                                            bool isRecursive)
  {
    using namespace boost::filesystem;

    if (!exists(folder))
    {
      return;
    }

    LOG(INFO) << "Scanning folder " << folder << " for plugins";

    directory_iterator end_it; // default construction yields past-the-end
    for (directory_iterator it(folder);
          it != end_it;
          ++it)
    {
      std::string path = it->path().string();

      if (is_directory(it->status()))
      {
        if (isRecursive)
        {
          ScanFolderForPlugins(path, true);
        }
      }
      else
      {
        std::string extension = boost::filesystem::extension(it->path());
        Toolbox::ToLowerCase(extension);

        if (extension == PLUGIN_EXTENSION)
        {
          LOG(INFO) << "Found a shared library: " << it->path();

          SharedLibrary plugin(path);
          if (IsOrthancPlugin(plugin))
          {
            RegisterPlugin(path);
          }
        }
      }
    }
  }


  void PluginsManager::ListPlugins(std::list<std::string>& result) const
  {
    result.clear();

    for (Plugins::const_iterator it = plugins_.begin(); 
         it != plugins_.end(); ++it)
    {
      result.push_back(it->first);
    }
  }


  bool PluginsManager::HasPlugin(const std::string& name) const
  {
    return plugins_.find(name) != plugins_.end();
  }


  const std::string& PluginsManager::GetPluginVersion(const std::string& name) const
  {
    Plugins::const_iterator it = plugins_.find(name);
    if (it == plugins_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return it->second->GetVersion();
    }
  }

  
  std::string PluginsManager::GetPluginName(SharedLibrary& library)
  {
    return CallGetName(library);
  }
}
