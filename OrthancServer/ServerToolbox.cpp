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


#include "PrecompiledHeadersServer.h"
#include "ServerToolbox.h"

#include "../Core/Logging.h"
#include "../Core/OrthancException.h"

#include <cassert>

namespace Orthanc
{
  void SimplifyTags(Json::Value& target,
                    const Json::Value& source)
  {
    assert(source.isObject());

    target = Json::objectValue;
    Json::Value::Members members = source.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& v = source[members[i]];
      const std::string& name = v["Name"].asString();
      const std::string& type = v["Type"].asString();

      if (type == "String")
      {
        target[name] = v["Value"].asString();
      }
      else if (type == "TooLong" ||
               type == "Null")
      {
        target[name] = Json::nullValue;
      }
      else if (type == "Sequence")
      {
        const Json::Value& array = v["Value"];
        assert(array.isArray());

        Json::Value children = Json::arrayValue;
        for (Json::Value::ArrayIndex i = 0; i < array.size(); i++)
        {
          Json::Value c;
          SimplifyTags(c, array[i]);
          children.append(c);
        }

        target[name] = children;
      }
      else
      {
        assert(0);
      }
    }
  }


  void LogMissingRequiredTag(const DicomMap& summary)
  {
    std::string s, t;

    if (summary.HasTag(DICOM_TAG_PATIENT_ID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "PatientID=" + summary.GetValue(DICOM_TAG_PATIENT_ID).AsString();
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "PatientID";
    }

    if (summary.HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "StudyInstanceUID=" + summary.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).AsString();
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "StudyInstanceUID";
    }

    if (summary.HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "SeriesInstanceUID=" + summary.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).AsString();
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "SeriesInstanceUID";
    }

    if (summary.HasTag(DICOM_TAG_SOP_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "SOPInstanceUID=" + summary.GetValue(DICOM_TAG_SOP_INSTANCE_UID).AsString();
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "SOPInstanceUID";
    }

    if (t.size() == 0)
    {
      LOG(ERROR) << "Store has failed because all the required tags (" << s << ") are missing (is it a DICOMDIR file?)";
    }
    else
    {
      LOG(ERROR) << "Store has failed because required tags (" << s << ") are missing for the following instance: " << t;
    }
  }
}
