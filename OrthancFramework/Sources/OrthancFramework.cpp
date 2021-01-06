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


#include "PrecompiledHeaders.h"
#include "OrthancFramework.h"

#if !defined(ORTHANC_ENABLE_CURL)
#  error Macro ORTHANC_ENABLE_CURL must be defined
#endif

#if !defined(ORTHANC_ENABLE_SSL)
#  error Macro ORTHANC_ENABLE_SSL must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK)
#  error Macro ORTHANC_ENABLE_DCMTK must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_NETWORKING)
#  error Macro ORTHANC_ENABLE_DCMTK_NETWORKING must be defined
#endif

#if ORTHANC_ENABLE_CURL == 1
#  include "HttpClient.h"
#endif

#if ORTHANC_ENABLE_DCMTK == 1
#  include "DicomParsing/FromDcmtkBridge.h"
#  if ORTHANC_ENABLE_DCMTK_NETWORKING == 1
#    include <dcmtk/dcmnet/dul.h>
#  endif
#endif

#include "Logging.h"
#include "Toolbox.h"


namespace Orthanc
{
  void InitializeFramework(const std::string& locale,
                           bool loadPrivateDictionary)
  {
    Logging::Initialize();

#if (ORTHANC_ENABLE_LOCALE == 1) && !defined(__EMSCRIPTEN__)  // No global locale in wasm/asm.js
    if (locale.empty())
    {
      Toolbox::InitializeGlobalLocale(NULL);
    }
    else
    {
      Toolbox::InitializeGlobalLocale(locale.c_str());
    }
#endif

    Toolbox::InitializeOpenSsl();

#if ORTHANC_ENABLE_CURL == 1
    HttpClient::GlobalInitialize();
#endif

#if ORTHANC_ENABLE_DCMTK == 1
    FromDcmtkBridge::InitializeDictionary(true);
    FromDcmtkBridge::InitializeCodecs();
#endif

#if (ORTHANC_ENABLE_DCMTK == 1 &&               \
     ORTHANC_ENABLE_DCMTK_NETWORKING == 1)
    /* Disable "gethostbyaddr" (which results in memory leaks) and use raw IP addresses */
    dcmDisableGethostbyaddr.set(OFTrue);
#endif
  }
  

  void FinalizeFramework()
  {
#if ORTHANC_ENABLE_DCMTK == 1
    FromDcmtkBridge::FinalizeCodecs();
#endif

#if ORTHANC_ENABLE_CURL == 1
    HttpClient::GlobalFinalize();
#endif
    
    Toolbox::FinalizeOpenSsl();

#if (ORTHANC_ENABLE_LOCALE == 1) && !defined(__EMSCRIPTEN__)
    Toolbox::FinalizeGlobalLocale();
#endif
    
    Logging::Finalize();
  }
}
