/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../ServerContext.h"

namespace Orthanc
{
  // Changes API --------------------------------------------------------------
 
  static void GetSinceAndLimit(int64_t& since,
                               unsigned int& limit,
                               bool& last,
                               const RestApiGetCall& call)
  {
    static const unsigned int DEFAULT_LIMIT = 100;
    
    if (call.HasArgument("last"))
    {
      last = true;
      return;
    }

    last = false;

    try
    {
      since = boost::lexical_cast<int64_t>(call.GetArgument("since", "0"));
      limit = boost::lexical_cast<unsigned int>(call.GetArgument("limit", boost::lexical_cast<std::string>(DEFAULT_LIMIT)));
    }
    catch (boost::bad_lexical_cast&)
    {
      since = 0;
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
        .SetDescription("Whenever Orthanc receives a new DICOM instance, this event is recorded in the so-called _Changes Log_. This enables remote scripts to react to the arrival of new DICOM resources. A typical application is auto-routing, where an external script waits for a new DICOM instance to arrive into Orthanc, then forward this instance to another modality.")
        .SetHttpGetArgument("limit", RestApiCallDocumentation::Type_Number, "Limit the number of results", false)
        .SetHttpGetArgument("since", RestApiCallDocumentation::Type_Number, "Show only the resources since the provided index", false)
        .AddAnswerType(MimeType_Json, "The list of changes")
        .SetAnswerField("Changes", RestApiCallDocumentation::Type_JsonListOfObjects, "The individual changes")
        .SetAnswerField("Done", RestApiCallDocumentation::Type_Boolean,
                        "Whether the last reported change is the last of the full history")
        .SetAnswerField("Last", RestApiCallDocumentation::Type_Number,
                        "The index of the last reported change, can be used for the `since` argument in subsequent calls to this route")
        .SetHttpGetSample("https://demo.orthanc-server.com/changes?since=0&limit=2", true);
      return;
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    //std::string filter = GetArgument(getArguments, "filter", "");
    int64_t since;
    unsigned int limit;
    bool last;
    GetSinceAndLimit(since, limit, last, call);

    Json::Value result;
    if (last)
    {
      context.GetIndex().GetLastChange(result);
    }
    else
    {
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

    int64_t since;
    unsigned int limit;
    bool last;
    GetSinceAndLimit(since, limit, last, call);

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
