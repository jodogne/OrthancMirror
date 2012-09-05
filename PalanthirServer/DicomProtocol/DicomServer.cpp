/**
 * Palanthir - A Lightweight, RESTful DICOM Store
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


#include "DicomServer.h"

#include "../../Core/PalanthirException.h"
#include "../../Core/Toolbox.h"
#include "../Internals/CommandDispatcher.h"

#include <boost/thread.hpp>
#include <dcmtk/dcmdata/dcdict.h>


namespace Palanthir
{
  struct DicomServer::PImpl
  {
    boost::thread thread_;

    //std::set<
  };


  namespace Internals
  {
    OFLogger Logger = OFLog::getLogger("dcmtk.apps.storescp");
  }


  void DicomServer::ServerThread(DicomServer* server)
  {
    /* Disable "gethostbyaddr" (which results in memory leaks) and use raw IP addresses */
    dcmDisableGethostbyaddr.set(OFTrue);

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
      OFLOG_WARN(Internals::Logger, "no data dictionary loaded, check environment variable: "
                 << DCM_DICT_ENVIRONMENT_VARIABLE);
    }

    /* initialize network, i.e. create an instance of T_ASC_Network*. */
    T_ASC_Network *net;
    OFCondition cond = ASC_initializeNetwork
      (NET_ACCEPTOR, OFstatic_cast(int, server->port_), /*opt_acse_timeout*/ 30, &net);
    if (cond.bad())
    {
      OFString temp_str;
      OFLOG_ERROR(Internals::Logger, "cannot create network: " << DimseCondition::dump(temp_str, cond));
      throw PalanthirException("Cannot create network");
    }

    OFLOG_WARN(Internals::Logger, "DICOM server started");

    server->started_ = true;

    while (server->continue_)
    {
      /* receive an association and acknowledge or reject it. If the association was */
      /* acknowledged, offer corresponding services and invoke one or more if required. */
      std::auto_ptr<Internals::CommandDispatcher> dispatcher(Internals::AcceptAssociation(*server, net));

      if (dispatcher.get() != NULL)
      {
        if (server->isThreaded_)
        {
          server->bagOfDispatchers_.Add(dispatcher.release());
        }
        else
        {
          IRunnableBySteps::RunUntilDone(*dispatcher);
        }
      }
    }

    OFLOG_WARN(Internals::Logger, "DICOM server stopping");

    /* drop the network, i.e. free memory of T_ASC_Network* structure. This call */
    /* is the counterpart of ASC_initializeNetwork(...) which was called above. */
    cond = ASC_dropNetwork(&net);
    if (cond.bad())
    {
      OFString temp_str;
      OFLOG_ERROR(Internals::Logger, DimseCondition::dump(temp_str, cond));
    }
  }                           


  DicomServer::DicomServer() : pimpl_(new PImpl)
  {
    aet_ = "ANY-SCP";
    port_ = 104;
    findRequestHandlerFactory_ = NULL;
    moveRequestHandlerFactory_ = NULL;
    storeRequestHandlerFactory_ = NULL;
    applicationEntityFilter_ = NULL;
    checkCalledAet_ = true;
    clientTimeout_ = 30;
    isThreaded_ = true;
  }

  DicomServer::~DicomServer()
  {
    Stop();
  }

  void DicomServer::SetPortNumber(uint16_t port)
  {
    Stop();
    port_ = port;
  }

  uint16_t DicomServer::GetPortNumber() const
  {
    return port_;
  }

  void DicomServer::SetThreaded(bool isThreaded)
  {
    Stop();
    isThreaded_ = isThreaded;
  }

  bool DicomServer::IsThreaded() const
  {
    return isThreaded_;
  }

  void DicomServer::SetClientTimeout(uint32_t timeout)
  {
    Stop();
    clientTimeout_ = timeout;
  }

  uint32_t DicomServer::GetClientTimeout() const
  {
    return clientTimeout_;
  }


  void DicomServer::SetCalledApplicationEntityTitleCheck(bool check)
  {
    Stop();
    checkCalledAet_ = check;
  }

  bool DicomServer::HasCalledApplicationEntityTitleCheck() const
  {
    return checkCalledAet_;
  }

  void DicomServer::SetApplicationEntityTitle(const std::string& aet)
  {
    if (aet.size() == 0)
    {
      throw PalanthirException("Too short AET");
    }

    for (size_t i = 0; i < aet.size(); i++)
    {
      if (!isalnum(aet[i]) && aet[i] != '-')
      {
        throw PalanthirException("Only alphanumeric characters are allowed in AET");
      }
    }

    Stop();
    aet_ = aet;
  }

  const std::string& DicomServer::GetApplicationEntityTitle() const
  {
    return aet_;
  }

  void DicomServer::SetFindRequestHandlerFactory(IFindRequestHandlerFactory& factory)
  {
    Stop();
    findRequestHandlerFactory_ = &factory;
  }

  bool DicomServer::HasFindRequestHandlerFactory() const
  {
    return (findRequestHandlerFactory_ != NULL);
  }

  IFindRequestHandlerFactory& DicomServer::GetFindRequestHandlerFactory() const
  {
    if (HasFindRequestHandlerFactory())
    {
      return *findRequestHandlerFactory_;
    }
    else
    {
      throw PalanthirException("No C-FIND request handler factory");
    }
  }

  void DicomServer::SetMoveRequestHandlerFactory(IMoveRequestHandlerFactory& factory)
  {
    Stop();
    moveRequestHandlerFactory_ = &factory;
  }

  bool DicomServer::HasMoveRequestHandlerFactory() const
  {
    return (moveRequestHandlerFactory_ != NULL);
  }

  IMoveRequestHandlerFactory& DicomServer::GetMoveRequestHandlerFactory() const
  {
    if (HasMoveRequestHandlerFactory())
    {
      return *moveRequestHandlerFactory_;
    }
    else
    {
      throw PalanthirException("No C-MOVE request handler factory");
    }
  }

  void DicomServer::SetStoreRequestHandlerFactory(IStoreRequestHandlerFactory& factory)
  {
    Stop();
    storeRequestHandlerFactory_ = &factory;
  }

  bool DicomServer::HasStoreRequestHandlerFactory() const
  {
    return (storeRequestHandlerFactory_ != NULL);
  }

  IStoreRequestHandlerFactory& DicomServer::GetStoreRequestHandlerFactory() const
  {
    if (HasStoreRequestHandlerFactory())
    {
      return *storeRequestHandlerFactory_;
    }
    else
    {
      throw PalanthirException("No C-STORE request handler factory");
    }
  }

  void DicomServer::SetApplicationEntityFilter(IApplicationEntityFilter& factory)
  {
    Stop();
    applicationEntityFilter_ = &factory;
  }

  bool DicomServer::HasApplicationEntityFilter() const
  {
    return (applicationEntityFilter_ != NULL);
  }

  IApplicationEntityFilter& DicomServer::GetApplicationEntityFilter() const
  {
    if (HasApplicationEntityFilter())
    {
      return *applicationEntityFilter_;
    }
    else
    {
      throw PalanthirException("No application entity filter");
    }
  }

  void DicomServer::Start()
  {
    Stop();
    continue_ = true;
    started_ = false;
    pimpl_->thread_ = boost::thread(ServerThread, this);

    while (!started_)
    {
      Toolbox::USleep(50000);  // Wait 50ms
    }
  }

  void DicomServer::Stop()
  {
    continue_ = false;
    pimpl_->thread_.join();

    bagOfDispatchers_.StopAll();
  }
}
