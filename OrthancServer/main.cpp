/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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


#include "OrthancRestApi.h"

#include <fstream>
#include <glog/logging.h>
#include <boost/algorithm/string/predicate.hpp>

#include "../Core/HttpServer/EmbeddedResourceHttpHandler.h"
#include "../Core/HttpServer/FilesystemHttpHandler.h"
#include "../Core/Lua/LuaFunctionCall.h"
#include "DicomProtocol/DicomServer.h"
#include "OrthancInitialization.h"
#include "ServerContext.h"

using namespace Orthanc;



class MyDicomStore : public IStoreRequestHandler
{
private:
  ServerContext& server_;

public:
  MyDicomStore(ServerContext& context) :
    server_(context)
  {
  }

  virtual void Handle(const std::string& dicomFile,
                      const DicomMap& dicomSummary,
                      const Json::Value& dicomJson,
                      const std::string& remoteAet)
  {
    if (dicomFile.size() > 0)
    {
      LuaContext& lua = server_.GetLuaContext();
      // TODO : Is existing trigger ?
      LuaFunctionCall call(lua, "NewInstanceFilter");
      call.PushJSON(dicomJson);
      call.PushString(remoteAet);

      if (call.ExecutePredicate())
      {
        server_.Store(&dicomFile[0], dicomFile.size(), dicomSummary, dicomJson, remoteAet);
      }
      else
      {
        LOG(WARNING) << "An instance has been discarded by a filter";
      }
    }
  }
};


class MyDicomStoreFactory : public IStoreRequestHandlerFactory
{
private:
  ServerContext& server_;

public:
  MyDicomStoreFactory(ServerContext& context) : server_(context)
  {
  }

  virtual IStoreRequestHandler* ConstructStoreRequestHandler()
  {
    return new MyDicomStore(server_);
  }

  void Done()
  {
    //index_.db().Execute("DELETE FROM Studies");
  }
};


void PrintHelp(char* path)
{
  std::cout 
    << "Usage: " << path << " [OPTION]... [CONFIGURATION]" << std::endl
    << "Orthanc, lightweight, RESTful DICOM server for healthcare and medical research." << std::endl
    << std::endl
    << "If no configuration file is given on the command line, a set of default " << std::endl
    << "parameters is used. Please refer to the Orthanc homepage for the full " << std::endl
    << "instructions about how to use Orthanc " << std::endl
    << "<https://code.google.com/p/orthanc/wiki/OrthancCookbook>." << std::endl
    << std::endl
    << "Command-line options:" << std::endl
    << "  --help\t\tdisplay this help and exit" << std::endl
    << "  --logdir=[dir]\tdirectory where to store the log files" << std::endl
    << "\t\t\t(if not used, the logs are dumped to stderr)" << std::endl
    << "  --config=[file]\tcreate a sample configuration file and exit" << std::endl
    << "  --trace\t\thighest verbosity in logs (for debug)" << std::endl
    << "  --verbose\t\tbe verbose in logs" << std::endl
    << "  --version\t\toutput version information and exit" << std::endl
    << std::endl
    << "Exit status:" << std::endl
    << " 0  if OK," << std::endl
    << " -1  if error (have a look at the logs)." << std::endl
    << std::endl;
}


void PrintVersion(char* path)
{
  std::cout
    << path << " " << ORTHANC_VERSION << std::endl
    << "Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege (Belgium) " << std::endl
    << "Licensing GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>, with OpenSSL exception." << std::endl
    << "This is free software: you are free to change and redistribute it." << std::endl
    << "There is NO WARRANTY, to the extent permitted by law." << std::endl
    << std::endl
    << "Written by Sebastien Jodogne <s.jodogne@gmail.com>" << std::endl;
}


