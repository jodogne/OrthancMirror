/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "DicomInstanceToStore.h"

#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomParsing/ParsedDicomFile.h"
#include "../Core/Logging.h"
#include "../Core/OrthancException.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>


namespace Orthanc
{
  // Anonymous namespace to avoid clashes between compilation modules
  namespace
  {
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
  }


  class DicomInstanceToStore::PImpl
  {
  public:
    DicomInstanceOrigin                  origin_;
    SmartContainer<std::string>          buffer_;
    SmartContainer<ParsedDicomFile>      parsed_;
    SmartContainer<DicomMap>             summary_;
    SmartContainer<Json::Value>          json_;
    MetadataMap                          metadata_;

  private:
    std::unique_ptr<DicomInstanceHasher>  hasher_;

    void ComputeMissingInformation()
    {
      if (buffer_.HasContent() &&
          summary_.HasContent() &&
          json_.HasContent())
      {
        // Fine, everything is available
        return; 
      }
    
      if (!buffer_.HasContent())
      {
        if (!parsed_.HasContent())
        {
          if (!summary_.HasContent())
          {
            throw OrthancException(ErrorCode_NotImplemented);
          }
          else
          {
            parsed_.TakeOwnership(new ParsedDicomFile(summary_.GetConstContent(),
                                                      GetDefaultDicomEncoding(),
                                                      false /* be strict */));
          }                                
        }

        // Serialize the parsed DICOM file
        buffer_.Allocate();
        if (!FromDcmtkBridge::SaveToMemoryBuffer(buffer_.GetContent(), 
                                                 *parsed_.GetContent().GetDcmtkObject().getDataset()))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Unable to serialize a DICOM file to a memory buffer");
        }
      }

      if (summary_.HasContent() &&
          json_.HasContent())
      {
        return;
      }

      // At this point, we know that the DICOM file is available as a
      // memory buffer, but that its summary or its JSON version is
      // missing

      if (!parsed_.HasContent())
      {
        parsed_.TakeOwnership(new ParsedDicomFile(buffer_.GetConstContent()));
      }

      // At this point, we have parsed the DICOM file
    
      if (!summary_.HasContent())
      {
        summary_.Allocate();
        FromDcmtkBridge::ExtractDicomSummary(summary_.GetContent(), 
                                             *parsed_.GetContent().GetDcmtkObject().getDataset());
      }
    
      if (!json_.HasContent())
      {
        json_.Allocate();

        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ExtractDicomAsJson(json_.GetContent(), 
                                            *parsed_.GetContent().GetDcmtkObject().getDataset(),
                                            ignoreTagLength);
      }
    }


  public:
    const char* GetBufferData()
    {
      ComputeMissingInformation();
    
      if (!buffer_.HasContent())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (buffer_.GetConstContent().size() == 0)
      {
        return NULL;
      }
      else
      {
        return buffer_.GetConstContent().c_str();
      }
    }


    size_t GetBufferSize()
    {
      ComputeMissingInformation();
    
      if (!buffer_.HasContent())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return buffer_.GetConstContent().size();
    }


    const DicomMap& GetSummary()
    {
      ComputeMissingInformation();
    
      if (!summary_.HasContent())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return summary_.GetConstContent();
    }

    
    const Json::Value& GetJson()
    {
      ComputeMissingInformation();
    
      if (!json_.HasContent())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return json_.GetConstContent();
    }


    DicomInstanceHasher& GetHasher()
    {
      if (hasher_.get() == NULL)
      {
        hasher_.reset(new DicomInstanceHasher(GetSummary()));
      }

      if (hasher_.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return *hasher_;
    }

    
    bool LookupTransferSyntax(std::string& result)
    {
      ComputeMissingInformation();

      DicomMap header;
      if (DicomMap::ParseDicomMetaInformation(header, GetBufferData(), GetBufferSize()))
      {
        const DicomValue* value = header.TestAndGetValue(DICOM_TAG_TRANSFER_SYNTAX_UID);
        if (value != NULL &&
            !value->IsBinary() &&
            !value->IsNull())
        {
          result = Toolbox::StripSpaces(value->GetContent());
          return true;
        }
      }

      return false;
    }
  };


  DicomInstanceToStore::DicomInstanceToStore() :
    pimpl_(new PImpl)
  {
  }


  void DicomInstanceToStore::SetOrigin(const DicomInstanceOrigin& origin)
  {
    pimpl_->origin_ = origin;
  }

    
  const DicomInstanceOrigin& DicomInstanceToStore::GetOrigin() const
  {
    return pimpl_->origin_;
  }

    
  void DicomInstanceToStore::SetBuffer(const std::string& dicom)
  {
    pimpl_->buffer_.SetConstReference(dicom);
  }


  void DicomInstanceToStore::SetParsedDicomFile(ParsedDicomFile& parsed)
  {
    pimpl_->parsed_.SetReference(parsed);
  }


  void DicomInstanceToStore::SetSummary(const DicomMap& summary)
  {
    pimpl_->summary_.SetConstReference(summary);
  }


  void DicomInstanceToStore::SetJson(const Json::Value& json)
  {
    pimpl_->json_.SetConstReference(json);
  }


  const DicomInstanceToStore::MetadataMap& DicomInstanceToStore::GetMetadata() const
  {
    return pimpl_->metadata_;
  }


  DicomInstanceToStore::MetadataMap& DicomInstanceToStore::GetMetadata()
  {
    return pimpl_->metadata_;
  }


  void DicomInstanceToStore::AddMetadata(ResourceType level,
                                         MetadataType metadata,
                                         const std::string& value)
  {
    pimpl_->metadata_[std::make_pair(level, metadata)] = value;
  }


  const char* DicomInstanceToStore::GetBufferData()
  {
    return pimpl_->GetBufferData();
  }


  size_t DicomInstanceToStore::GetBufferSize()
  {
    return pimpl_->GetBufferSize();
  }


  const DicomMap& DicomInstanceToStore::GetSummary()
  {
    return pimpl_->GetSummary();
  }

    
  const Json::Value& DicomInstanceToStore::GetJson()
  {
    return pimpl_->GetJson();
  }


  bool DicomInstanceToStore::LookupTransferSyntax(std::string& result)
  {
    return pimpl_->LookupTransferSyntax(result);
  }


  DicomInstanceHasher& DicomInstanceToStore::GetHasher()
  {
    return pimpl_->GetHasher();
  }
}
