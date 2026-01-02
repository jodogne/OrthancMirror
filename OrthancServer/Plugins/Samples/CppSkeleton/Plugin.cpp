/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include <Logging.h>
#include <OrthancPluginCppWrapper.h>

#if ORTHANC_STANDALONE == 1
#  include <EmbeddedResources.h>
#else
#  include <SystemToolbox.h>
#endif


static void GetEmbeddedResource(std::string& target,
                                const std::string& path)
{
#if ORTHANC_STANDALONE == 0
  Orthanc::SystemToolbox::ReadFile(target, Orthanc::SystemToolbox::InterpretRelativePath(PLUGIN_RESOURCES_PATH, path));
#else
  const std::string s = "/" + path;
  Orthanc::EmbeddedResources::GetDirectoryResource(target, Orthanc::EmbeddedResources::PLUGIN_RESOURCES, s.c_str());
#endif
}


static bool DisplayPerformanceWarning()
{
  (void) DisplayPerformanceWarning;   // Disable warning about unused function
  LOG(WARNING) << "Performance warning in plugin: "
               << "Non-release build, runtime debug assertions are turned on";
  return true;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context, ORTHANC_PLUGIN_NAME);

#if ORTHANC_FRAMEWORK_VERSION_IS_ABOVE(1, 12, 4)
    Orthanc::Logging::InitializePluginContext(context, ORTHANC_PLUGIN_NAME);
#elif ORTHANC_FRAMEWORK_VERSION_IS_ABOVE(1, 7, 2)
    Orthanc::Logging::InitializePluginContext(context);
#else
    Orthanc::Logging::Initialize(context);
#endif

    assert(DisplayPerformanceWarning());

    Orthanc::Logging::EnableInfoLevel(true);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    OrthancPlugins::SetDescription(ORTHANC_PLUGIN_NAME, "Skeleton of a C++ plugin Orthanc.");

    try
    {
      std::string s;
      GetEmbeddedResource(s, "app.js");
      LOG(WARNING) << s;
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "Exception while initializing the plugin: " << e.What();
      return -1;
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    LOG(WARNING) << "Finalizing the skeleton plugin";
    Orthanc::Logging::Finalize();
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
