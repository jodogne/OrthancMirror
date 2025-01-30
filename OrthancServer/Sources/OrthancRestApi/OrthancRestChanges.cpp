/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../ServerContext.h"

namespace Orthanc
{
  // Changes API --------------------------------------------------------------
  static const unsigned int DEFAULT_LIMIT = 100;
  static const int64_t DEFAULT_TO = -1;
 
  static void GetSinceToAndLimit(int64_t& since,
                                 int64_t& to,
                                 unsigned int& limit,
                                 bool& last,
                                 const RestApiGetCall& call)
  {
    if (call.HasArgument("last"))
    {
      last = true;
      return;
    }

    last = false;

    try
    {
      since = boost::lexical_cast<int64_t>(call.GetArgument("since", "0"));
      to = boost::lexical_cast<int64_t>(call.GetArgument("to", boost::lexical_cast<std::string>(DEFAULT_TO)));
      limit = boost::lexical_cast<unsigned int>(call.GetArgument("limit", boost::lexical_cast<std::string>(DEFAULT_LIMIT)));
    }
    catch (boost::bad_lexical_cast&)
    {
      since = 0;
      to = DEFAULT_TO;
      limit = DEFAULT_LIMIT;
      return;
    }
  }

  static void GetChanges(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Tracking changes")
        .SetSummary("List changes")
        .SetDescription("Whenever Orthanc receives a new DICOM instance, this event is recorded in the so-called _Changes Log_. This enables remote scripts to react to the arrival of new DICOM resources. A typical application is auto-routing, where an external script waits for a new DICOM instance to arrive into Orthanc, then forward this instance to another modality. Please note that, when resources are deleted, their corresponding change entries are also removed from the Changes Log, which helps ensuring that this log does not grow indefinitely.")
        .SetHttpGetArgument("last", RestApiCallDocumentation::Type_Number, "Request only the last change id (this argument must be used alone)", false)
        .SetHttpGetArgument("limit", RestApiCallDocumentation::Type_Number, "Limit the number of results", false)
        .SetHttpGetArgument("since", RestApiCallDocumentation::Type_Number, "Show only the resources since the provided index excluded", false)
        .SetHttpGetArgument("to", RestApiCallDocumentation::Type_Number, "Show only the resources till the provided index included (only available if your DB backend supports ExtendedChanges)", false)
        .SetHttpGetArgument("type", RestApiCallDocumentation::Type_String, "Show only the changes of the provided type (only available if your DB backend supports ExtendedChanges).  Multiple values can be provided and must be separated by a ';'.", false)
        .AddAnswerType(MimeType_Json, "The list of changes")
        .SetAnswerField("Changes", RestApiCallDocumentation::Type_JsonListOfObjects, "The individual changes")
        .SetAnswerField("Done", RestApiCallDocumentation::Type_Boolean,
                        "Whether the last reported change is the last of the full history.")
        .SetAnswerField("Last", RestApiCallDocumentation::Type_Number,
                        "The index of the last reported change, can be used for the `since` argument in subsequent calls to this route")
        .SetAnswerField("First", RestApiCallDocumentation::Type_Number,
                        "The index of the first reported change, its value-1 can be used for the `to` argument in subsequent calls to this route when browsing the changes in reverse order")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/changes?since=0&limit=2", true);
      return;
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    int64_t since, to;
    std::set<ChangeType> filterType;

    unsigned int limit;
    bool last;
    GetSinceToAndLimit(since, to, limit, last, call);

    std::string filterArgument = call.GetArgument("type", "all");
    if (filterArgument != "all" && filterArgument != "All")
    {
      std::set<std::string> filterTypeStrings;
      Toolbox::SplitString(filterTypeStrings, filterArgument, ';');

      for (std::set<std::string>::const_iterator it = filterTypeStrings.begin(); it != filterTypeStrings.end(); ++it)
      {
        filterType.insert(StringToChangeType(*it));
      }
    }

    Json::Value result;
    if (last)
    {
      context.GetIndex().GetLastChange(result);
    }
    else if (context.GetIndex().HasExtendedChanges())
    {
      context.GetIndex().GetChangesExtended(result, since, to, limit, filterType);
    }
    else
    {
      if (filterType.size() > 0)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "CAPABILITIES: Trying to filter changes while the Database backend does not support it (requires a DB backend with support for ExtendedChanges)");
      }

      if (to != DEFAULT_TO)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "CAPABILITIES: Trying to use the 'to' parameter in /changes while the Database backend does not support it (requires a DB backend with support for ExtendedChanges)");
      }

      context.GetIndex().GetChanges(result, since, limit);
    }

    call.GetOutput().AnswerJson(result);
  }


  static void DeleteChanges(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Tracking changes")
        .SetSummary("Clear changes")
        .SetDescription("Clear the full history stored in the changes log");
      return;
    }

    OrthancRestApi::GetIndex(call).DeleteChanges();
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  // Exports API --------------------------------------------------------------
 
  static void GetExports(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Tracking changes")
        .SetSummary("List exports")
        .SetDescription("For medical traceability, Orthanc can be configured to store a log of all the resources "
                        "that have been exported to remote modalities. In auto-routing scenarios, it is important "
                        "to prevent this log to grow indefinitely as incoming instances are routed. You can either "
                        "disable this logging by setting the option `LogExportedResources` to `false` in the "
                        "configuration file, or periodically clear this log by `DELETE`-ing this URI. This route "
                        "might be removed in future versions of Orthanc.")
        .SetHttpGetArgument("limit", RestApiCallDocumentation::Type_Number, "Limit the number of results", false)
        .SetHttpGetArgument("since", RestApiCallDocumentation::Type_Number, "Show only the resources since the provided index", false)
        .AddAnswerType(MimeType_Json, "The list of exports");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    int64_t since, to;
    unsigned int limit;
    bool last;
    GetSinceToAndLimit(since, to, limit, last, call);

    Json::Value result;
    if (last)
    {
      context.GetIndex().GetLastExportedResource(result);
    }
    else
    {
      context.GetIndex().GetExportedResources(result, since, limit);
    }

    call.GetOutput().AnswerJson(result);
  }


  static void DeleteExports(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Tracking changes")
        .SetSummary("Clear exports")
        .SetDescription("Clear the full history stored in the exports log");
      return;
    }

    OrthancRestApi::GetIndex(call).DeleteExportedResources();
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }
  

  void OrthancRestApi::RegisterChanges()
  {
    Register("/changes", GetChanges);
    Register("/changes", DeleteChanges);
    Register("/exports", GetExports);
    Register("/exports", DeleteExports);
  }
}
