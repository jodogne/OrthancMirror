/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../Toolbox.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

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
#if DCMTK_VERSION_NUMBER >= 367
    static bool IsFailure(OFCondition cond)
    {
      return !cond.good();
    }
#else
    static bool IsFailure(DcmTransportLayerStatus status)
    {
      return (status != TCS_ok);
    }
#endif


#if DCMTK_VERSION_NUMBER >= 367
    static OFCondition MyConvertOpenSSLError(unsigned long errorCode, OFBool logAsError)
    {
      return DcmTLSTransportLayer::convertOpenSSLError(errorCode, logAsError);
    }
#else
    static OFCondition MyConvertOpenSSLError(unsigned long errorCode, OFBool logAsError)
    {
      if (errorCode == 0)
      {
        return EC_Normal;
      }
      else
      {
        const char *err = ERR_reason_error_string(errorCode);
        if (err == NULL)
        {
          err = "OpenSSL error";
        }

        if (logAsError)
        {
          DCMTLS_ERROR("OpenSSL error " << STD_NAMESPACE hex << STD_NAMESPACE setfill('0')
                       << STD_NAMESPACE setw(8) << errorCode << ": " << err);
        }

        // The "2" below corresponds to the same error code as "DCMTLS_EC_FailedToSetCiphersuites"
        return OFCondition(OFM_dcmtls, 2, OF_error, err);
      }
    }
