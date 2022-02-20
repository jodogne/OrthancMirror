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


#pragma once

#if ORTHANC_ENABLE_DCMTK_NETWORKING != 1
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be set to 1
#endif

#if !defined(ORTHANC_ENABLE_SSL)
#  error The macro ORTHANC_ENABLE_SSL must be defined
#endif

#if ORTHANC_ENABLE_SSL != 1
#  error SSL support must be enabled to use this file
#endif


#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmtls/tlslayer.h>


namespace Orthanc
{
  namespace Internals
  {
    DcmTLSTransportLayer* InitializeDicomTls(
      T_ASC_Network *network,
      T_ASC_NetworkRole role,
      const std::string& ownPrivateKeyPath,        // This is the first argument of "+tls" option from DCMTK command-line tools
      const std::string& ownCertificatePath,       // This is the second argument of "+tls" option
      const std::string& trustedCertificatesPath,  // This is the "--add-cert-file" ("+cf") option
      bool requireRemoteCertificate);              // "true" means "--require-peer-cert", "false" means "--verify-peer-cert"
  }
}
