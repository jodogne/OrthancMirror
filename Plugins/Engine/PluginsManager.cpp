/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "PluginsManager.h"

#include <glog/logging.h>
#include <cassert>
#include <memory>
#include <boost/filesystem.hpp>

#ifdef WIN32
#define PLUGIN_EXTENSION ".dll"
#elif defined(__linux)
#define PLUGIN_EXTENSION ".so"
#else
#error Support your platform here
#endif


namespace Orthanc
{
  static void CallInitialize(SharedLibrary& plugin,
                             const OrthancPluginContext& context)
  {
    typedef int32_t (*Initialize) (const OrthancPluginContext*);

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

    Finalize finalize;
    *(void **) (&finalize) = plugin.GetFunction("OrthancPluginFinalize");
    assert(finalize != NULL);

    finalize();
  }


  static const char* CallGetName(SharedLibrary& plugin)
  {
    typedef const char* (*GetName) ();

    GetName getName;
    *(void **) (&getName) = plugin.GetFunction("OrthancPluginGetName");
    assert(getName != NULL);

    return getName();
  }


  static const char* CallGetVersion(SharedLibrary& plugin)
  {
    typedef const char* (*GetVersion) ();

    GetVersion getVersion;
    *(void **) (&getVersion) = plugin.GetFunction("OrthancPluginGetVersion");
    assert(getVersion != NULL);

    return getVersion();
  }


  static void LogError(const char* str)
  {
    LOG(ERROR) << str;
  }

  static void LogWarning(const char* str)
  {
    LOG(WARNING) << str;
  }

  static void LogInfo(const char* str)
  {
    LOG(INFO) << str;
  }

  static int32_t InvokeService(const char* serviceName,
                               const void* serviceParameters)
  {
    // TODO
    return 0;
  }

  PluginsManager::PluginsManager()
  {
    context_.orthancVersion = ORTHANC_VERSION;
    context_.InvokeService = InvokeService;
    context_.LogError = LogError;
    context_.LogWarning = LogWarning;
    context_.LogInfo = LogInfo;
  }

  PluginsManager::~PluginsManager()
  {
    for (Plugins::iterator it = plugins_.begin(); it != plugins_.end(); it++)
    {
      if (it->second != NULL)
      {
        CallFinalize(*(it->second));
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
    std::auto_ptr<SharedLibrary> plugin(new SharedLibrary(path));

    if (!IsOrthancPlugin(*plugin))
    {
      LOG(ERROR) << "Plugin " << plugin->GetPath()
                 << " does not declare the proper entry functions";
      throw OrthancException(ErrorCode_SharedLibrary);
    }

    std::string name(CallGetName(*plugin));
    if (plugins_.find(name) != plugins_.end())
    {
      LOG(ERROR) << "Plugin '" << name << "' already registered";
      throw OrthancException(ErrorCode_SharedLibrary);
    }

    LOG(WARNING) << "Registering plugin '" << name
                 << "' (version " << CallGetVersion(*plugin) << ")";

    CallInitialize(*plugin, context_);

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
        if (boost::filesystem::extension(it->path()) == PLUGIN_EXTENSION)
        {
          LOG(INFO) << "Found a shared library: " << it->path();

          try
          {
            SharedLibrary plugin(path);
            if (IsOrthancPlugin(plugin))
            {
              RegisterPlugin(path);
            }
          }
          catch (OrthancException&)
          {
          }
        }
      }
    }
  }

}
