/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
#include "Pkcs11.h"


#if defined(OPENSSL_NO_RSA) || defined(OPENSSL_NO_EC) || defined(OPENSSL_NO_ECDSA) || defined(OPENSSL_NO_ECDH)
#  error OpenSSL was compiled without support for RSA, EC, ECDSA or ECDH
#endif


#include "Logging.h"
#include "OrthancException.h"
#include "SystemToolbox.h"

extern "C"
{
#  include <libp11/engine.h>  // This is P11's "engine.h"
#  include <libp11/libp11.h>
}

#include <openssl/engine.h>


namespace Orthanc
{
  namespace Pkcs11
  {
    static const char* PKCS11_ENGINE_ID = "pkcs11";
    static const char* PKCS11_ENGINE_NAME = "PKCS#11 for Orthanc";
    static const ENGINE_CMD_DEFN PKCS11_ENGINE_COMMANDS[] = 
    {
      { 
        CMD_MODULE_PATH,
        "MODULE_PATH",
        "Specifies the path to the PKCS#11 module shared library",
        ENGINE_CMD_FLAG_STRING
      },
      {
        CMD_PIN,
        "PIN",
        "Specifies the pin code",
        ENGINE_CMD_FLAG_STRING 
      },
      {
        CMD_VERBOSE,
        "VERBOSE",
        "Print additional details",
        ENGINE_CMD_FLAG_NO_INPUT 
      },
      {
        CMD_LOAD_CERT_CTRL,
        "LOAD_CERT_CTRL",
        "Get the certificate from card",
        ENGINE_CMD_FLAG_INTERNAL
      },
      {
        0,
        NULL, 
        NULL, 
        0
      }
    };


    static bool pkcs11Initialized_ = false;
    static ENGINE_CTX *context_ = NULL;

    static int EngineInitialize(ENGINE* engine)
    {
      if (context_ == NULL)
      {
        return 0;
      }
      else
      {
        return pkcs11_init(context_);
      }
    }


    static int EngineFinalize(ENGINE* engine)
    {
      if (context_ == NULL)
      {
        return 0;
      }
      else
      {
        return pkcs11_finish(context_);
      }
    }


    static int EngineDestroy(ENGINE* engine)
    {
      return (context_ == NULL ? 0 : 1);
    }


    static int EngineControl(ENGINE *engine, 
                             int command, 
                             long i, 
                             void *p, 
                             void (*f) ())
    {
      if (context_ == NULL)
      {
        return 0;
      }
      else
      {
        return pkcs11_engine_ctrl(context_, command, i, p, f);
      }
    }


    static EVP_PKEY *EngineLoadPublicKey(ENGINE *engine, 
                                         const char *s_key_id,
                                         UI_METHOD *ui_method, 
                                         void *callback_data)
    {
      if (context_ == NULL)
      {
        return 0;
      }
      else
      {
        return pkcs11_load_public_key(context_, s_key_id, ui_method, callback_data);
      }
    }


    static EVP_PKEY *EngineLoadPrivateKey(ENGINE *engine, 
                                          const char *s_key_id,
                                          UI_METHOD *ui_method, 
                                          void *callback_data)
    {
      if (context_ == NULL)
      {
        return 0;
      }
      else
      {
        return pkcs11_load_private_key(context_, s_key_id, ui_method, callback_data);
      }
    }


