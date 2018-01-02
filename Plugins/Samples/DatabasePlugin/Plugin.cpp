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


#include "Database.h"

#include <memory>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>

static OrthancPluginContext*  context_ = NULL;
static std::auto_ptr<OrthancPlugins::IDatabaseBackend>  backend_;


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context_ = c;
    OrthancPluginLogWarning(context_, "Sample plugin is initializing");

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[256];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    std::string path = "SampleDatabase.sqlite";
    uint32_t argCount = OrthancPluginGetCommandLineArgumentsCount(context_);
    for (uint32_t i = 0; i < argCount; i++)
    {
      char* tmp = OrthancPluginGetCommandLineArgument(context_, i);
      std::string argument(tmp);
      OrthancPluginFreeString(context_, tmp);

      if (boost::starts_with(argument, "--database="))
      {
        path = argument.substr(11);
      }
    }

    std::string s = "Using the following SQLite database: " + path;
    OrthancPluginLogWarning(context_, s.c_str());

    backend_.reset(new Database(path));
    OrthancPlugins::DatabaseBackendAdapter::Register(context_, *backend_);

    return 0;
  }

  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    backend_.reset(NULL);
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "sample-database";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "1.0";
  }
}
