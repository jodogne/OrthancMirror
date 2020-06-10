#include <Core/HttpClient.h>
#include <Core/Logging.h>
#include <Core/OrthancException.h>
#include <Core/SystemToolbox.h>

#include <iostream>
#include <boost/thread.hpp>

static void Worker(bool *done)
{
  LOG(WARNING) << "One thread has started";

  Orthanc::HttpClient client;
  //client.SetUrl("http://localhost:8042/studies");
  //client.SetUrl("http://localhost:8042/tools/default-encoding");
  client.SetUrl("http://localhost:8042/system");
  //client.SetUrl("http://localhost:8042/");
  //client.SetCredentials("orthanc", "orthanc");
  client.SetRedirectionFollowed(false);
  
  while (!(*done))
  {
    try
    {
#if 0
      Json::Value v;
      if (!client.Apply(v) ||
          v.type() != Json::objectValue)
      {
        printf("ERROR\n");
      }
#else
      std::string s;
      if (!client.Apply(s) ||
          s.empty())
      {
        printf("ERROR\n");
      }
#endif
    }
    catch (Orthanc::OrthancException& e)
    {
      printf("EXCEPTION: %s", e.What());
    }
  }

  LOG(WARNING) << "One thread has stopped";
}

int main()
{
  Orthanc::Logging::Initialize();
  //Orthanc::Logging::EnableInfoLevel(true);
  Orthanc::HttpClient::GlobalInitialize();

  {
    bool done = false;

    std::vector<boost::thread*> threads;

    for (size_t i = 0; i < 100; i++)
    {
      threads.push_back(new boost::thread(Worker, &done));
    }

    LOG(WARNING) << "STARTED";
    Orthanc::SystemToolbox::ServerBarrier();
    LOG(WARNING) << "STOPPING";

    done = true;

    for (size_t i = 0; i < threads.size(); i++)
    {
      if (threads[i]->joinable())
      {
        threads[i]->join();
      }

      delete threads[i];
    }
  }
  
  Orthanc::HttpClient::GlobalFinalize();
  printf("OK\n");
  return 0;
}
