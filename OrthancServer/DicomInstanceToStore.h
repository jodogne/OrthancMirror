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


#pragma once

#include "../Core/DicomParsing/ParsedDicomFile.h"
#include "ServerIndex.h"
#include "../Core/OrthancException.h"
#include "../Core/RestApi/RestApiCall.h"

namespace Orthanc
{
  class DicomInstanceToStore
  {
  private:
    template <typename T>
    class SmartContainer
    {
    private:
      T* content_;
      bool toDelete_;
      bool isReadOnly_;

      void Deallocate()
      {
        if (content_ && toDelete_)
        {
          delete content_;
          toDelete_ = false;
          content_ = NULL;
        }
      }

    public:
      SmartContainer() : content_(NULL), toDelete_(false), isReadOnly_(true)
      {
      }

      ~SmartContainer()
      {
        Deallocate();
      }

      void Allocate()
      {
        Deallocate();
        content_ = new T;
        toDelete_ = true;
        isReadOnly_ = false;
      }

      void TakeOwnership(T* content)
      {
        if (content == NULL)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

        Deallocate();
        content_ = content;
        toDelete_ = true;
        isReadOnly_ = false;
      }

      void SetReference(T& content)   // Read and write assign, without transfering ownership
      {
        Deallocate();
        content_ = &content;
        toDelete_ = false;
        isReadOnly_ = false;
      }

      void SetConstReference(const T& content)   // Read-only assign, without transfering ownership
      {
        Deallocate();
        content_ = &const_cast<T&>(content);
        toDelete_ = false;
        isReadOnly_ = true;
      }

      bool HasContent() const
      {
        return content_ != NULL;
      }

      T& GetContent()
      {
        if (content_ == NULL)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        if (isReadOnly_)
        {
          throw OrthancException(ErrorCode_ReadOnly);
        }

        return *content_;
      }

      const T& GetConstContent() const
      {
        if (content_ == NULL)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        return *content_;
      }
    };


    SmartContainer<std::string>  buffer_;
    SmartContainer<ParsedDicomFile>  parsed_;
    SmartContainer<DicomMap>  summary_;
    SmartContainer<Json::Value>  json_;

    RequestOrigin origin_;
    std::string remoteIp_;
    std::string dicomRemoteAet_;
    std::string dicomCalledAet_;
    std::string httpUsername_;
    ServerIndex::MetadataMap metadata_;

    void ComputeMissingInformation();

  public:
    DicomInstanceToStore() : origin_(RequestOrigin_Unknown)
    {
    }

    void SetDicomProtocolOrigin(const char* remoteIp,
                                const char* remoteAet,
                                const char* calledAet);

    void SetRestOrigin(const RestApiCall& call);

    void SetHttpOrigin(const char* remoteIp,
                       const char* username);

    void SetLuaOrigin();

    void SetPluginsOrigin();

    RequestOrigin GetRequestOrigin() const
    {
      return origin_;
    }

    const char* GetRemoteAet() const; 

    void SetBuffer(const std::string& dicom)
    {
      buffer_.SetConstReference(dicom);
    }

    void SetParsedDicomFile(ParsedDicomFile& parsed)
    {
      parsed_.SetReference(parsed);
    }

    void SetSummary(const DicomMap& summary)
    {
      summary_.SetConstReference(summary);
    }

    void SetJson(const Json::Value& json)
    {
      json_.SetConstReference(json);
    }

    void AddMetadata(ResourceType level,
                     MetadataType metadata,
                     const std::string& value);

    const ServerIndex::MetadataMap& GetMetadata() const
    {
      return metadata_;
    }

    ServerIndex::MetadataMap& GetMetadata()
    {
      return metadata_;
    }

    const char* GetBufferData();

    size_t GetBufferSize();

    const DicomMap& GetSummary();
    
    const Json::Value& GetJson();

    void GetOriginInformation(Json::Value& result) const;

    bool LookupTransferSyntax(std::string& result);
  };
}
