/**
 * Laaw - Lightweight, Automated API Wrapper
 * Copyright (C) 2010-2013 Jomago - Alain Mazy, Benjamin Golinvaux,
 * Sebastien Jodogne
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


#include <laaw/laaw.h>
#include <string.h>  // For strcpy() and strlen()
#include <stdlib.h>  // For free()

static char* LAAW_EXTERNC_CopyString(const char* str)
{
  char* copy = reinterpret_cast<char*>(malloc(strlen(str) + 1));
  strcpy(copy, str);
  return copy;
}

extern "C"
{
      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_1f1acb322ea4d0aad65172824607673c(void** newObject, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::OrthancConnection(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_f3fd272e4636f6a531aabb72ee01cd5b(void** newObject, const char* arg0, const char* arg1, const char* arg2)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::OrthancConnection(reinterpret_cast< const char* >(arg0), reinterpret_cast< const char* >(arg1), reinterpret_cast< const char* >(arg2));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_12d3de0a96e9efb11136a9811bb9ed38(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          delete static_cast<OrthancClient::OrthancConnection*>(thisObject);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_557aee7b61817292a0f31269d3c35db7(const void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::OrthancConnection* this_ = static_cast<const OrthancClient::OrthancConnection*>(thisObject);
*result = this_->GetThreadCount();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_0b8dff0ce67f10954a49b059e348837e(void* thisObject, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
this_->SetThreadCount(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_e05097c153f676e5a5ee54dcfc78256f(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
this_->Reload();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_e840242bf58d17d3c1d722da09ce88e0(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::OrthancConnection* this_ = static_cast<const OrthancClient::OrthancConnection*>(thisObject);
*result = this_->GetOrthancUrl();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_c9af31433001b5dfc012a552dc6d0050(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
*result = this_->GetPatientCount();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_3fba4d6b818180a44cd1cae6046334dc(void* thisObject, void** result, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
*result = &this_->GetPatient(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_aeb20dc75b9246188db857317e5e0ce7(void* thisObject, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
this_->DeletePatient(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_62689803d9871e4d9c51a648640b320b(void* thisObject, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
this_->StoreFile(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_2fb64c9e5a67eccd413b0e913469a421(void* thisObject, const void* arg0, uint64_t arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::OrthancConnection* this_ = static_cast<OrthancClient::OrthancConnection*>(thisObject);
this_->Store(reinterpret_cast< const void* >(arg0), arg1);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_6cf0d7268667f9b0aa4511bacf184919(void** newObject, void* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::Patient(*reinterpret_cast< ::OrthancClient::OrthancConnection* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_7d81cd502ee27e859735d0ea7112b5a1(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          delete static_cast<OrthancClient::Patient*>(thisObject);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_f756172daf04516eec3a566adabb4335(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Patient* this_ = static_cast<OrthancClient::Patient*>(thisObject);
this_->Reload();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_ddb68763ec902a97d579666a73a20118(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Patient* this_ = static_cast<OrthancClient::Patient*>(thisObject);
*result = this_->GetStudyCount();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_fba3c68b4be7558dbc65f7ce1ab57d63(void* thisObject, void** result, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Patient* this_ = static_cast<OrthancClient::Patient*>(thisObject);
*result = &this_->GetStudy(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_b4ca99d958f843493e58d1ef967340e1(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Patient* this_ = static_cast<const OrthancClient::Patient*>(thisObject);
*result = this_->GetId();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_78d5cc76d282437b6f93ec3b82c35701(const void* thisObject, const char** result, const char* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Patient* this_ = static_cast<const OrthancClient::Patient*>(thisObject);
*result = this_->GetMainDicomTag(reinterpret_cast< const char* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_193599b9e345384fcdfcd47c29c55342(void** newObject, void* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::Series(*reinterpret_cast< ::OrthancClient::OrthancConnection* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_7c97f17063a357d38c5fab1136ad12a0(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          delete static_cast<OrthancClient::Series*>(thisObject);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_48a2a1a9d68c047e22bfba23014643d2(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
this_->Reload();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_852bf8296ca21c5fde5ec565cc10721d(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetInstanceCount();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_efd04574e0779faa83df1f2d8f9888db(void* thisObject, void** result, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = &this_->GetInstance(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_736247ff5e8036dac38163da6f666ed5(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Series* this_ = static_cast<const OrthancClient::Series*>(thisObject);
*result = this_->GetId();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_d82d2598a7a73f4b6fcc0c09c25b08ca(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Series* this_ = static_cast<const OrthancClient::Series*>(thisObject);
*result = this_->GetUrl();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_88134b978f9acb2aecdadf54aeab3c64(const void* thisObject, const char** result, const char* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Series* this_ = static_cast<const OrthancClient::Series*>(thisObject);
*result = this_->GetMainDicomTag(reinterpret_cast< const char* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_152cb1b704c053d24b0dab7461ba6ea3(void* thisObject, int32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->Is3DImage();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_eee03f337ec81d9f1783cd41e5238757(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetWidth();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_006f08237bd7611636fc721baebfb4c5(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetHeight();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_b794f5cd3dad7d7b575dd1fd902afdd0(void* thisObject, float* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetVoxelSizeX();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_8ee2e50dd9df8f66a3c1766090dd03ab(void* thisObject, float* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetVoxelSizeY();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_046aed35bbe4751691f4c34cc249a61d(void* thisObject, float* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetVoxelSizeZ();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_2be452e7af5bf7dfd8c5021842674497(void* thisObject, float* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
*result = this_->GetSliceThickness();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_4dcc7a0fd025efba251ac6e9b701c2c5(void* thisObject, void* arg0, int32_t arg1, int64_t arg2, int64_t arg3)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
this_->Load3DImage(reinterpret_cast< void* >(arg0), static_cast< ::Orthanc::PixelFormat >(arg1), arg2, arg3);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_b2601a161c24ad0a1d3586246f87452c(void* thisObject, void* arg0, int32_t arg1, int64_t arg2, int64_t arg3, float* arg4)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Series* this_ = static_cast<OrthancClient::Series*>(thisObject);
this_->Load3DImage(reinterpret_cast< void* >(arg0), static_cast< ::Orthanc::PixelFormat >(arg1), arg2, arg3, reinterpret_cast< float* >(arg4));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_b01c6003238eb46c8db5dc823d7ca678(void** newObject, void* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::Study(*reinterpret_cast< ::OrthancClient::OrthancConnection* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_0147007fb99bad8cd95a139ec8795376(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          delete static_cast<OrthancClient::Study*>(thisObject);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_e65b20b7e0170b67544cd6664a4639b7(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Study* this_ = static_cast<OrthancClient::Study*>(thisObject);
this_->Reload();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_470e981b0e41f17231ba0ae6f3033321(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Study* this_ = static_cast<OrthancClient::Study*>(thisObject);
*result = this_->GetSeriesCount();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_04cefd138b6ea15ad909858f2a0a8f05(void* thisObject, void** result, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Study* this_ = static_cast<OrthancClient::Study*>(thisObject);
*result = &this_->GetSeries(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_aee5b1f6f0c082f2c3b0986f9f6a18c7(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Study* this_ = static_cast<const OrthancClient::Study*>(thisObject);
*result = this_->GetId();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_93965682bace75491413e1f0b8d5a654(const void* thisObject, const char** result, const char* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Study* this_ = static_cast<const OrthancClient::Study*>(thisObject);
*result = this_->GetMainDicomTag(reinterpret_cast< const char* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_6c5ad02f91b583e29cebd0bd319ce21d(void** newObject, void* arg0, const char* arg1)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          *newObject = new OrthancClient::Instance(*reinterpret_cast< ::OrthancClient::OrthancConnection* >(arg0), reinterpret_cast< const char* >(arg1));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_4068241c44a9c1367fe0e57be523f207(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          delete static_cast<OrthancClient::Instance*>(thisObject);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_236ee8b403bc99535a8a4695c0cd45cb(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetId();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_2a437b7aba6bb01e81113835be8f0146(void* thisObject, int32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
this_->SetImageExtractionMode(static_cast< ::Orthanc::ImageExtractionMode >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_2bcbcb850934ae0bb4c6f0cc940e6cda(const void* thisObject, int32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetImageExtractionMode();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_8d415c3a78a48e7e61d9fd24e7c79484(const void* thisObject, const char** result, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetTagAsString(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_70d2f8398bbc63b5f792b69b4ad5fecb(const void* thisObject, float* result, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetTagAsFloat(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_1729a067d902771517388eedd7346b23(const void* thisObject, int32_t* result, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetTagAsInt(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_72e2aeee66cd3abd8ab7e987321c3745(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetWidth();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_1ea3df5a1ac1a1a687fe7325adddb6f0(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetHeight();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_99b4f370e4f532d8b763e2cb49db92f8(void* thisObject, uint32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetPitch();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_c41c742b68617f1c0590577a0a5ebc0c(void* thisObject, int32_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetPixelFormat();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_142dd2feba0fc1d262bbd0baeb441a8b(void* thisObject, const void** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetBuffer();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_5f5c9f81a4dff8daa6c359f1d0488fef(void* thisObject, const void** result, uint32_t arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetBuffer(arg0);

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_9ca979fffd08fa256306d4e68d8b0e91(void* thisObject, uint64_t* result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetDicomSize();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_6f2d77a26edc91c28d89408dbc3c271e(void* thisObject, const void** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
*result = this_->GetDicom();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_c0f494b80d4ff8b232df7a75baa0700a(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
this_->DiscardImage();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_d604f44bd5195e082e745e9cbc164f4c(void* thisObject)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
this_->DiscardDicom();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_1710299d1c5f3b1f2b7cf3962deebbfd(void* thisObject, const char* arg0)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          OrthancClient::Instance* this_ = static_cast<OrthancClient::Instance*>(thisObject);
this_->LoadTagContent(reinterpret_cast< const char* >(arg0));

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }

      LAAW_EXPORT_DLL_API char* LAAW_CALL_CONVENTION LAAW_EXTERNC_bb55aaf772ddceaadee36f4e54136bcb(const void* thisObject, const char** result)
      {
        try
        {
          #ifdef LAAW_EXTERNC_START_FUNCTION
          LAAW_EXTERNC_START_FUNCTION;
          #endif

          const OrthancClient::Instance* this_ = static_cast<const OrthancClient::Instance*>(thisObject);
*result = this_->GetLoadedTagContent();

          return NULL;
        }
        catch (::Laaw::LaawException& e)
        {
          return LAAW_EXTERNC_CopyString(e.What());
        }
        catch (...)
        {
          return LAAW_EXTERNC_CopyString("...");
        }
      }


  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetDescription()
  {
    return "Native client to the REST API of Orthanc";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetCompany()
  {
    return "University Hospital of Liege";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetProduct()
  {
    return "OrthancClient";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetCopyright()
  {
    return "(c) 2012-2015, Sebastien Jodogne, University Hospital of Liege";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetVersion()
  {
    return "0.8";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetFileVersion()
  {
    return "0.8.0.6";
  }

  LAAW_EXPORT_DLL_API const char* LAAW_CALL_CONVENTION LAAW_EXTERNC_GetFullVersion()
  {
    return "0.8.6";
  }

  LAAW_EXPORT_DLL_API void LAAW_CALL_CONVENTION LAAW_EXTERNC_FreeString(char* str)
  {
    if (str != NULL)
      free(str);
  }
}
