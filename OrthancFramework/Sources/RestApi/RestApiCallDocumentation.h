/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../Enumerations.h"

#include <boost/noncopyable.hpp>
#include <json/value.h>

#include <map>
#include <set>

namespace Orthanc
{
  class RestApiCallDocumentation : public boost::noncopyable
  {
  public:
    enum Type
    {
      Type_Unknown,
      Type_Text,
      Type_String,
      Type_Number,
      Type_Boolean,
      Type_JsonListOfStrings,
      Type_JsonListOfObjects,
      Type_JsonObject
    };    
    
  private:
    class Parameter
    {
    private:
      Type         type_;
      std::string  description_;
      bool         required_;

    public:
      Parameter() :
        type_(Type_Unknown),
        required_(false)
      {
      }
      
      Parameter(Type type,
                const std::string& description,
                bool required) :
        type_(type),
        description_(description),
        required_(required)
      {
      }

      Type GetType() const
      {
        return type_;
      }

      const std::string& GetDescription() const
      {
        return description_;
      }

      bool IsRequired() const
      {
        return required_;
      }
    };
    
    typedef std::map<std::string, Parameter>  Parameters;
    typedef std::map<MimeType, std::string>   AllowedTypes;

    HttpMethod    method_;
    std::string   tag_;
    std::string   summary_;
    std::string   description_;
    Parameters    uriArguments_;
    Parameters    httpHeaders_;
    Parameters    getArguments_;
    AllowedTypes  requestTypes_;
    Parameters    requestFields_;  // For JSON request
    AllowedTypes  answerTypes_;
    Parameters    answerFields_;  // Only if JSON object
    std::string   answerDescription_;
    bool          hasSampleText_;
    std::string   sampleText_;
    Json::Value   sampleJson_;
    bool          deprecated_;

  public:
    explicit RestApiCallDocumentation(HttpMethod method) :
      method_(method),
      hasSampleText_(false),
      sampleJson_(Json::nullValue),
      deprecated_(false)
    {
    }
    
    RestApiCallDocumentation& SetTag(const std::string& tag)
    {
      tag_ = tag;
      return *this;
    }

    RestApiCallDocumentation& SetSummary(const std::string& summary)
    {
      summary_ = summary;
      return *this;
    }

    RestApiCallDocumentation& SetDescription(const std::string& description)
    {
      description_ = description;
      return *this;
    }

    RestApiCallDocumentation& AddRequestType(MimeType mime,
                                             const std::string& description);

    RestApiCallDocumentation& SetRequestField(const std::string& name,
                                              Type type,
                                              const std::string& description,
                                              bool required);

    RestApiCallDocumentation& AddAnswerType(MimeType type,
                                            const std::string& description);

    RestApiCallDocumentation& SetUriArgument(const std::string& name,
                                             Type type,
                                             const std::string& description);

    RestApiCallDocumentation& SetUriArgument(const std::string& name,
                                             const std::string& description)
    {
      return SetUriArgument(name, Type_String, description);
    }

    bool HasUriArgument(const std::string& name) const
    {
      return (uriArguments_.find(name) != uriArguments_.end());
    }

    RestApiCallDocumentation& SetHttpHeader(const std::string& name,
                                            const std::string& description);

    RestApiCallDocumentation& SetHttpGetArgument(const std::string& name,
                                                 Type type,
                                                 const std::string& description,
                                                 bool required);

    RestApiCallDocumentation& SetAnswerField(const std::string& name,
                                             Type type,
                                             const std::string& description);

    void SetHttpGetSample(const std::string& url,
                          bool isJson);

    void SetTruncatedJsonHttpGetSample(const std::string& url,
                                       size_t size);

    void SetSample(const Json::Value& sample)
    {
      sampleJson_ = sample;
    }

    bool FormatOpenApi(Json::Value& target,
                       const std::set<std::string>& expectedUriArguments,
                       const std::string& uri /* only used in logs */) const;

    bool HasSummary() const
    {
      return !summary_.empty();
    }

    const std::string& GetSummary() const
    {
      return summary_;
    }

    const std::string& GetTag() const
    {
      return tag_;
    }

    RestApiCallDocumentation& SetDeprecated()
    {
      deprecated_ = true;
      return *this;
    }

    bool IsDeprecated() const
    {
      return deprecated_;
    }
  };
}
