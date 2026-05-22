/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "MultipleDicomReader.h"

#include "../OrthancException.h"
#include "DataSourceAnswer.h"
#include "DataSourceReader.h"
#include "DataSourceRequest.h"


namespace Orthanc
{
  class MultipleDicomReader::PImpl : public boost::noncopyable
  {
  private:
    boost::shared_ptr<DataSourceReader>  reader_;
    std::unique_ptr<DataSourceRequest>   request_;
    boost::shared_ptr<DataSourceAnswer>  answer_;

  public:
    PImpl(const boost::shared_ptr<DataSourceReader>& reader) :
      reader_(reader),
      request_(new DataSourceRequest)
    {
    }

    void Enqueue(const FileInfo& attachment)
    {
      if (request_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        request_->Enqueue(DicomDataSource::CreateWholeRequest(attachment));
      }
    }

    DicomDataSource::Dicom* Dequeue()
    {
      if (answer_.get() == NULL)
      {
        assert(request_.get() != NULL);
        answer_ = reader_->Submit(request_.release());
      }

      std::unique_ptr<DataSourceAnswer::Item> item(answer_->Dequeue());
      return new DicomDataSource::Dicom(item->GetValue());
    }
  };


  MultipleDicomReader::MultipleDicomReader(const boost::shared_ptr<DataSourceReader>& reader) :
    pimpl_(new PImpl(reader))
  {
  }


  MultipleDicomReader::~MultipleDicomReader()
  {
    assert(pimpl_ != NULL);
    delete pimpl_;
  }


  void MultipleDicomReader::Enqueue(const FileInfo& attachment)
  {
    pimpl_->Enqueue(attachment);
  }


  DicomDataSource::Dicom* MultipleDicomReader::Dequeue()
  {
    return pimpl_->Dequeue();
  }
}