    static ENGINE* LoadEngine()
    {
      // This function creates an engine for PKCS#11 and inspired by
      // the "ENGINE_load_dynamic" function from OpenSSL, in file
      // "crypto/engine/eng_dyn.c"

      ENGINE* engine = ENGINE_new();
      if (!engine)
      {
        LOG(ERROR) << "Cannot create an OpenSSL engine for PKCS#11";
        throw OrthancException(ErrorCode_InternalError);
      }

      // Create a PKCS#11 context using libp11
      context_ = pkcs11_new();
      if (!context_)
      {
        LOG(ERROR) << "Cannot create a libp11 context for PKCS#11";
        ENGINE_free(engine);
        throw OrthancException(ErrorCode_InternalError);
      }

      if (!ENGINE_set_id(engine, PKCS11_ENGINE_ID) ||
          !ENGINE_set_name(engine, PKCS11_ENGINE_NAME) ||
          !ENGINE_set_cmd_defns(engine, PKCS11_ENGINE_COMMANDS) ||

          // Register the callback functions
          !ENGINE_set_init_function(engine, EngineInitialize) ||
          !ENGINE_set_finish_function(engine, EngineFinalize) ||
          !ENGINE_set_destroy_function(engine, EngineDestroy) ||
          !ENGINE_set_ctrl_function(engine, EngineControl) ||
          !ENGINE_set_load_pubkey_function(engine, EngineLoadPublicKey) ||
          !ENGINE_set_load_privkey_function(engine, EngineLoadPrivateKey) ||

          !ENGINE_set_RSA(engine, PKCS11_get_rsa_method()) ||
          !ENGINE_set_ECDSA(engine, PKCS11_get_ecdsa_method()) ||
          !ENGINE_set_ECDH(engine, PKCS11_get_ecdh_method()) ||

#if OPENSSL_VERSION_NUMBER  >= 0x10100002L
          !ENGINE_set_EC(engine, PKCS11_get_ec_key_method()) ||
#endif

          // Make OpenSSL know about our PKCS#11 engine
          !ENGINE_add(engine))
      {
        LOG(ERROR) << "Cannot initialize the OpenSSL engine for PKCS#11";
        pkcs11_finish(context_);
        ENGINE_free(engine);
        throw OrthancException(ErrorCode_InternalError);
      }

      // If the "ENGINE_add" worked, it gets a structural
      // reference. We release our just-created reference.
      ENGINE_free(engine);

      return ENGINE_by_id(PKCS11_ENGINE_ID);
    }


    bool IsInitialized()
    {
      return pkcs11Initialized_;
    }

    const char* GetEngineIdentifier()
    {
      return PKCS11_ENGINE_ID;
    }

    void Initialize(const std::string& module,
                    const std::string& pin,
                    bool verbose)
    {
      if (pkcs11Initialized_)
      {
        LOG(ERROR) << "The PKCS#11 engine has already been initialized";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      if (module.empty() ||
          !SystemToolbox::IsRegularFile(module))
      {
        LOG(ERROR) << "The PKCS#11 module must be a path to one shared library (DLL or .so)";
        throw OrthancException(ErrorCode_InexistentFile);
      }

      ENGINE* engine = LoadEngine();
      if (!engine)
      {
        LOG(ERROR) << "Cannot create an OpenSSL engine for PKCS#11";
        throw OrthancException(ErrorCode_InternalError);
      }

      if (!ENGINE_ctrl_cmd_string(engine, "MODULE_PATH", module.c_str(), 0))
      {
        LOG(ERROR) << "Cannot configure the OpenSSL dynamic engine for PKCS#11";
        throw OrthancException(ErrorCode_InternalError);
      }

      if (verbose)
      {
        ENGINE_ctrl_cmd_string(engine, "VERBOSE", NULL, 0);
      }

      if (!pin.empty() &&
          !ENGINE_ctrl_cmd_string(engine, "PIN", pin.c_str(), 0)) 
      {
        LOG(ERROR) << "Cannot set the PIN code for PKCS#11";
        throw OrthancException(ErrorCode_InternalError);
      }
  
      if (!ENGINE_init(engine))
      {
        LOG(ERROR) << "Cannot initialize the OpenSSL dynamic engine for PKCS#11";
        throw OrthancException(ErrorCode_InternalError);
      }

      LOG(WARNING) << "The PKCS#11 engine has been successfully initialized";
      pkcs11Initialized_ = true;
    }


    void Finalize()
    {
      // Nothing to do, the unregistration of the engine is
      // automatically done by OpenSSL
    }
  }
}
