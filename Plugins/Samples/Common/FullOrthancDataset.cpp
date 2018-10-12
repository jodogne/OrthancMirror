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


#include "FullOrthancDataset.h"

#include "OrthancPluginException.h"

#include <stdio.h>
#include <cassert>

namespace OrthancPlugins
{
  static const Json::Value* AccessTag(const Json::Value& dataset,
                                      const DicomTag& tag) 
  {
    if (dataset.type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    char name[16];
    sprintf(name, "%04x,%04x", tag.GetGroup(), tag.GetElement());

    if (!dataset.isMember(name))
    {
      return NULL;
    }

    const Json::Value& value = dataset[name];
    if (value.type() != Json::objectValue ||
        !value.isMember("Name") ||
        !value.isMember("Type") ||
        !value.isMember("Value") ||
        value["Name"].type() != Json::stringValue ||
        value["Type"].type() != Json::stringValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    return &value;
  }


  static const Json::Value& GetSequenceContent(const Json::Value& sequence)
  {
    assert(sequence.type() == Json::objectValue);
    assert(sequence.isMember("Type"));
    assert(sequence.isMember("Value"));

    const Json::Value& value = sequence["Value"];
      
    if (sequence["Type"].asString() != "Sequence" ||
        value.type() != Json::arrayValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    else
    {
      return value;
    }
  }


  static bool GetStringInternal(std::string& result,
                                const Json::Value& tag)
  {
    assert(tag.type() == Json::objectValue);
    assert(tag.isMember("Type"));
    assert(tag.isMember("Value"));

    const Json::Value& value = tag["Value"];
      
    if (tag["Type"].asString() != "String" ||
        value.type() != Json::stringValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    else
    {
      result = value.asString();
      return true;
    }
  }


  const Json::Value* FullOrthancDataset::LookupPath(const DicomPath& path) const
  {
    const Json::Value* content = &root_;
                                  
    for (unsigned int depth = 0; depth < path.GetPrefixLength(); depth++)
    {
      const Json::Value* sequence = AccessTag(*content, path.GetPrefixTag(depth));
      if (sequence == NULL)
      {
        return NULL;
      }

      const Json::Value& nextContent = GetSequenceContent(*sequence);

      size_t index = path.GetPrefixIndex(depth);
      if (index >= nextContent.size())
      {
        return NULL;
      }
      else
      {
        content = &nextContent[static_cast<Json::Value::ArrayIndex>(index)];
      }
    }

    return AccessTag(*content, path.GetFinalTag());
  }


  void FullOrthancDataset::CheckRoot() const
  {
    if (root_.type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }


  FullOrthancDataset::FullOrthancDataset(IOrthancConnection& orthanc,
                                         const std::string& uri)
  {
    IOrthancConnection::RestApiGet(root_, orthanc, uri);
    CheckRoot();
  }


  FullOrthancDataset::FullOrthancDataset(const std::string& content)
  {
    IOrthancConnection::ParseJson(root_, content);
    CheckRoot();
  }


  FullOrthancDataset::FullOrthancDataset(const void* content,
                                         size_t size)
  {
    IOrthancConnection::ParseJson(root_, content, size);
    CheckRoot();
  }


  FullOrthancDataset::FullOrthancDataset(const Json::Value& root) :
    root_(root)
  {
    CheckRoot();
  }


  bool FullOrthancDataset::GetStringValue(std::string& result,
                                          const DicomPath& path) const
  {
    const Json::Value* value = LookupPath(path);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return GetStringInternal(result, *value);
    }
  }


  bool FullOrthancDataset::GetSequenceSize(size_t& size,
                                           const DicomPath& path) const
  {
    const Json::Value* sequence = LookupPath(path);

    if (sequence == NULL)
    {
      return false;
    }
    else
    {
      size = GetSequenceContent(*sequence).size();
      return true;
    }
  }
}
