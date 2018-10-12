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


#include "SimplifiedOrthancDataset.h"

#include "OrthancPluginException.h"

namespace OrthancPlugins
{
  const Json::Value* SimplifiedOrthancDataset::LookupPath(const DicomPath& path) const
  {
    const Json::Value* content = &root_;
                                  
    for (unsigned int depth = 0; depth < path.GetPrefixLength(); depth++)
    {
      const char* name = path.GetPrefixTag(depth).GetName();
      if (content->type() != Json::objectValue)
      {
        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
      }

      if (!content->isMember(name))
      {
        return NULL;
      }

      const Json::Value& sequence = (*content) [name];
      if (sequence.type() != Json::arrayValue)
      {
        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
      }

      size_t index = path.GetPrefixIndex(depth);
      if (index >= sequence.size())
      {
        return NULL;
      }
      else
      {
        content = &sequence[static_cast<Json::Value::ArrayIndex>(index)];
      }
    }

    const char* name = path.GetFinalTag().GetName();

    if (content->type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    if (!content->isMember(name))
    {
      return NULL;
    }
    else
    {
      return &((*content) [name]);
    }
  }


  void SimplifiedOrthancDataset::CheckRoot() const
  {
    if (root_.type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }


  SimplifiedOrthancDataset::SimplifiedOrthancDataset(IOrthancConnection& orthanc,
                                                     const std::string& uri)
  {
    IOrthancConnection::RestApiGet(root_, orthanc, uri);
    CheckRoot();
  }


  SimplifiedOrthancDataset::SimplifiedOrthancDataset(const std::string& content)
  {
    IOrthancConnection::ParseJson(root_, content);
    CheckRoot();
  }


  bool SimplifiedOrthancDataset::GetStringValue(std::string& result,
                                                const DicomPath& path) const
  {
    const Json::Value* value = LookupPath(path);

    if (value == NULL)
    {
      return false;
    }
    else if (value->type() != Json::stringValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    else
    {
      result = value->asString();
      return true;
    }
  }


  bool SimplifiedOrthancDataset::GetSequenceSize(size_t& size,
                                                 const DicomPath& path) const
  {
    const Json::Value* sequence = LookupPath(path);

    if (sequence == NULL)
    {
      // Inexistent path
      return false;
    }
    else if (sequence->type() != Json::arrayValue)
    {
      // Not a sequence
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    else
    {
      size = sequence->size();
      return true;
    }
  }
}
