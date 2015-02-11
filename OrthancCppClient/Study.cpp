/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "../Core/PrecompiledHeaders.h"
#include "Study.h"

#include "OrthancConnection.h"

namespace OrthancClient
{
  void Study::ReadStudy()
  {
    Orthanc::HttpClient client(connection_.GetHttpClient());
    client.SetUrl(std::string(connection_.GetOrthancUrl()) + "/studies/" + id_);

    Json::Value v;
    if (!client.Apply(study_))
    {
      throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
    }
  }

  Orthanc::IDynamicObject* Study::GetFillerItem(size_t index)
  {
    Json::Value::ArrayIndex tmp = static_cast<Json::Value::ArrayIndex>(index);
    std::string id = study_["Series"][tmp].asString();
    return new Series(connection_, id.c_str());
  }

  Study::Study(const OrthancConnection& connection,
               const char* id) :
    connection_(connection),
    id_(id),
    series_(*this)
  {
    series_.SetThreadCount(connection.GetThreadCount());
    ReadStudy();
  }

  const char* Study::GetMainDicomTag(const char* tag, const char* defaultValue) const
  {
    if (study_["MainDicomTags"].isMember(tag))
    {
      return study_["MainDicomTags"][tag].asCString();
    }
    else
    {
      return defaultValue;
    }
  }
}
