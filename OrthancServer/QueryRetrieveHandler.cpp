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


#include "PrecompiledHeadersServer.h"
#include "QueryRetrieveHandler.h"

#include "OrthancInitialization.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"


namespace Orthanc
{
  static void FixQuery(DicomMap& query,
                       ServerContext& context,
                       const std::string& modality)
  {
    static const char* LUA_CALLBACK = "OutgoingFindRequestFilter";

    LuaScripting::Locker locker(context.GetLua());
    if (locker.GetLua().IsExistingFunction(LUA_CALLBACK))
    {
      LuaFunctionCall call(locker.GetLua(), LUA_CALLBACK);
      call.PushDicom(query);
      call.PushJson(modality);
      FromDcmtkBridge::ExecuteToDicom(query, call);
    }
  }


  static void FixQuery(DicomMap& query,
                       ModalityManufacturer manufacturer)
  {
    /**
     * Introduce patches for specific manufacturers below.
     **/

    switch (manufacturer)
    {
      default:
        break;
    }
  }


  void QueryRetrieveHandler::Invalidate()
  {
    done_ = false;
    answers_.Clear();
  }


  void QueryRetrieveHandler::Run()
  {
    if (!done_)
    {
      // Firstly, fix the content of the query for specific manufacturers
      DicomMap fixed;
      fixed.Assign(query_);
      FixQuery(fixed, modality_.GetManufacturer());

      // Secondly, possibly fix the query with the user-provider Lua callback
      FixQuery(fixed, context_, modality_.GetApplicationEntityTitle()); 

      {
        // Finally, run the C-FIND SCU against the fixed query
        ReusableDicomUserConnection::Locker locker(context_.GetReusableDicomUserConnection(), localAet_, modality_);
        locker.GetConnection().Find(answers_, level_, fixed);
      }

      done_ = true;
    }
  }


  QueryRetrieveHandler::QueryRetrieveHandler(ServerContext& context) : 
    context_(context),
    localAet_(context.GetDefaultLocalApplicationEntityTitle()),
    done_(false),
    level_(ResourceType_Study),
    answers_(false)
  {
  }


  void QueryRetrieveHandler::SetModality(const std::string& symbolicName)
  {
    Invalidate();
    modalityName_ = symbolicName;
    Configuration::GetDicomModalityUsingSymbolicName(modality_, symbolicName);
  }


  void QueryRetrieveHandler::SetLevel(ResourceType level)
  {
    Invalidate();
    level_ = level;
  }


  void QueryRetrieveHandler::SetQuery(const DicomTag& tag,
                                      const std::string& value)
  {
    Invalidate();
    query_.SetValue(tag, value, false);
  }


  size_t QueryRetrieveHandler::GetAnswerCount()
  {
    Run();
    return answers_.GetSize();
  }


  void QueryRetrieveHandler::GetAnswer(DicomMap& target,
                                       size_t i)
  {
    Run();
    answers_.GetAnswer(i).ExtractDicomSummary(target);
  }


  void QueryRetrieveHandler::Retrieve(const std::string& target,
                                      size_t i)
  {
    DicomMap map;
    GetAnswer(map, i);

    {
      ReusableDicomUserConnection::Locker locker(context_.GetReusableDicomUserConnection(), localAet_, modality_);
      locker.GetConnection().Move(target, map);
    }
  }


  void QueryRetrieveHandler::Retrieve(const std::string& target)
  {
    for (size_t i = 0; i < GetAnswerCount(); i++)
    {
      Retrieve(target, i);
    }
  }
}
