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


#pragma once

#include "ServerIndex.h"

#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ExactResourceFinder : public boost::noncopyable
  {
  public:
    class IMainTagsFilter : public boost::noncopyable
    {
    public:
      virtual ~IMainTagsFilter()
      {
      }

      bool Apply(const DicomMap& mainTags,
                 ResourceType level);
    };


    class IInstanceFilter : public boost::noncopyable
    {
    public:
      virtual ~IInstanceFilter()
      {
      }

      bool Apply(const std::string& instanceId,
                 const Json::Value& content);
    };


  private:
    typedef std::map<DicomTag, std::string>  Identifiers;

    class CandidateResources;

    ServerContext&    context_;
    ResourceType      level_;
    size_t            maxResults_;
    Identifiers       identifiers_;
    IMainTagsFilter  *mainTagsFilter_;
    IInstanceFilter  *instanceFilter_;

    void ApplyAtLevel(CandidateResources& candidates,
                      ResourceType level);

  public:
    ExactResourceFinder(ServerContext& context);

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetLevel(ResourceType level)
    {
      level_ = level;
    }

    void SetIdentifier(const DicomTag& tag,
                       const std::string& value);

    void SetMainTagsFilter(IMainTagsFilter& filter)
    {
      mainTagsFilter_ = &filter;
    }

    void SetInstanceFilter(IInstanceFilter& filter)
    {
      instanceFilter_ = &filter;
    }

    void SetMaxResults(size_t value)
    {
      maxResults_ = value;
    }

    size_t GetMaxResults() const
    {
      return maxResults_;
    }

    // Returns "true" iff. all the matching resources have been
    // returned. Will be "false" if the results were truncated by
    // "SetMaxResults()".
    bool Apply(std::list<std::string>& result);
  };

}
