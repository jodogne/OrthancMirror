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
#include "DicomFindQuery.h"

#include "FromDcmtkBridge.h"

#include <boost/regex.hpp> 


namespace Orthanc
{
  class DicomFindQuery::ValueConstraint : public DicomFindQuery::IConstraint
  {
  private:
    bool          isCaseSensitive_;
    std::string   expected_;

  public:
    ValueConstraint(const std::string& value,
                    bool caseSensitive) :
      isCaseSensitive_(caseSensitive),
      expected_(value)
    {
    }

    const std::string& GetValue() const
    {
      return expected_;
    }

    virtual bool IsExactConstraint() const
    {
      return isCaseSensitive_;
    }

    virtual bool Apply(const std::string& value) const
    {
      if (isCaseSensitive_)
      {
        return expected_ == value;
      }
      else
      {
        std::string v, c;
        Toolbox::ToLowerCase(v, value);
        Toolbox::ToLowerCase(c, expected_);
        return v == c;
      }
    }
  };


  class DicomFindQuery::ListConstraint : public DicomFindQuery::IConstraint
  {
  private:
    std::set<std::string>  values_;

  public:
    ListConstraint(const std::string& values)
    {
      std::vector<std::string> items;
      Toolbox::TokenizeString(items, values, '\\');

      for (size_t i = 0; i < items.size(); i++)
      {
        std::string lower;
        Toolbox::ToLowerCase(lower, items[i]);
        values_.insert(lower);
      }
    }

    virtual bool Apply(const std::string& value) const
    {
      std::string tmp;
      Toolbox::ToLowerCase(tmp, value);
      return values_.find(tmp) != values_.end();
    }
  };


  class DicomFindQuery::RangeConstraint : public DicomFindQuery::IConstraint
  {
  private:
    std::string lower_;
    std::string upper_;

  public:
    RangeConstraint(const std::string& range)
    {
      size_t separator = range.find('-');
      Toolbox::ToLowerCase(lower_, range.substr(0, separator));
      Toolbox::ToLowerCase(upper_, range.substr(separator + 1));
    }

    virtual bool Apply(const std::string& value) const
    {
      std::string v;
      Toolbox::ToLowerCase(v, value);

      if (lower_.size() == 0 && 
          upper_.size() == 0)
      {
        return false;
      }

      if (lower_.size() == 0)
      {
        return v <= upper_;
      }

      if (upper_.size() == 0)
      {
        return v >= lower_;
      }
    
      return (v >= lower_ && v <= upper_);
    }
  };


  class DicomFindQuery::WildcardConstraint : public DicomFindQuery::IConstraint
  {
  private:
    boost::regex pattern_;

  public:
    WildcardConstraint(const std::string& wildcard,
                       bool caseSensitive)
    {
      std::string re = Toolbox::WildcardToRegularExpression(wildcard);

      if (caseSensitive)
      {
        pattern_ = boost::regex(re);
      }
      else
      {
        pattern_ = boost::regex(re, boost::regex::icase /* case insensitive search */);
      }
    }

    virtual bool Apply(const std::string& value) const
    {
      return boost::regex_match(value, pattern_);
    }
  };


  void DicomFindQuery::PrepareMainDicomTags(ResourceType level)
  {
    std::set<DicomTag> tags;
    DicomMap::GetMainDicomTags(tags, level);

    for (std::set<DicomTag>::const_iterator
           it = tags.begin(); it != tags.end(); ++it)
    {
      mainDicomTags_[*it] = level;
    }
  }


  DicomFindQuery::DicomFindQuery() : 
    level_(ResourceType_Patient),
    filterJson_(false)
  {
    PrepareMainDicomTags(ResourceType_Patient);
    PrepareMainDicomTags(ResourceType_Study);
    PrepareMainDicomTags(ResourceType_Series);
    PrepareMainDicomTags(ResourceType_Instance);
  }


  DicomFindQuery::~DicomFindQuery()
  {
    for (Constraints::iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      delete it->second;
    }
  }




  void DicomFindQuery::AssignConstraint(const DicomTag& tag,
                                        IConstraint* constraint)
  {
    Constraints::iterator it = constraints_.find(tag);

    if (it != constraints_.end())
    {
      constraints_.erase(it);
    }

    constraints_[tag] = constraint;

    MainDicomTags::const_iterator tmp = mainDicomTags_.find(tag);
    if (tmp == mainDicomTags_.end())
    {
      // The query depends upon a DICOM tag that is not a main tag
      // from the point of view of Orthanc, we need to decode the
      // JSON file on the disk.
      filterJson_ = true;
    }
    else
    {
      filteredLevels_.insert(tmp->second);
    }
  }


