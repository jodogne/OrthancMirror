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


#pragma once

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if !defined(ORTHANC_ENABLE_PKCS11)
#  error The macro ORTHANC_ENABLE_PKCS11 must be defined
#endif

#if !defined(ORTHANC_ENABLE_SSL)
#  error The macro ORTHANC_ENABLE_SSL must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error This file cannot be used in sandboxed environments
#endif

#if ORTHANC_ENABLE_PKCS11 != 1 || ORTHANC_ENABLE_SSL != 1
#  error This file cannot be used if OpenSSL or PKCS#11 support is disabled
#endif


#include <string>

namespace Orthanc
{
  namespace Pkcs11
  {
    const char* GetEngineIdentifier();

    bool IsInitialized();

    void Initialize(const std::string& module,
                    const std::string& pin,
                    bool verbose);

    void Finalize();
  }
}
