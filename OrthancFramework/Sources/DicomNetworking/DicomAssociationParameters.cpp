/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomAssociationParameters.h"

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "NetworkingCompatibility.h"

#include <boost/thread/mutex.hpp>

// By default, the timeout for client DICOM connections is set to 10 seconds
static boost::mutex  defaultTimeoutMutex_;
static uint32_t defaultTimeout_ = 10;


namespace Orthanc
{
  void DicomAssociationParameters::CheckHost(const std::string& host)
  {
    if (host.size() > HOST_NAME_MAX - 10)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Invalid host name (too long): " + host);
    }
  }

  
  uint32_t DicomAssociationParameters::GetDefaultTimeout()
  {
    boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
    return defaultTimeout_;
  }


  DicomAssociationParameters::DicomAssociationParameters() :
    localAet_("ORTHANC"),
    timeout_(GetDefaultTimeout())
  {
    remote_.SetApplicationEntityTitle("ANY-SCP");
  }

    
  DicomAssociationParameters::DicomAssociationParameters(const std::string& localAet,
                                                         const RemoteModalityParameters& remote) :
    localAet_(localAet),
    timeout_(GetDefaultTimeout())
  {
    SetRemoteModality(remote);
  }

  const std::string &DicomAssociationParameters::GetLocalApplicationEntityTitle() const
  {
    return localAet_;
  }

  void DicomAssociationParameters::SetLocalApplicationEntityTitle(const std::string &aet)
  {
    localAet_ = aet;
  }

  const RemoteModalityParameters &DicomAssociationParameters::GetRemoteModality() const
  {
    return remote_;
  }


  void DicomAssociationParameters::SetRemoteModality(const RemoteModalityParameters& remote)
  {
    CheckHost(remote.GetHost());
    remote_ = remote;
  }

  void DicomAssociationParameters::SetRemoteApplicationEntityTitle(const std::string &aet)
  {
    remote_.SetApplicationEntityTitle(aet);
  }


  void DicomAssociationParameters::SetRemoteHost(const std::string& host)
  {
    CheckHost(host);
    remote_.SetHost(host);
  }

  void DicomAssociationParameters::SetRemotePort(uint16_t port)
  {
    remote_.SetPortNumber(port);
  }

  void DicomAssociationParameters::SetRemoteManufacturer(ModalityManufacturer manufacturer)
  {
    remote_.SetManufacturer(manufacturer);
  }


  bool DicomAssociationParameters::IsEqual(const DicomAssociationParameters& other) const
  {
    return (localAet_ == other.localAet_ &&
            remote_.GetApplicationEntityTitle() == other.remote_.GetApplicationEntityTitle() &&
            remote_.GetHost() == other.remote_.GetHost() &&
            remote_.GetPortNumber() == other.remote_.GetPortNumber() &&
            remote_.GetManufacturer() == other.remote_.GetManufacturer() &&
            timeout_ == other.timeout_);
  }

  void DicomAssociationParameters::SetTimeout(uint32_t seconds)
  {
    timeout_ = seconds;
  }

  uint32_t DicomAssociationParameters::GetTimeout() const
  {
    return timeout_;
  }

  bool DicomAssociationParameters::HasTimeout() const
  {
    return timeout_ != 0;
  }


  static const char* const LOCAL_AET = "LocalAet";
  static const char* const REMOTE = "Remote";
  static const char* const TIMEOUT = "Timeout";  // New in Orthanc in 1.7.0

  
  void DicomAssociationParameters::SerializeJob(Json::Value& target) const
  {
    if (target.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      target[LOCAL_AET] = localAet_;
      remote_.Serialize(target[REMOTE], true /* force advanced format */);
      target[TIMEOUT] = timeout_;
    }
  }


  DicomAssociationParameters DicomAssociationParameters::UnserializeJob(const Json::Value& serialized)
  {
    if (serialized.type() == Json::objectValue)
    {
      DicomAssociationParameters result;
    
      result.remote_ = RemoteModalityParameters(serialized[REMOTE]);
      result.localAet_ = SerializationToolbox::ReadString(serialized, LOCAL_AET);
      result.timeout_ = SerializationToolbox::ReadInteger(serialized, TIMEOUT, GetDefaultTimeout());

      return result;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
    

  void DicomAssociationParameters::SetDefaultTimeout(uint32_t seconds)
  {
    CLOG(INFO, DICOM) << "Default timeout for DICOM connections if Orthanc acts as SCU (client): " 
                      << seconds << " seconds (0 = no timeout)";

    {
      boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
      defaultTimeout_ = seconds;
    }
  }
}