  void DicomFindQuery::SetConstraint(const DicomTag& tag,
                                     const std::string& constraint,
                                     bool caseSensitivePN)
  {
    ValueRepresentation vr = FromDcmtkBridge::GetValueRepresentation(tag);

    bool sensitive = true;
    if (vr == ValueRepresentation_PatientName)
    {
      sensitive = caseSensitivePN;
    }

    // http://www.itk.org/Wiki/DICOM_QueryRetrieve_Explained
    // http://dicomiseasy.blogspot.be/2012/01/dicom-queryretrieve-part-i.html  

    if ((vr == ValueRepresentation_Date ||
         vr == ValueRepresentation_DateTime ||
         vr == ValueRepresentation_Time) &&
        constraint.find('-') != std::string::npos)
    {
      /**
       * Range matching is only defined for TM, DA and DT value
       * representations. This code fixes issues 35 and 37.
       *
       * Reference: "Range matching is not defined for types of
       * Attributes other than dates and times", DICOM PS 3.4,
       * C.2.2.2.5 ("Range Matching").
       **/
      AssignConstraint(tag, new RangeConstraint(constraint));
    }
    else if (constraint.find('\\') != std::string::npos)
    {
      AssignConstraint(tag, new ListConstraint(constraint));
    }
    else if (constraint.find('*') != std::string::npos ||
             constraint.find('?') != std::string::npos)
    {
      AssignConstraint(tag, new WildcardConstraint(constraint, sensitive));
    }
    else
    {
      /**
       * Case-insensitive match for PN value representation (Patient
       * Name). Case-senstive match for all the other value
       * representations.
       *
       * Reference: DICOM PS 3.4
       *   - C.2.2.2.1 ("Single Value Matching") 
       *   - C.2.2.2.4 ("Wild Card Matching")
       * http://medical.nema.org/Dicom/2011/11_04pu.pdf
       *
       * "Except for Attributes with a PN Value Representation, only
       * entities with values which match exactly the value specified in the
       * request shall match. This matching is case-sensitive, i.e.,
       * sensitive to the exact encoding of the key attribute value in
       * character sets where a letter may have multiple encodings (e.g.,
       * based on its case, its position in a word, or whether it is
       * accented)
       * 
       * For Attributes with a PN Value Representation (e.g., Patient Name
       * (0010,0010)), an application may perform literal matching that is
       * either case-sensitive, or that is insensitive to some or all
       * aspects of case, position, accent, or other character encoding
       * variants."
       *
       * (0008,0018) UI SOPInstanceUID     => Case-sensitive
       * (0008,0050) SH AccessionNumber    => Case-sensitive
       * (0010,0020) LO PatientID          => Case-sensitive
       * (0020,000D) UI StudyInstanceUID   => Case-sensitive
       * (0020,000E) UI SeriesInstanceUID  => Case-sensitive
      **/

      AssignConstraint(tag, new ValueConstraint(constraint, sensitive));
    }
  }


  bool DicomFindQuery::RestrictIdentifier(std::string& value,
                                          DicomTag identifier) const
  {
    Constraints::const_iterator it = constraints_.find(identifier);
    if (it == constraints_.end() ||
        !it->second->IsExactConstraint())
    {
      return false;
    }
    else
    {
      value = dynamic_cast<ValueConstraint*>(it->second)->GetValue();
      return true;
    }
  }

  bool DicomFindQuery::HasMainDicomTagsFilter(ResourceType level) const
  {
    return filteredLevels_.find(level) != filteredLevels_.end();
  }

  bool DicomFindQuery::FilterMainDicomTags(const std::string& resourceId,
                                           ResourceType level,
                                           const DicomMap& mainTags) const
  {
    std::set<DicomTag> tags;
    mainTags.GetTags(tags);

    for (std::set<DicomTag>::const_iterator
           it = tags.begin(); it != tags.end(); ++it)
    {
      Constraints::const_iterator constraint = constraints_.find(*it);
      if (constraint != constraints_.end() &&
          !constraint->second->Apply(mainTags.GetValue(*it).AsString()))
      {
        return false;
      }
    }

    return true;
  }

  bool DicomFindQuery::HasInstanceFilter() const
  {
    return filterJson_;
  }

  bool DicomFindQuery::FilterInstance(const std::string& instanceId,
                                      const Json::Value& content) const
  {
    for (Constraints::const_iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      std::string tag = it->first.Format();
      std::string value;
      if (content.isMember(tag))
      {
        value = content.get(tag, Json::arrayValue).get("Value", "").asString();
      }

      if (!it->second->Apply(value))
      {
        return false;
      }
    }

    return true;
  }
}
