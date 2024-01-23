/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
