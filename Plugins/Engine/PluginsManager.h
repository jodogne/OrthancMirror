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


#pragma once

#if ORTHANC_ENABLE_PLUGINS == 1

#include "IPluginServiceProvider.h"

#include <map>
#include <list>

namespace Orthanc
{
  class PluginsManager : public boost::noncopyable
  {
  private:
    class Plugin : public boost::noncopyable
    {
    private:
      OrthancPluginContext  context_;
      SharedLibrary         library_;
      std::string           version_;
      PluginsManager&       pluginManager_;

    public:
      Plugin(PluginsManager& pluginManager,
             const std::string& path);

      SharedLibrary& GetSharedLibrary()
      {
        return library_;
      }

      void SetVersion(const std::string& version)
      {
        version_ = version;
      }

      const std::string& GetVersion() const
      {
        return version_;
      }

      PluginsManager& GetPluginManager()
      {
        return pluginManager_;
      }

      OrthancPluginContext& GetContext()
      {
        return context_;
      }
    };

    typedef std::map<std::string, Plugin*>  Plugins;

    Plugins  plugins_;
    std::list<IPluginServiceProvider*> serviceProviders_;

    static OrthancPluginErrorCode InvokeService(OrthancPluginContext* context,
                                                _OrthancPluginService service,
                                                const void* parameters);

  public:
    PluginsManager();

    ~PluginsManager();

    void RegisterPlugin(const std::string& path);

    void ScanFolderForPlugins(const std::string& path,
                              bool isRecursive);

    void RegisterServiceProvider(IPluginServiceProvider& provider)
    {
      serviceProviders_.push_back(&provider);
    }

    void ListPlugins(std::list<std::string>& result) const;

    bool HasPlugin(const std::string& name) const;

    const std::string& GetPluginVersion(const std::string& name) const;

    static std::string GetPluginName(SharedLibrary& library);
  };
}

#endif
