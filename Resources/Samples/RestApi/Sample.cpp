/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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


#include <Core/HttpServer/MongooseServer.h>
#include <Core/RestApi/RestApi.h>
#include <Core/Toolbox.h>
#include <glog/logging.h>
#include <stdio.h>


/**
 * This is a demo program that shows how to setup a REST server with
 * the Orthanc Core API. Once the server is running, here are some 
 * sample command lines to interact with it:
 * 
 *  # curl http://localhost:8042
 *  # curl 'http://localhost:8042?name=Hide'
 *  # curl http://localhost:8042 -X DELETE
 *  # curl http://localhost:8042 -X PUT -d "PutBody"
 *  # curl http://localhost:8042 -X POST -d "PostBody"
 **/

static void GetRoot(Orthanc::RestApi::GetCall& call)
{
  std::string answer = "Hello world\n";
  answer += "Glad to meet you, Mr. " + call.GetArgument("name", "Nobody") + "\n";
  call.GetOutput().AnswerBuffer(answer, "text/plain");
}
 
static void DeleteRoot(Orthanc::RestApi::DeleteCall& call)
{
  call.GetOutput().AnswerBuffer("Hey, you have just deleted the server!\n",
                                "text/plain");
}
 
static void PostRoot(Orthanc::RestApi::PostCall& call)
{
  call.GetOutput().AnswerBuffer("I have received a POST with body: [" +
                                call.GetPostBody() + "]\n", "text/plain");
}
 
static void PutRoot(Orthanc::RestApi::PutCall& call)
{
  call.GetOutput().AnswerBuffer("I have received a PUT with body: [" +
                                call.GetPutBody() + "]\n", "text/plain");
}
 
int main()
{
  // Initialize the logging mechanism
  google::InitGoogleLogging("Orthanc");
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;                      // Use the verbose mode
  FLAGS_v = 0;
  
  // Define the callbacks of the REST API
  std::auto_ptr<Orthanc::RestApi> rest(new Orthanc::RestApi);
  rest->Register("/", GetRoot);
  rest->Register("/", PostRoot);
  rest->Register("/", PutRoot);
  rest->Register("/", DeleteRoot);

  // Setup the embedded HTTP server
  Orthanc::MongooseServer httpServer;
  httpServer.SetPortNumber(8042);             // Use TCP port 8042
  httpServer.SetRemoteAccessAllowed(true);    // Do not block remote requests
  httpServer.RegisterHandler(rest.release()); // The REST API is the handler

  // Start the server and wait for the user to hit "Ctrl-C"
  httpServer.Start();
  LOG(WARNING) << "REST server has started";
  Orthanc::Toolbox::ServerBarrier();
  LOG(WARNING) << "REST server has stopped";

  return 0;
}
