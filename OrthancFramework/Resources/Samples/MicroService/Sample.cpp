#include <stdio.h>

#include <HttpServer/MongooseServer.h>
#include <Logging.h>
#include <RestApi/RestApi.h>
#include <SystemToolbox.h>

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
    Orthanc::MongooseServer httpServer;
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
