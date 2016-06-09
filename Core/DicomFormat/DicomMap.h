/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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

#include "DicomTag.h"
#include "DicomValue.h"
#include "../Enumerations.h"

#include <set>
#include <map>
#include <json/json.h>

namespace Orthanc
{
  class DicomMap : public boost::noncopyable
  {
  private:
    friend class DicomArray;
    friend class FromDcmtkBridge;
    friend class ToDcmtkBridge;

    typedef std::map<DicomTag, DicomValue*>  Map;

    Map map_;

    // Warning: This takes the ownership of "value"
    void SetValue(uint16_t group, 
                  uint16_t element, 
                  DicomValue* value);

    void SetValue(DicomTag tag, 
                  DicomValue* value);

    void ExtractTags(DicomMap& source,
                     const DicomTag* tags,
                     size_t count) const;
   
    static void GetMainDicomTagsInternal(std::set<DicomTag>& result, ResourceType level);

  public:
    DicomMap()
    {
    }

    ~DicomMap()
    {
      Clear();
    }

    size_t GetSize() const
    {
      return map_.size();
    }
    
    DicomMap* Clone() const;

    void Assign(const DicomMap& other);

    void Clear();

    void SetValue(uint16_t group, 
                  uint16_t element, 
                  const DicomValue& value)
    {
      SetValue(group, element, value.Clone());
    }

    void SetValue(const DicomTag& tag,
                  const DicomValue& value)
    {
      SetValue(tag, value.Clone());
    }

    void SetValue(const DicomTag& tag,
                  const std::string& str,
                  bool isBinary)
    {
      SetValue(tag, new DicomValue(str, isBinary));
    }

    void SetValue(uint16_t group, 
                  uint16_t element, 
                  const std::string& str,
                  bool isBinary)
    {
      SetValue(group, element, new DicomValue(str, isBinary));
    }

    bool HasTag(uint16_t group, uint16_t element) const
    {
      return HasTag(DicomTag(group, element));
    }

    bool HasTag(const DicomTag& tag) const
    {
      return map_.find(tag) != map_.end();
    }

    const DicomValue& GetValue(uint16_t group, uint16_t element) const
    {
      return GetValue(DicomTag(group, element));
    }

    const DicomValue& GetValue(const DicomTag& tag) const;

    // DO NOT delete the returned value!
    const DicomValue* TestAndGetValue(uint16_t group, uint16_t element) const
    {
      return TestAndGetValue(DicomTag(group, element));
    }       

    // DO NOT delete the returned value!
    const DicomValue* TestAndGetValue(const DicomTag& tag) const;

    void Remove(const DicomTag& tag);

    void ExtractPatientInformation(DicomMap& result) const;

    void ExtractStudyInformation(DicomMap& result) const;

    void ExtractSeriesInformation(DicomMap& result) const;

    void ExtractInstanceInformation(DicomMap& result) const;

    static void SetupFindPatientTemplate(DicomMap& result);

    static void SetupFindStudyTemplate(DicomMap& result);

    static void SetupFindSeriesTemplate(DicomMap& result);

    static void SetupFindInstanceTemplate(DicomMap& result);

    void CopyTagIfExists(const DicomMap& source,
                         const DicomTag& tag);

    static bool IsMainDicomTag(const DicomTag& tag, ResourceType level);

    static bool IsMainDicomTag(const DicomTag& tag);

    static void GetMainDicomTags(std::set<DicomTag>& result, ResourceType level);

    static void GetMainDicomTags(std::set<DicomTag>& result);

    void GetTags(std::set<DicomTag>& tags) const;

    static void LoadMainDicomTags(const DicomTag*& tags,
                                  size_t& size,
                                  ResourceType level);

    static bool ParseDicomMetaInformation(DicomMap& result,
                                          const char* dicom,
                                          size_t size);
  };
}
