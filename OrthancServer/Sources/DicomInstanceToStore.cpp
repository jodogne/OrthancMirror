/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "OrthancConfiguration.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"

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
    bool                                 hasBuffer_;
    std::unique_ptr<std::string>         ownBuffer_;
    const void*                          bufferData_;
    size_t                               bufferSize_;
    SmartContainer<ParsedDicomFile>      parsed_;
    MetadataMap                          metadata_;

    PImpl() :
      hasBuffer_(false),
      bufferData_(NULL),
      bufferSize_(0)
    {
    }

  private:
    void ComputeParsedDicomFileIfMissing()
    {
      if (!parsed_.HasContent())
      {
        if (!hasBuffer_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      
        if (ownBuffer_.get() != NULL)
        {
          parsed_.TakeOwnership(new ParsedDicomFile(*ownBuffer_));
        }
        else
        {
          parsed_.TakeOwnership(new ParsedDicomFile(bufferData_, bufferSize_));
        }
      }
    }

    void ComputeDicomBufferIfMissing()
    {
      if (!hasBuffer_)
      {
        if (!parsed_.HasContent())
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }

        // Serialize the parsed DICOM file
        ownBuffer_.reset(new std::string);
        if (!FromDcmtkBridge::SaveToMemoryBuffer(*ownBuffer_,
                                                 *parsed_.GetContent().GetDcmtkObject().getDataset()))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Unable to serialize a DICOM file to a memory buffer");
        }

        hasBuffer_ = true;
      }
    }


  public:
    void SetBuffer(const void* data,
                   size_t size)
    {
      ownBuffer_.reset(NULL);
      bufferData_ = data;
      bufferSize_ = size;
      hasBuffer_ = true;
    }
    
    const void* GetBufferData()
    {
      ComputeDicomBufferIfMissing();

      if (!hasBuffer_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (ownBuffer_.get() != NULL)
      {
        if (ownBuffer_->empty())
        {
          return NULL;
        }
        else
        {
          return ownBuffer_->c_str();
        }
      }
      else
      {
        return bufferData_;
      }
    }


    size_t GetBufferSize()
    {
      ComputeDicomBufferIfMissing();
    
      if (!hasBuffer_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (ownBuffer_.get() != NULL)
      {
        return ownBuffer_->size();
      }
      else
      {
        return bufferSize_;
      }
    }


    bool LookupTransferSyntax(std::string& result)
    {
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
      else
      {
        // This is a DICOM file without a proper meta-header. Fallback
        // to DCMTK, which will fully parse the dataset to retrieve
        // the transfer syntax. Added in Orthanc 1.8.2.
        std::string transferSyntax;
        if (GetParsedDicomFile().LookupTransferSyntax(transferSyntax))
        {
          result = Toolbox::StripSpaces(transferSyntax);
          return true;
        }
      }

      return false;
    }


    ParsedDicomFile& GetParsedDicomFile()
    {
      ComputeParsedDicomFileIfMissing();
      
      if (parsed_.HasContent())
      {
        return parsed_.GetContent();
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
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

    
  void DicomInstanceToStore::SetBuffer(const void* dicom,
                                       size_t size)
  {
    pimpl_->SetBuffer(dicom, size);
  }


  void DicomInstanceToStore::SetParsedDicomFile(ParsedDicomFile& parsed)
  {
    pimpl_->parsed_.SetReference(parsed);
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


  const void* DicomInstanceToStore::GetBufferData() const
  {
    return const_cast<PImpl&>(*pimpl_).GetBufferData();
  }


  size_t DicomInstanceToStore::GetBufferSize() const
  {
    return const_cast<PImpl&>(*pimpl_).GetBufferSize();
  }


  bool DicomInstanceToStore::LookupTransferSyntax(std::string& result) const
  {
    return const_cast<PImpl&>(*pimpl_).LookupTransferSyntax(result);
  }


  bool DicomInstanceToStore::HasPixelData() const
  {
    return const_cast<PImpl&>(*pimpl_).GetParsedDicomFile().HasTag(DICOM_TAG_PIXEL_DATA);
  }

  ParsedDicomFile& DicomInstanceToStore::GetParsedDicomFile() const
  {
    return const_cast<PImpl&>(*pimpl_).GetParsedDicomFile();
  }
}
