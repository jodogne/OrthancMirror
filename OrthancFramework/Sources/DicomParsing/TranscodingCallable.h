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


#pragma once

#include "IDicomTranscoder.h"
#include "../MultiThreading/ICallable.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class TranscodingCallable : public ICallable
  {
  private:
    boost::shared_ptr<IDicomTranscoder>  transcoder_;
    IDicomTranscoder::DicomImage         source_;
    std::set<DicomTransferSyntax>        allowedSyntaxes_;
    TranscodingSopInstanceUidMode        mode_;
    bool                                 hasLossyQuality_;
    unsigned int                         lossyQuality_;

  public:
    // By default, allow new SOP instance UID
    TranscodingCallable();

    void AcquireParsed(ParsedDicomFile& parsed /* will be invalidated */)
    {
      source_.AcquireParsed(parsed);
    }

    void AcquireBuffer(std::string& buffer /* will be swapped */)
    {
      source_.AcquireBuffer(buffer);
    }

    void SetTranscoder(const boost::shared_ptr<IDicomTranscoder>& transcoder);

    void SetMode(TranscodingSopInstanceUidMode mode)
    {
      mode_ = mode;
    }

    void AddTransferSyntax(DicomTransferSyntax syntax)
    {
      allowedSyntaxes_.insert(syntax);
    }

    void SetLossyQuality(unsigned int quality);

    virtual IDynamicObject* Call() ORTHANC_OVERRIDE;
  };
}
