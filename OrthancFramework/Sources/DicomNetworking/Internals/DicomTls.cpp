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


#include "../../PrecompiledHeaders.h"
#include "DicomTls.h"

#include "../../Logging.h"
#include "../../OrthancException.h"
#include "../../SystemToolbox.h"


#if DCMTK_VERSION_NUMBER < 364
#  define DCF_Filetype_PEM  SSL_FILETYPE_PEM
#  if OPENSSL_VERSION_NUMBER >= 0x0090700fL
// This seems to correspond to TSP_Profile_AES: https://support.dcmtk.org/docs/tlsciphr_8h.html
static std::string opt_ciphersuites(TLS1_TXT_RSA_WITH_AES_128_SHA ":" SSL3_TXT_RSA_DES_192_CBC3_SHA);
#  else
// This seems to correspond to TSP_Profile_Basic in DCMTK >= 3.6.4: https://support.dcmtk.org/docs/tlsciphr_8h.html
static std::string opt_ciphersuites(SSL3_TXT_RSA_DES_192_CBC3_SHA);
#  endif
#endif


namespace Orthanc
{
  namespace Internals
  {
    DcmTLSTransportLayer* InitializeDicomTls(T_ASC_Network *network,
                                             T_ASC_NetworkRole role,
                                             const std::string& ownPrivateKeyPath,
                                             const std::string& ownCertificatePath,
                                             const std::string& trustedCertificatesPath)
    {
      if (network == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      if (role != NET_ACCEPTOR &&
          role != NET_REQUESTOR)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Unknown role");
      }
    
      if (!SystemToolbox::IsRegularFile(trustedCertificatesPath))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Cannot read file with trusted certificates for DICOM TLS: " +
                               trustedCertificatesPath);
      }

      if (!SystemToolbox::IsRegularFile(ownPrivateKeyPath))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Cannot read file with own private key for DICOM TLS: " +
                               ownPrivateKeyPath);
      }

      if (!SystemToolbox::IsRegularFile(ownCertificatePath))
      {
        throw OrthancException(ErrorCode_InexistentFile, "Cannot read file with own certificate for DICOM TLS: " +
                               ownCertificatePath);
      }

      CLOG(INFO, DICOM) << "Initializing DICOM TLS for Orthanc "
                        << (role == NET_ACCEPTOR ? "SCP" : "SCU");

#if DCMTK_VERSION_NUMBER >= 364
      const T_ASC_NetworkRole tmpRole = role;
#else
      int tmpRole;
      switch (role)
      {
        case NET_ACCEPTOR:
          tmpRole = DICOM_APPLICATION_ACCEPTOR;
          break;
          
        case NET_REQUESTOR:
          tmpRole = DICOM_APPLICATION_REQUESTOR;
          break;
          
        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }          
#endif
      
      std::unique_ptr<DcmTLSTransportLayer> tls(
        new DcmTLSTransportLayer(tmpRole /*opt_networkRole*/, NULL /*opt_readSeedFile*/,
                                 OFFalse /*initializeOpenSSL, done by Orthanc::Toolbox::InitializeOpenSsl()*/));

      if (tls->addTrustedCertificateFile(trustedCertificatesPath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/) != TCS_ok)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse PEM file with trusted certificates for DICOM TLS: " +
                               trustedCertificatesPath);
      }

      if (tls->setPrivateKeyFile(ownPrivateKeyPath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/) != TCS_ok)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse PEM file with private key for DICOM TLS: " +
                               ownPrivateKeyPath);
      }

      if (tls->setCertificateFile(ownCertificatePath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/) != TCS_ok)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse PEM file with own certificate for DICOM TLS: " +
                               ownCertificatePath);
      }

      if (!tls->checkPrivateKeyMatchesCertificate())
      {
        throw OrthancException(ErrorCode_BadFileFormat, "The private key doesn't match the own certificate: " +
                               ownPrivateKeyPath + " vs. " + ownCertificatePath);
      }

#if DCMTK_VERSION_NUMBER >= 364
      if (tls->setTLSProfile(TSP_Profile_BCP195 /*opt_tlsProfile*/) != TCS_ok)
      {
        throw OrthancException(ErrorCode_InternalError, "Cannot set the DICOM TLS profile");
      }
    
      if (tls->activateCipherSuites())
      {
        throw OrthancException(ErrorCode_InternalError, "Cannot activate the cipher suites for DICOM TLS");
      }
#else
      CLOG(INFO, DICOM) << "Using the following cipher suites for DICOM TLS: " << opt_ciphersuites;
      if (tls->setCipherSuites(opt_ciphersuites.c_str()) != TCS_ok)
      {
        throw OrthancException(ErrorCode_InternalError, "Unable to set cipher suites to: " + opt_ciphersuites);
      }
#endif

      tls->setCertificateVerification(DCV_requireCertificate /*opt_certVerification*/);
      
      if (ASC_setTransportLayer(network, tls.get(), 0).bad())
      {
        throw OrthancException(ErrorCode_InternalError, "Cannot enable DICOM TLS in the Orthanc " +
                               std::string(role == NET_ACCEPTOR ? "SCP" : "SCU"));
      }

      return tls.release();
    }
  }
}
