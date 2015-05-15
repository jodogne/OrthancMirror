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
  class ResourceFinder : public boost::noncopyable
  {
  public:
    class IQuery : public boost::noncopyable
    {
    public:
      virtual ~IQuery()
      {
      }

      virtual ResourceType GetLevel() const = 0;

      virtual bool RestrictIdentifier(std::string& value,
                                      DicomTag identifier) const = 0;

      virtual bool HasMainDicomTagsFilter(ResourceType level) const = 0;

      virtual bool FilterMainDicomTags(const std::string& resourceId,
                                       ResourceType level,
                                       const DicomMap& mainTags) const = 0;

      virtual bool HasInstanceFilter() const = 0;

      virtual bool FilterInstance(const std::string& instanceId,
                                  const Json::Value& content) const = 0;
    };


  private:
    typedef std::map<DicomTag, std::string>  Identifiers;

    class CandidateResources;

    ServerContext&    context_;
    size_t            maxResults_;

    void ApplyAtLevel(CandidateResources& candidates,
                      const IQuery& query,
                      ResourceType level);

  public:
    ResourceFinder(ServerContext& context);

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
    bool Apply(std::list<std::string>& result,
               const IQuery& query);
  };

}
