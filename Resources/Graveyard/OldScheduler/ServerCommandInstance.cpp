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


#include "../PrecompiledHeadersServer.h"
#include "ServerCommandInstance.h"

#include "../../Core/OrthancException.h"

namespace Orthanc
{
  bool ServerCommandInstance::Execute(IListener& listener)
  {
    ListOfStrings outputs;

    bool success = false;

    try
    {
      if (command_->Apply(outputs, inputs_))
      {
        success = true;
      }
    }
    catch (OrthancException&)
    {
    }

    if (!success)
    {
      listener.SignalFailure(jobId_);
      return true;
    }

    for (std::list<ServerCommandInstance*>::iterator
           it = next_.begin(); it != next_.end(); ++it)
    {
      for (ListOfStrings::const_iterator
             output = outputs.begin(); output != outputs.end(); ++output)
      {
        (*it)->AddInput(*output);
      }
    }

    listener.SignalSuccess(jobId_);
    return true;
  }


  ServerCommandInstance::ServerCommandInstance(IServerCommand *command,
                                               const std::string& jobId) : 
    command_(command), 
    jobId_(jobId),
    connectedToSink_(false)
  {
    if (command_ == NULL)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ServerCommandInstance::~ServerCommandInstance()
  {
    if (command_ != NULL)
    {
      delete command_;
    }
  }
}
