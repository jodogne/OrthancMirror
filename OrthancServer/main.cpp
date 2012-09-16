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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "OrthancRestApi.h"

#include <stdio.h>

#include "../Core/HttpServer/EmbeddedResourceHttpHandler.h"
#include "../Core/HttpServer/FilesystemHttpHandler.h"
#include "../Core/HttpServer/MongooseServer.h"
#include "DicomProtocol/DicomServer.h"
#include "OrthancInitialization.h"


using namespace Orthanc;


class MyDicomStore : public IStoreRequestHandler
{
private:
  ServerIndex& index_;
  FileStorage storage_;

public:
  MyDicomStore(ServerIndex& index,
               const std::string& path) :
    index_(index),
    storage_(path)
  {
  }

  virtual void Handle(const std::vector<uint8_t>& dicomFile,
                      const DicomMap& dicomSummary,
                      const Json::Value& dicomJson,
                      const std::string& remoteAet)
  {
    std::string instanceUuid;
    if (dicomFile.size() > 0)
    {
      index_.Store(instanceUuid, storage_, 
                   reinterpret_cast<const char*>(&dicomFile[0]), dicomFile.size(),
                   dicomSummary, dicomJson, remoteAet);
    }
  }
};


class MyDicomStoreFactory : public IStoreRequestHandlerFactory
{
private:
  ServerIndex& index_;
  std::string path_;

public:
  MyDicomStoreFactory(ServerIndex& index,
                      const std::string& path) :
    index_(index),
    path_(path)
  {
  }

  virtual IStoreRequestHandler* ConstructStoreRequestHandler()
  {
    return new MyDicomStore(index_, path_);
  }

  void Done()
  {
    //index_.db().Execute("DELETE FROM Studies");
  }
};







int main(int argc, char* argv[]) 
{
  try
  {
    if (argc >= 2)
    {
      OrthancInitialize(argv[1]);
    }
    else
    {
      OrthancInitialize();
    }

    std::string storageDirectory = GetGlobalStringParameter("StorageDirectory", "OrthancStorage");
    ServerIndex index(storageDirectory);
    MyDicomStoreFactory storeScp(index, storageDirectory);

    {
      // DICOM server
      DicomServer dicomServer;
      dicomServer.SetCalledApplicationEntityTitleCheck(GetGlobalBoolParameter("DicomCheckCalledAet", false));
      dicomServer.SetStoreRequestHandlerFactory(storeScp);
      dicomServer.SetPortNumber(GetGlobalIntegerParameter("DicomPort", 4242));
      dicomServer.SetApplicationEntityTitle(GetGlobalStringParameter("DicomAet", "ORTHANC"));

      // HTTP server
      MongooseServer httpServer;
      httpServer.SetPort(GetGlobalIntegerParameter("HttpPort", 8000));
      httpServer.SetRemoteAccessAllowed(GetGlobalBoolParameter("RemoteAccessAllowed", false));

      httpServer.SetAuthenticationEnabled(GetGlobalBoolParameter("AuthenticationEnabled", false));
      SetupRegisteredUsers(httpServer);

      if (GetGlobalBoolParameter("SslEnabled", false))
      {
        std::string certificate = GetGlobalStringParameter("SslCertificate", "certificate.pem");
        httpServer.SetSslEnabled(true);
        httpServer.SetSslCertificate(certificate.c_str());
      }
      else
      {
        httpServer.SetSslEnabled(false);
      }

#if ORTHANC_STANDALONE == 1
      httpServer.RegisterHandler(new EmbeddedResourceHttpHandler("/app", EmbeddedResources::ORTHANC_EXPLORER));
#else
      httpServer.RegisterHandler(new FilesystemHttpHandler("/app", ORTHANC_PATH "/OrthancExplorer"));
#endif

      httpServer.RegisterHandler(new OrthancRestApi(index, storageDirectory));

      // GO !!!
      httpServer.Start();
      dicomServer.Start();

      printf("The server has started\n");
      Toolbox::ServerBarrier();

      // Stop
      printf("Finishing\n");
    }

    storeScp.Done();
  }
  catch (OrthancException& e)
  {
    std::cout << "EXCEPT [" << e.What() << "]" << std::endl;
  }

  OrthancFinalize();

  return 0;
}