int main(int argc, char* argv[]) 
{
  // Initialize Google's logging library.
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 1;
  FLAGS_v = 0;

  for (int i = 1; i < argc; i++)
  {
    if (std::string(argv[i]) == "--help")
    {
      PrintHelp(argv[0]);
      return 0;
    }

    if (std::string(argv[i]) == "--version")
    {
      PrintVersion(argv[0]);
      return 0;
    }

    if (std::string(argv[i]) == "--verbose")
    {
      FLAGS_minloglevel = 0;
    }

    if (std::string(argv[i]) == "--trace")
    {
      FLAGS_minloglevel = 0;
      FLAGS_v = 1;
    }

    if (boost::starts_with(argv[i], "--logdir="))
    {
      FLAGS_logtostderr = false;
      FLAGS_log_dir = std::string(argv[i]).substr(9);
    }

    if (boost::starts_with(argv[i], "--config="))
    {
      std::string configurationSample;
      GetFileResource(configurationSample, EmbeddedResources::CONFIGURATION_SAMPLE);

      std::string target = std::string(argv[i]).substr(9);
      std::ofstream f(target.c_str());
      f << configurationSample;
      f.close();
      return 0;
    }
  }

  google::InitGoogleLogging("Orthanc");

  int status = 0;
  try
  {
    bool isInitialized = false;
    if (argc >= 2)
    {
      for (int i = 1; i < argc; i++)
      {
        // Use the first argument that does not start with a "-" as
        // the configuration file
        if (argv[i][0] != '-')
        {
          OrthancInitialize(argv[i]);
          isInitialized = true;
        }
      }
    }

    if (!isInitialized)
    {
      OrthancInitialize();
    }

    boost::filesystem::path storageDirectory = 
      InterpretStringParameterAsPath(GetGlobalStringParameter("StorageDirectory", "OrthancStorage"));
    boost::filesystem::path indexDirectory = 
      InterpretStringParameterAsPath(GetGlobalStringParameter("IndexDirectory", storageDirectory.string()));
    ServerContext context(storageDirectory, indexDirectory);

    LOG(WARNING) << "Storage directory: " << storageDirectory;
    LOG(WARNING) << "Index directory: " << indexDirectory;

    context.SetCompressionEnabled(GetGlobalBoolParameter("StorageCompression", false));

    std::list<std::string> luaScripts;
    GetGlobalListOfStringsParameter(luaScripts, "LuaScripts");
    for (std::list<std::string>::const_iterator
           it = luaScripts.begin(); it != luaScripts.end(); it++)
    {
      std::string path = InterpretStringParameterAsPath(*it);
      LOG(WARNING) << "Installing the Lua scripts from: " << path;
      std::string script;
      Toolbox::ReadFile(script, path);
      context.GetLuaContext().Execute(script);
    }


    try
    {
      context.GetIndex().SetMaximumPatientCount(GetGlobalIntegerParameter("MaximumPatientCount", 0));
    }
    catch (...)
    {
      context.GetIndex().SetMaximumPatientCount(0);
    }

    try
    {
      uint64_t size = GetGlobalIntegerParameter("MaximumStorageSize", 0);
      context.GetIndex().SetMaximumStorageSize(size * 1024 * 1024);
    }
    catch (...)
    {
      context.GetIndex().SetMaximumStorageSize(0);
    }

    MyDicomStoreFactory storeScp(context);

    {
      // DICOM server
      DicomServer dicomServer;
      dicomServer.SetCalledApplicationEntityTitleCheck(GetGlobalBoolParameter("DicomCheckCalledAet", false));
      dicomServer.SetStoreRequestHandlerFactory(storeScp);
      dicomServer.SetPortNumber(GetGlobalIntegerParameter("DicomPort", 4242));
      dicomServer.SetApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));

      // HTTP server
      MongooseServer httpServer;
      httpServer.SetPortNumber(GetGlobalIntegerParameter("HttpPort", 8042));
      httpServer.SetRemoteAccessAllowed(GetGlobalBoolParameter("RemoteAccessAllowed", false));

      httpServer.SetAuthenticationEnabled(GetGlobalBoolParameter("AuthenticationEnabled", false));
      SetupRegisteredUsers(httpServer);

      if (GetGlobalBoolParameter("SslEnabled", false))
      {
        std::string certificate = 
          InterpretStringParameterAsPath(GetGlobalStringParameter("SslCertificate", "certificate.pem"));
        httpServer.SetSslEnabled(true);
        httpServer.SetSslCertificate(certificate.c_str());
      }
      else
      {
        httpServer.SetSslEnabled(false);
      }

      LOG(WARNING) << "DICOM server listening on port: " << dicomServer.GetPortNumber();
      LOG(WARNING) << "HTTP server listening on port: " << httpServer.GetPortNumber();

#if ORTHANC_STANDALONE == 1
      httpServer.RegisterHandler(new EmbeddedResourceHttpHandler("/app", EmbeddedResources::ORTHANC_EXPLORER));
#else
      httpServer.RegisterHandler(new FilesystemHttpHandler("/app", ORTHANC_PATH "/OrthancExplorer"));
#endif

      httpServer.RegisterHandler(new OrthancRestApi(context));

      // GO !!!
      httpServer.Start();
      dicomServer.Start();

      LOG(WARNING) << "Orthanc has started";
      Toolbox::ServerBarrier();

      // Stop
      LOG(WARNING) << "Orthanc is stopping";
    }

    storeScp.Done();
  }
  catch (OrthancException& e)
  {
    LOG(ERROR) << "EXCEPTION [" << e.What() << "]";
    status = -1;
  }
  catch (...)
  {
    LOG(ERROR) << "NATIVE EXCEPTION";
    status = -1;
  }

  OrthancFinalize();

  return status;
}
