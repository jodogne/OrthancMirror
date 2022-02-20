/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "OrthancConfiguration.h"

#include "../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Lua/LuaFunctionCall.h"
#include "LuaScripting.h"
#include "ServerContext.h"


namespace Orthanc
{
  static void FixQueryLua(DicomMap& query,
                          ServerContext& context,
                          const std::string& modality)
  {
    static const char* LUA_CALLBACK = "OutgoingFindRequestFilter";

    LuaScripting::Lock lock(context.GetLuaScripting());

    if (lock.GetLua().IsExistingFunction(LUA_CALLBACK))
    {
      LuaFunctionCall call(lock.GetLua(), LUA_CALLBACK);
      call.PushDicom(query);
      call.PushJson(modality);
      call.ExecuteToDicom(query);
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

      // Secondly, possibly fix the query with the user-provider Lua callback
      FixQueryLua(fixed, context_, modality_.GetApplicationEntityTitle()); 

      {
        DicomAssociationParameters params(localAet_, modality_);

        if (timeout_ != 0)
        {
          params.SetTimeout(timeout_);
        }
        
        DicomControlUserConnection connection(params);
        connection.Find(answers_, level_, fixed, findNormalized_);
      }

      done_ = true;
    }
  }


  QueryRetrieveHandler::QueryRetrieveHandler(ServerContext& context) : 
    context_(context),
    localAet_(context.GetDefaultLocalApplicationEntityTitle()),
    done_(false),
    level_(ResourceType_Study),
    answers_(false),
    findNormalized_(true),
    timeout_(0)
  {
  }


  void QueryRetrieveHandler::SetModality(const std::string& symbolicName)
  {
    Invalidate();
    modalityName_ = symbolicName;

    {
      OrthancConfiguration::ReaderLock lock;
      lock.GetConfiguration().GetDicomModalityUsingSymbolicName(modality_, symbolicName);
    }
  }


  void QueryRetrieveHandler::SetLocalAet(const std::string& localAet)
  {
    Invalidate();
    localAet_ = localAet;
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


  void QueryRetrieveHandler::CopyStringTag(const DicomMap& from,
                                           const DicomTag& tag)
  {
    const DicomValue* value = from.TestAndGetValue(tag);

    if (value == NULL ||
        value->IsNull() ||
        value->IsBinary())
    {
      throw OrthancException(ErrorCode_InexistentTag);
    }
    else
    {
      SetQuery(tag, value->GetContent());
    }
  }


  size_t QueryRetrieveHandler::GetAnswersCount()
  {
    Run();
    return answers_.GetSize();
  }


  void QueryRetrieveHandler::GetAnswer(DicomMap& target,
                                       size_t i)
  {
    Run();
    answers_.GetAnswer(i).ExtractDicomSummary(target, 0 /* don't truncate tags */);
  }

  
  void QueryRetrieveHandler::SetFindNormalized(bool normalized)
  {
    Invalidate();
    findNormalized_ = normalized;
  }
}
