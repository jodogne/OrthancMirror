/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include <stdio.h>

#include "../../../Sources/HttpServer/HttpServer.h"
#include "../../../Sources/Logging.h"
#include "../../../Sources/RestApi/RestApi.h"
#include "../../../Sources/SystemToolbox.h"

class MicroService : public Orthanc::RestApi
{
private:
  static MicroService& GetSelf(Orthanc::RestApiCall& call)
  {
    return dynamic_cast<MicroService&>(call.GetContext());
  }

  void SayHello()
  {
    printf("Hello\n");
  }

  static void Hello(Orthanc::RestApiGetCall& call)
  {
    GetSelf(call).SayHello();
    
    Json::Value value = Json::arrayValue;
    value.append("World");
    
    call.GetOutput().AnswerJson(value);
  }

public:
  MicroService()
  {
    Register("/hello", Hello);
  }  
};

int main()
{
  Orthanc::Logging::Initialize();
  Orthanc::Logging::EnableTraceLevel(true);

  MicroService rest;
  
  {
    Orthanc::HttpServer httpServer;
    httpServer.SetPortNumber(8000);
    httpServer.Register(rest);
    httpServer.SetRemoteAccessAllowed(true);
    httpServer.Start();
    
    LOG(WARNING) << "Micro-service started on port " << httpServer.GetPortNumber();
    Orthanc::SystemToolbox::ServerBarrier();
  }

  LOG(WARNING) << "Micro-service stopped";

  Orthanc::Logging::Finalize();
  
  return 0;
}
