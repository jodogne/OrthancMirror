/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "RestApiCallDocumentation.h"

#if ORTHANC_ENABLE_CURL == 1
#  include "../HttpClient.h"
#endif

#include "../Logging.h"
#include "../OrthancException.h"


namespace Orthanc
{
  RestApiCallDocumentation& RestApiCallDocumentation::AddRequestType(MimeType mime,
                                                                     const std::string& description)
  {
    if (method_ != HttpMethod_Post &&
        method_ != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_BadParameterType, "Request body is only allowed on POST and PUT");
    }
    else if (requestTypes_.find(mime) != requestTypes_.end() &&
             mime != MimeType_Json)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot register twice the same type of request: " +
                             std::string(EnumerationToString(mime)));
    }
    else
    {
      requestTypes_[mime] = description;
    }
        
    return *this;
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetRequestField(const std::string& name,
                                                                      Type type,
                                                                      const std::string& description,
                                                                      bool required)
  {
    if (method_ != HttpMethod_Post &&
        method_ != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_BadParameterType, "Request body is only allowed on POST and PUT");
    }    

    if (requestTypes_.find(MimeType_Json) == requestTypes_.end())
    {
      requestTypes_[MimeType_Json] = "";
    }
    
    if (requestFields_.find(name) != requestFields_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Field \"" + name + "\" of JSON request is already documented");
    }
    else
    {
      requestFields_[name] = Parameter(type, description, required);
      return *this;
    }    
  }


  RestApiCallDocumentation& RestApiCallDocumentation::AddAnswerType(MimeType mime,
                                                                    const std::string& description)
  {
    if (answerTypes_.find(mime) != answerTypes_.end() &&
        mime != MimeType_Json)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot register twice the same type of answer: " +
                             std::string(EnumerationToString(mime)));
    }
    else
    {
      answerTypes_[mime] = description;
    }

    return *this;
  }
  

  RestApiCallDocumentation& RestApiCallDocumentation::SetUriArgument(const std::string& name,
                                                                     Type type,
                                                                     const std::string& description)
  {
    if (uriArguments_.find(name) != uriArguments_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "URI argument \"" + name + "\" is already documented");
    }
    else
    {
      uriArguments_[name] = Parameter(type, description, true);
      return *this;
    }
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetHttpHeader(const std::string& name,
                                                                    const std::string& description)
  {
    if (httpHeaders_.find(name) != httpHeaders_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "HTTP header \"" + name + "\" is already documented");
    }
    else
    {
      httpHeaders_[name] = Parameter(Type_String, description, false);
      return *this;
    }
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetHttpGetArgument(const std::string& name,
                                                                         Type type,
                                                                         const std::string& description,
                                                                         bool required)
  {
    if (method_ != HttpMethod_Get)
    {
      throw OrthancException(ErrorCode_InternalError, "Cannot set a HTTP GET argument on HTTP method: " +
                             std::string(EnumerationToString(method_)));
    }    
    else if (getArguments_.find(name) != getArguments_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "GET argument \"" + name + "\" is already documented");
    }
    else
    {
      getArguments_[name] = Parameter(type, description, required);
      return *this;
    }
  }

  
  RestApiCallDocumentation& RestApiCallDocumentation::SetAnswerField(const std::string& name,
                                                                     Type type,
                                                                     const std::string& description)
  {
    if (answerTypes_.find(MimeType_Json) == answerTypes_.end())
    {
      answerTypes_[MimeType_Json] = "";
    }
    
    if (answerFields_.find(name) != answerFields_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Field \"" + name + "\" of JSON answer is already documented");
    }
    else
    {
      answerFields_[name] = Parameter(type, description, false);
      return *this;
    }    
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetAnswerHeader(const std::string& name,
                                                                    const std::string& description)
  {
    if (answerHeaders_.find(name) != answerHeaders_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Answer HTTP header \"" + name + "\" is already documented");
    }
    else
    {
      answerHeaders_[name] = Parameter(Type_String, description, false);
      return *this;
    }
  }


  void RestApiCallDocumentation::SetHttpGetSample(const std::string& url,
                                                  bool isJson)
  {
#if ORTHANC_ENABLE_CURL == 1
    HttpClient client;
    client.SetUrl(url);
    client.SetHttpsVerifyPeers(false);

    if (isJson)
    {
      if (!client.Apply(sampleJson_))
      {
        LOG(ERROR) << "Cannot GET: " << url;
        sampleJson_ = Json::nullValue;
      }
    }
    else
    {
      if (client.Apply(sampleText_))
      {
        hasSampleText_ = true;
      }
      else
      {
        LOG(ERROR) << "Cannot GET: " << url;
        hasSampleText_ = false;
      }
    }
#else
    LOG(WARNING) << "HTTP client is not available to generated the documentation";
#endif
  }


  static void Truncate(Json::Value& value,
                       size_t size)
  {
    if (value.type() == Json::arrayValue)
    {
      if (value.size() > size)
      {
        value.resize(size);
        value.append("...");
      }

      for (Json::Value::ArrayIndex i = 0; i < value.size(); i++)
      {
        Truncate(value[i], size);
      }
    }
    else if (value.type() == Json::objectValue)
    {
      std::vector<std::string> members = value.getMemberNames();
      if (members.size() > size)
      {
        members.resize(size);

        Json::Value v = Json::objectValue;
        for (size_t i = 0; i < members.size(); i++)
        {
          v[members[i]] = value[members[i]];
        }

        // We use the "{" symbol, as it the last in the 7bit ASCII
        // table, which places "..." at the end of the object in OpenAPI
        v["{...}"] = "...";
        
        value = v;
      }

      for (size_t i = 0; i < members.size(); i++)
      {
        Truncate(value[members[i]], size);
      }
    }
  }

  
  void RestApiCallDocumentation::SetTruncatedJsonHttpGetSample(const std::string& url,
                                                               size_t size)
  {
    SetHttpGetSample(url, true);
    Truncate(sampleJson_, size);
  }



  static void TypeToSchema(Json::Value& target,
                           RestApiCallDocumentation::Type type)
  {
    switch (type)
    {
      case RestApiCallDocumentation::Type_Unknown:
        throw OrthancException(ErrorCode_ParameterOutOfRange);

      case RestApiCallDocumentation::Type_String:
      case RestApiCallDocumentation::Type_Text:
        target["type"] = "string";
        return;

      case RestApiCallDocumentation::Type_Number:
        target["type"] = "number";
        return;

      case RestApiCallDocumentation::Type_Boolean:
        target["type"] = "boolean";
        return;

      case RestApiCallDocumentation::Type_JsonObject:
        target["type"] = "object";
        return;

      case RestApiCallDocumentation::Type_JsonListOfStrings:
        target["type"] = "array";
        target["items"]["type"] = "string";
        return;

      case RestApiCallDocumentation::Type_JsonListOfObjects:
        target["type"] = "array";
        target["items"]["type"] = "object";
        return;
        
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool RestApiCallDocumentation::FormatOpenApi(Json::Value& target,
                                               const std::set<std::string>& expectedUriArguments,
                                               const std::string& uri) const
  {
    if (summary_.empty() &&
        description_.empty())
    {
      return false;
    }
    else
    {
      target = Json::objectValue;
    
      if (!tag_.empty())
      {
        target["tags"].append(tag_);
      }

      if (!summary_.empty())
      {
        target["summary"] = summary_;
      }
      else if (!description_.empty())
      {
        target["summary"] = description_;
      }

      if (!description_.empty())
      {
        target["description"] = description_;
      }
      else if (!summary_.empty())
      {
        target["description"] = summary_;
      }

      target["deprecated"] = deprecated_;

      if (method_ == HttpMethod_Post ||
          method_ == HttpMethod_Put)
      {
        for (AllowedTypes::const_iterator it = requestTypes_.begin();
             it != requestTypes_.end(); ++it)
        {
          Json::Value& schema = target["requestBody"]["content"][EnumerationToString(it->first)]["schema"];
          schema["description"] = it->second;

          if (it->first == MimeType_Json)
          {
            for (Parameters::const_iterator field = requestFields_.begin();
                 field != requestFields_.end(); ++field)
            {
              Json::Value p = Json::objectValue;
              TypeToSchema(p, field->second.GetType());
              p["description"] = field->second.GetDescription();
              schema["properties"][field->first] = p;         
            }        

            if (!it->second.empty() &&
                answerFields_.size() > 0)
            {
              LOG(WARNING) << "The JSON description will not be visible if the fields of the JSON request are detailed: "
                           << EnumerationToString(method_) << " " << uri;
            }
          }
        }
      }

      target["responses"]["200"]["description"] = (answerDescription_.empty() ? "" : answerDescription_);

      for (AllowedTypes::const_iterator it = answerTypes_.begin();
           it != answerTypes_.end(); ++it)
      {
        Json::Value& schema = target["responses"]["200"]["content"][EnumerationToString(it->first)]["schema"];
        schema["description"] = it->second;

        if (it->first == MimeType_Json)
        {
          for (Parameters::const_iterator field = answerFields_.begin();
               field != answerFields_.end(); ++field)
          {
            Json::Value p = Json::objectValue;
            TypeToSchema(p, field->second.GetType());
            p["description"] = field->second.GetDescription();
            schema["properties"][field->first] = p;         
          }

          if (!it->second.empty() &&
              answerFields_.size() > 0)
          {
            LOG(WARNING) << "The JSON description will not be visible if the fields of the JSON answer are detailed: "
                         << EnumerationToString(method_) << " " << uri;
          }
        }
      }
      
      for (AllowedTypes::const_iterator it = answerTypes_.begin();
           it != answerTypes_.end(); ++it)
      {
        if (it->first == MimeType_Json &&
            sampleJson_.type() != Json::nullValue)
        {
          // Handled below
        }
        else if (it->first == MimeType_PlainText &&
                 hasSampleText_)
        {
          // Handled below
        }
        else
        {
          // No sample for this MIME type
          target["responses"]["200"]["content"][EnumerationToString(it->first)]["examples"] = Json::objectValue;
        }
      }

      if (sampleJson_.type() != Json::nullValue)
      {
        target["responses"]["200"]["content"][EnumerationToString(MimeType_Json)]["schema"]["example"] = sampleJson_;
      }
      
      if (hasSampleText_)
      {
        target["responses"]["200"]["content"][EnumerationToString(MimeType_PlainText)]["example"] = sampleText_;
      }

      if (!answerHeaders_.empty())
      {
        Json::Value answerHeaders = Json::objectValue;

        for (Parameters::const_iterator it = answerHeaders_.begin(); it != answerHeaders_.end(); ++it)
        {
          Json::Value h = Json::objectValue;
          h["description"] = it->second.GetDescription();          
          answerHeaders[it->first] = h;
        }

        target["responses"]["200"]["headers"] = answerHeaders;
      }

      Json::Value parameters = Json::arrayValue;
        
      for (Parameters::const_iterator it = getArguments_.begin();
           it != getArguments_.end(); ++it)
      {
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "query";
        p["required"] = it->second.IsRequired();
        TypeToSchema(p["schema"], it->second.GetType());
        p["description"] = it->second.GetDescription();
        parameters.append(p);         
      }

      for (Parameters::const_iterator it = httpHeaders_.begin();
           it != httpHeaders_.end(); ++it)
      {
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "header";
        p["required"] = it->second.IsRequired();
        TypeToSchema(p["schema"], it->second.GetType());
        p["description"] = it->second.GetDescription();
        parameters.append(p);         
      }

      for (Parameters::const_iterator it = uriArguments_.begin();
           it != uriArguments_.end(); ++it)
      {
        if (expectedUriArguments.find(it->first) == expectedUriArguments.end())
        {
          throw OrthancException(ErrorCode_InternalError, "Unexpected URI argument: " + it->first);
        }
        
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "path";
        p["required"] = it->second.IsRequired();
        TypeToSchema(p["schema"], it->second.GetType());
        p["description"] = it->second.GetDescription();
        parameters.append(p);         
      }

      for (std::set<std::string>::const_iterator it = expectedUriArguments.begin();
           it != expectedUriArguments.end(); ++it)
      {
        if (uriArguments_.find(*it) == uriArguments_.end())
        {
          throw OrthancException(ErrorCode_InternalError, "Missing URI argument: " + *it);
        }
      }

      target["parameters"] = parameters;

      return true;
    }
  }
}
