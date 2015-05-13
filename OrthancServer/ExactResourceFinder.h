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
  private:
    typedef std::map<DicomTag, std::string>  Query;

    class CandidateResources;

    ServerIndex&  index_;
    ResourceType  level_;
    bool          caseSensitive_;
    bool          nonMainTagsIgnored_;
    Query         query_;

    static void ExtractTagsForLevel(Query& result,
                                    Query& source,
                                    ResourceType level);

    void ApplyAtLevel(CandidateResources& candidates,
                      ResourceType level);

  public:
    ExactResourceFinder(ServerIndex& index);

    bool IsCaseSensitive() const
    {
      return caseSensitive_;
    }

    void SetCaseSensitive(bool sensitive)
    {
      caseSensitive_ = sensitive;
    }

    bool NonMainTagsIgnored() const
    {
      return nonMainTagsIgnored_;
    }

    void SetNonMainTagsIgnored(bool ignored)
    {
      nonMainTagsIgnored_ = ignored;
    }

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetLevel(ResourceType level)
    {
      level_ = level;
    }

    void AddTag(const DicomTag& tag,
                const std::string& value)
    {
      query_.insert(std::make_pair(tag, value));
    }

    void AddTag(const std::string& tag,
                const std::string& value);

    void Apply(std::list<std::string>& result);
  };

}
