/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../SystemToolbox.h"
#include "NetworkingCompatibility.h"

#include <dcmtk/dcmnet/diutil.h>  // For ASC_DEFAULTMAXPDU

#include <boost/thread/mutex.hpp>

// By default, the default timeout for client DICOM connections is set to 10 seconds
static boost::mutex  defaultConfigurationMutex_;
static uint32_t      defaultTimeout_ = 10;
static std::string   defaultOwnPrivateKeyPath_;
static std::string   defaultOwnCertificatePath_;
static std::string   defaultTrustedCertificatesPath_;
static unsigned int  defaultMaximumPduLength_ = ASC_DEFAULTMAXPDU;
static bool          defaultRemoteCertificateRequired_ = true;


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
    boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
    return defaultTimeout_;
  }


  void DicomAssociationParameters::SetDefaultParameters()
  {
    boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
    timeout_ = defaultTimeout_;
    ownPrivateKeyPath_ = defaultOwnPrivateKeyPath_;
    ownCertificatePath_ = defaultOwnCertificatePath_;
    trustedCertificatesPath_ = defaultTrustedCertificatesPath_;
    maximumPduLength_ = defaultMaximumPduLength_;
    remoteCertificateRequired_ = defaultRemoteCertificateRequired_;
  }


  DicomAssociationParameters::DicomAssociationParameters() :
    localAet_("ORTHANC"),
    timeout_(0),  // Will be set by SetDefaultParameters()
    maximumPduLength_(0)  // Will be set by SetDefaultParameters()
  {
    SetDefaultParameters();
    remote_.SetApplicationEntityTitle("ANY-SCP");
  }

    
  DicomAssociationParameters::DicomAssociationParameters(const std::string& localAet,
                                                         const RemoteModalityParameters& remote) :
    localAet_(localAet),
    timeout_(0),  // Will be set by SetDefaultParameters()
    maximumPduLength_(0)  // Will be set by SetDefaultParameters()
  {
    SetDefaultParameters();
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

    if (remote.HasTimeout())
    {
      timeout_ = remote.GetTimeout();
      assert(timeout_ != 0);
    }
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
            timeout_ == other.timeout_ &&
            ownPrivateKeyPath_ == other.ownPrivateKeyPath_ &&
            ownCertificatePath_ == other.ownCertificatePath_ &&
            trustedCertificatesPath_ == other.trustedCertificatesPath_ &&
            maximumPduLength_ == other.maximumPduLength_);
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


  void DicomAssociationParameters::CheckDicomTlsConfiguration() const
  {
    if (!remote_.IsDicomTlsEnabled())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "DICOM TLS is not enabled");
    }
    else if (ownPrivateKeyPath_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "DICOM TLS - No path to the private key of the local certificate was provided");
    }
    else if (ownCertificatePath_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "DICOM TLS - No path to the local certificate was provided");
    }
    else if (trustedCertificatesPath_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "DICOM TLS - No path to the trusted remote certificates was provided");
    }
  }
  
  void DicomAssociationParameters::SetOwnCertificatePath(const std::string& privateKeyPath,
                                                         const std::string& certificatePath)
  {
    ownPrivateKeyPath_ = privateKeyPath;
    ownCertificatePath_ = certificatePath;
  }

  void DicomAssociationParameters::SetTrustedCertificatesPath(const std::string& path)
  {
    trustedCertificatesPath_ = path;
  }

  const std::string& DicomAssociationParameters::GetOwnPrivateKeyPath() const
  {
    CheckDicomTlsConfiguration();
    return ownPrivateKeyPath_;
  }
    
  const std::string& DicomAssociationParameters::GetOwnCertificatePath() const
  {
    CheckDicomTlsConfiguration();
    return ownCertificatePath_;
  }

  const std::string& DicomAssociationParameters::GetTrustedCertificatesPath() const
  {
    CheckDicomTlsConfiguration();
    return trustedCertificatesPath_;
  }

  unsigned int DicomAssociationParameters::GetMaximumPduLength() const
  {
    return maximumPduLength_;
  }

  void DicomAssociationParameters::SetMaximumPduLength(unsigned int pdu)
  {
    CheckMaximumPduLength(pdu);
    maximumPduLength_ = pdu;
  }

  void DicomAssociationParameters::SetRemoteCertificateRequired(bool required)
  {
    remoteCertificateRequired_ = required;
  }

  bool DicomAssociationParameters::IsRemoteCertificateRequired() const
  {
    return remoteCertificateRequired_;
  }

  

  static const char* const LOCAL_AET = "LocalAet";
  static const char* const REMOTE = "Remote";
  static const char* const TIMEOUT = "Timeout";                           // New in Orthanc in 1.7.0
  static const char* const OWN_PRIVATE_KEY = "OwnPrivateKey";             // New in Orthanc 1.9.0
  static const char* const OWN_CERTIFICATE = "OwnCertificate";            // New in Orthanc 1.9.0
  static const char* const TRUSTED_CERTIFICATES = "TrustedCertificates";  // New in Orthanc 1.9.0
  static const char* const MAXIMUM_PDU_LENGTH = "MaximumPduLength";       // New in Orthanc 1.9.0
  static const char* const REMOTE_CERTIFICATE_REQUIRED = "RemoteCertificateRequired";  // New in Orthanc 1.9.3

  
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
      target[MAXIMUM_PDU_LENGTH] = maximumPduLength_;
      target[REMOTE_CERTIFICATE_REQUIRED] = remoteCertificateRequired_;

      // Don't write the DICOM TLS parameters if they are not required
      if (ownPrivateKeyPath_.empty())
      {
        target.removeMember(OWN_PRIVATE_KEY);
      }
      else
      {
        target[OWN_PRIVATE_KEY] = ownPrivateKeyPath_;
      }
      
      if (ownCertificatePath_.empty())
      {
        target.removeMember(OWN_CERTIFICATE);
      }
      else
      {
        target[OWN_CERTIFICATE] = ownCertificatePath_;
      }
      
      if (trustedCertificatesPath_.empty())
      {
        target.removeMember(TRUSTED_CERTIFICATES);
      }
      else
      {
        target[TRUSTED_CERTIFICATES] = trustedCertificatesPath_;
      }
    }
  }


  DicomAssociationParameters DicomAssociationParameters::UnserializeJob(const Json::Value& serialized)
  {
    if (serialized.type() == Json::objectValue)
    {
      DicomAssociationParameters result;

      if (!serialized.isMember(REMOTE))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      result.remote_ = RemoteModalityParameters(serialized[REMOTE]);
      result.localAet_ = SerializationToolbox::ReadString(serialized, LOCAL_AET);
      result.timeout_ = SerializationToolbox::ReadInteger(serialized, TIMEOUT, GetDefaultTimeout());

      // The calls to "isMember()" below are for compatibility with Orthanc <= 1.8.2 serialization
      if (serialized.isMember(MAXIMUM_PDU_LENGTH))
      {
        result.maximumPduLength_ = SerializationToolbox::ReadUnsignedInteger(
          serialized, MAXIMUM_PDU_LENGTH, defaultMaximumPduLength_);
      }

      if (serialized.isMember(OWN_PRIVATE_KEY))
      {
        result.ownPrivateKeyPath_ = SerializationToolbox::ReadString(serialized, OWN_PRIVATE_KEY);
      }
      else
      {
        result.ownPrivateKeyPath_.clear();
      }

      if (serialized.isMember(OWN_CERTIFICATE))
      {
        result.ownCertificatePath_ = SerializationToolbox::ReadString(serialized, OWN_CERTIFICATE);
      }
      else
      {
        result.ownCertificatePath_.clear();
      }

      if (serialized.isMember(TRUSTED_CERTIFICATES))
      {
        result.trustedCertificatesPath_ = SerializationToolbox::ReadString(serialized, TRUSTED_CERTIFICATES);
      }
      else
      {
        result.trustedCertificatesPath_.clear();
      }

      if (serialized.isMember(REMOTE_CERTIFICATE_REQUIRED))
      {
        result.remoteCertificateRequired_ = SerializationToolbox::ReadBoolean(serialized, REMOTE_CERTIFICATE_REQUIRED);
      }
      
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
      boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
      defaultTimeout_ = seconds;
    }
  }


  void DicomAssociationParameters::SetDefaultOwnCertificatePath(const std::string& privateKeyPath,
                                                                const std::string& certificatePath)
  {
    if (!privateKeyPath.empty() &&
        !certificatePath.empty())
    {
      CLOG(INFO, DICOM) << "Setting the default TLS certificate for DICOM SCU connections: " 
                        << privateKeyPath << " (key), " << certificatePath << " (certificate)";

      if (certificatePath.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "No path to the default DICOM TLS certificate was provided");
      }
      
      if (privateKeyPath.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "No path to the private key for the default DICOM TLS certificate was provided");
      }
      
      if (!SystemToolbox::IsRegularFile(privateKeyPath))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Inexistent file: " + privateKeyPath);
      }

      if (!SystemToolbox::IsRegularFile(certificatePath))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Inexistent file: " + certificatePath);
      }
      
      {
        boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
        defaultOwnPrivateKeyPath_ = privateKeyPath;
        defaultOwnCertificatePath_ = certificatePath;
      }
    }
    else
    {
      boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
      defaultOwnPrivateKeyPath_.clear();
      defaultOwnCertificatePath_.clear();
    }
  }    

  
  void DicomAssociationParameters::SetDefaultTrustedCertificatesPath(const std::string& path)
  {
    if (!path.empty())
    {
      CLOG(INFO, DICOM) << "Setting the default trusted certificates for DICOM SCU connections: " << path;

      if (!SystemToolbox::IsRegularFile(path))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Inexistent file: " + path);
      }
      
      {
        boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
        defaultTrustedCertificatesPath_ = path;
      }
    }
    else
    {
      boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
      defaultTrustedCertificatesPath_.clear();
    }
  }



  void DicomAssociationParameters::CheckMaximumPduLength(unsigned int pdu)
  {
    if (pdu > ASC_MAXIMUMPDUSIZE)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Maximum PDU length must be smaller than " +
                             boost::lexical_cast<std::string>(ASC_MAXIMUMPDUSIZE));
    }
    else if (pdu < ASC_MINIMUMPDUSIZE)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Maximum PDU length must be greater than " +
                             boost::lexical_cast<std::string>(ASC_MINIMUMPDUSIZE));
    }
  }


  void DicomAssociationParameters::SetDefaultMaximumPduLength(unsigned int pdu)
  {
    CheckMaximumPduLength(pdu);

    {
      boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
      defaultMaximumPduLength_ = pdu;
    }
  }


  unsigned int DicomAssociationParameters::GetDefaultMaximumPduLength()
  {
    boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
    return defaultMaximumPduLength_;
  }


  void DicomAssociationParameters::SetDefaultRemoteCertificateRequired(bool required)
  {
    boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
    defaultRemoteCertificateRequired_ = required;
  }
  

  bool DicomAssociationParameters::GetDefaultRemoteCertificateRequired()
  {
    boost::mutex::scoped_lock lock(defaultConfigurationMutex_);
    return defaultRemoteCertificateRequired_;
  }
}
