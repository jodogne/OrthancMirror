/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "DicomServer.h"

#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "../../Core/Uuid.h"
#include "../Internals/CommandDispatcher.h"
#include "../OrthancInitialization.h"
#include "EmbeddedResources.h"

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <dcmtk/dcmdata/dcdict.h>

#if defined(__linux)
#include <cstdlib>
#endif


namespace Orthanc
{
  struct DicomServer::PImpl
  {
    boost::thread thread_;

    //std::set<
  };


#if DCMTK_USE_EMBEDDED_DICTIONARIES == 1
  static void LoadEmbeddedDictionary(DcmDataDictionary& dictionary,
                                     EmbeddedResources::FileResourceId resource)
  {
    Toolbox::TemporaryFile tmp;

    FILE* fp = fopen(tmp.GetPath().c_str(), "wb");
    fwrite(EmbeddedResources::GetFileResourceBuffer(resource), 
           EmbeddedResources::GetFileResourceSize(resource), 1, fp);
    fclose(fp);

    if (!dictionary.loadDictionary(tmp.GetPath().c_str()))
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
                             
#else
  static void LoadExternalDictionary(DcmDataDictionary& dictionary,
                                     const std::string& directory,
                                     const std::string& filename)
  {
    boost::filesystem::path p = directory;
    p = p / filename;

    LOG(WARNING) << "Loading the external DICOM dictionary " << p;

    if (!dictionary.loadDictionary(p.string().c_str()))
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
                            
#endif


  void DicomServer::InitializeDictionary()
  {
    /* Disable "gethostbyaddr" (which results in memory leaks) and use raw IP addresses */
    dcmDisableGethostbyaddr.set(OFTrue);

    dcmDataDict.clear();
    DcmDataDictionary& d = dcmDataDict.wrlock();

#if DCMTK_USE_EMBEDDED_DICTIONARIES == 1
    LOG(WARNING) << "Loading the embedded dictionaries";
    /**
     * Do not load DICONDE dictionary, it breaks the other tags. The
     * command "strace storescu 2>&1 |grep dic" shows that DICONDE
     * dictionary is not loaded by storescu.
     **/
    //LoadEmbeddedDictionary(d, EmbeddedResources::DICTIONARY_DICONDE);

    LoadEmbeddedDictionary(d, EmbeddedResources::DICTIONARY_DICOM);
    LoadEmbeddedDictionary(d, EmbeddedResources::DICTIONARY_PRIVATE);

#elif defined(__linux) || defined(__FreeBSD_kernel__)
    std::string path = DCMTK_DICTIONARY_DIR;

    const char* env = std::getenv(DCM_DICT_ENVIRONMENT_VARIABLE);
    if (env != NULL)
    {
      path = std::string(env);
    }

    LoadExternalDictionary(d, path, "dicom.dic");
    LoadExternalDictionary(d, path, "private.dic");

#else
#error Support your platform here
#endif

    dcmDataDict.unlock();

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
      LOG(ERROR) << "No DICOM dictionary loaded, check environment variable: " << DCM_DICT_ENVIRONMENT_VARIABLE;
      throw OrthancException(ErrorCode_InternalError);
    }

    {
      // Test the dictionary with a simple DICOM tag
      DcmTag key(0x0010, 0x1030); // This is PatientWeight
      if (key.getEVR() != EVR_DS)
      {
        LOG(ERROR) << "The DICOM dictionary has not been correctly read";
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  void DicomServer::ServerThread(DicomServer* server)
  {
    /* initialize network, i.e. create an instance of T_ASC_Network*. */
    T_ASC_Network *net;
    OFCondition cond = ASC_initializeNetwork
      (NET_ACCEPTOR, OFstatic_cast(int, server->port_), /*opt_acse_timeout*/ 30, &net);
    if (cond.bad())
    {
      LOG(ERROR) << "cannot create network: " << cond.text();
      throw OrthancException("Cannot create network");
    }

    LOG(INFO) << "DICOM server started";

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

    LOG(INFO) << "DICOM server stopping";

    if (server->isThreaded_)
    {
      server->bagOfDispatchers_.StopAll();
    }

    /* drop the network, i.e. free memory of T_ASC_Network* structure. This call */
    /* is the counterpart of ASC_initializeNetwork(...) which was called above. */
    cond = ASC_dropNetwork(&net);
    if (cond.bad())
    {
      LOG(ERROR) << "Error while dropping the network: " << cond.text();
    }
  }                           


  DicomServer::DicomServer() : 
    pimpl_(new PImpl),
    aet_("ANY-SCP")
  {
    port_ = 104;
    findRequestHandlerFactory_ = NULL;
    moveRequestHandlerFactory_ = NULL;
    storeRequestHandlerFactory_ = NULL;
    applicationEntityFilter_ = NULL;
    checkCalledAet_ = true;
    clientTimeout_ = 30;
    isThreaded_ = true;
    continue_ = false;
    started_ = false;
  }

  DicomServer::~DicomServer()
  {
    if (continue_)
    {
      LOG(ERROR) << "INTERNAL ERROR: DicomServer::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
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
      throw OrthancException("Too short AET");
    }

    if (aet.size() > 16)
    {
      throw OrthancException("AET must be shorter than 16 characters");
    }

    for (size_t i = 0; i < aet.size(); i++)
    {
      if (!(aet[i] == '-' ||
            aet[i] == '_' ||
            isdigit(aet[i]) ||
            (aet[i] >= 'A' && aet[i] <= 'Z')))
      {
        LOG(WARNING) << "For best interoperability, only upper case, alphanumeric characters should be present in AET: \"" << aet << "\"";
        break;
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
      throw OrthancException("No C-FIND request handler factory");
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
      throw OrthancException("No C-MOVE request handler factory");
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
      throw OrthancException("No C-STORE request handler factory");
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
      throw OrthancException("No application entity filter");
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
    if (continue_)
    {
      continue_ = false;

      if (pimpl_->thread_.joinable())
      {
        pimpl_->thread_.join();
      }

      bagOfDispatchers_.Finalize();
    }
  }


  bool DicomServer::IsMyAETitle(const std::string& aet) const
  {
    if (!HasCalledApplicationEntityTitleCheck())
    {
      // OK, no check on the AET.
      return true;
    }

    return Configuration::IsSameAETitle(aet, GetApplicationEntityTitle());
  }

}