#endif


    DcmTLSTransportLayer* InitializeDicomTls(T_ASC_Network *network,
                                             T_ASC_NetworkRole role,
                                             const std::string& ownPrivateKeyPath,
                                             const std::string& ownCertificatePath,
                                             const std::string& trustedCertificatesPath,
                                             bool requireRemoteCertificate,
                                             unsigned int minimalTlsVersion,
                                             const std::set<std::string>& ciphers)
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
    
      if (requireRemoteCertificate && !SystemToolbox::IsRegularFile(trustedCertificatesPath))
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

      if (requireRemoteCertificate && IsFailure(tls->addTrustedCertificateFile(trustedCertificatesPath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/)))
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse PEM file with trusted certificates for DICOM TLS: " +
                               trustedCertificatesPath);
      }

      if (IsFailure(tls->setPrivateKeyFile(ownPrivateKeyPath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/)))
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse PEM file with private key for DICOM TLS: " +
                               ownPrivateKeyPath);
      }

      if (IsFailure(tls->setCertificateFile(
                      ownCertificatePath.c_str(), DCF_Filetype_PEM /*opt_keyFileFormat*/
#if DCMTK_VERSION_NUMBER >= 368
                      /**
                       * DICOM BCP 195 RFC 8996 TLS Profile, based on RFC 8996 and RFC 9325.
                       * This profile only negotiates TLS 1.2 or newer, and will not fall back to
                       * previous TLS versions. It provides the higher security level offered by the
                       * 2021 revised edition of BCP 195.
                       **/
                      , TSP_Profile_BCP_195_RFC_8996
#endif
                      )))
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
      if (minimalTlsVersion == 0) // use the default values (same behavior as before 1.12.4)
      {
        if (ciphers.size() > 0)
        {
          throw OrthancException(ErrorCode_BadFileFormat, "The cipher suites can not be specified when using the default BCP profile");
        }

        if (IsFailure(tls->setTLSProfile(TSP_Profile_BCP195 /*opt_tlsProfile*/)))
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot set the DICOM TLS profile");
        }
      
        if (IsFailure(tls->activateCipherSuites()))
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot activate the cipher suites for DICOM TLS");
        }
      }
      else
      {
        // Fine tune the SSL context
        if (IsFailure(tls->setTLSProfile(TSP_Profile_None)))
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot set the DICOM TLS profile");
        }

        DcmTLSTransportLayer::native_handle_type sslNativeHandle = tls->getNativeHandle();
        SSL_CTX_clear_options(sslNativeHandle, SSL_OP_NO_SSL_MASK);
        if (minimalTlsVersion > 1) 
        {
          SSL_CTX_set_options(sslNativeHandle, SSL_OP_NO_SSLv3);
        }
        if (minimalTlsVersion > 2) 
        {
          SSL_CTX_set_options(sslNativeHandle, SSL_OP_NO_TLSv1);
        }
        if (minimalTlsVersion > 3) 
        {
          SSL_CTX_set_options(sslNativeHandle, SSL_OP_NO_TLSv1_1);
        }
        if (minimalTlsVersion > 4) 
        {
          SSL_CTX_set_options(sslNativeHandle, SSL_OP_NO_TLSv1_2);
        }

        std::set<std::string> ciphersTls;
        std::set<std::string> ciphersTls13;

        // DCMTK 3.8 is missing a method to add TLS13 cipher suite in the DcmTLSTransportLayer interface.
        // And, anyway, since we do not run dcmtkPrepare.cmake, DCMTK is not aware of TLS v1.3 cipher suite names.
        for (std::set<std::string>::const_iterator it = ciphers.begin(); it != ciphers.end(); ++it)
        {
          bool isValid = false;
          if (DcmTLSCiphersuiteHandler::lookupCiphersuiteByOpenSSLName(it->c_str()) != DcmTLSCiphersuiteHandler::unknownCipherSuiteIndex)
          {
            ciphersTls.insert(it->c_str());
            isValid = true;
          }
          
          // list of TLS v1.3 ciphers according to https://www.openssl.org/docs/man3.3/man1/openssl-ciphers.html
          if (strstr("TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256", it->c_str()) != NULL)
          {
            ciphersTls13.insert(it->c_str());
            isValid = true;
          }

          if (!isValid)
          {
            throw OrthancException(ErrorCode_BadFileFormat, "The cipher suite " + *it + " is not recognized as valid cipher suite by OpenSSL ");
          }
        }

        std::string joinedCiphersTls;
        std::string joinedCiphersTls13;
        Toolbox::JoinStrings(joinedCiphersTls, ciphersTls, ":");
        Toolbox::JoinStrings(joinedCiphersTls13, ciphersTls13, ":");

        if (joinedCiphersTls.size() > 0 && SSL_CTX_set_cipher_list(sslNativeHandle, joinedCiphersTls.c_str()) != 1)
        {
          OFCondition cond = MyConvertOpenSSLError(ERR_get_error(), OFTrue);
          throw OrthancException(ErrorCode_InternalError, "Unable to configure cipher suite.  OpenSSL error: " + boost::lexical_cast<std::string>(cond.code()) + " - " + cond.text());
        }

        if (joinedCiphersTls13.size() > 0 && SSL_CTX_set_ciphersuites(sslNativeHandle, joinedCiphersTls13.c_str()) != 1)
        {
          OFCondition cond = MyConvertOpenSSLError(ERR_get_error(), OFTrue);
          throw OrthancException(ErrorCode_InternalError, "Unable to configure cipher suite for TLS 1.3.  OpenSSL error: " + boost::lexical_cast<std::string>(cond.code()) + " - " + cond.text());
        }

      }
#else
      CLOG(INFO, DICOM) << "Using the following cipher suites for DICOM TLS: " << opt_ciphersuites;
      if (IsFailure(tls->setCipherSuites(opt_ciphersuites.c_str())))
      {
        throw OrthancException(ErrorCode_InternalError, "Unable to set cipher suites to: " + opt_ciphersuites);
      }
#endif

      if (requireRemoteCertificate)
      {
        // Check remote certificate, fail if no certificate is present
        tls->setCertificateVerification(DCV_requireCertificate /*opt_certVerification*/);
      }
      else
      {
        // From 1.12.4, do not even request remote certificate (prior to 1.12.4, we were requesting a certificates, checking it if present and succeeding if not present)
        tls->setCertificateVerification(DCV_ignoreCertificate /*opt_certVerification*/);
      }
      
      if (ASC_setTransportLayer(network, tls.get(), 0).bad())
      {
        throw OrthancException(ErrorCode_InternalError, "Cannot enable DICOM TLS in the Orthanc " +
                               std::string(role == NET_ACCEPTOR ? "SCP" : "SCU"));
      }

      return tls.release();
    }
  }
}
