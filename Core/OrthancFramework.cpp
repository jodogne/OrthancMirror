/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
