/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "DicomFindAnswers.h"

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class DicomUserConnection : public boost::noncopyable
  {
  private:
    enum FindRootModel
    {
      FindRootModel_Patient,
      FindRootModel_Study,
      FindRootModel_Series,
      FindRootModel_Instance
    };

    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    // Connection parameters
    std::string localAet_;
    std::string distantAet_;
    std::string distantHost_;
    uint16_t distantPort_;

    void CheckIsOpen() const;

    void SetupPresentationContexts();

    void Find(DicomFindAnswers& result,
              FindRootModel model,
              const DicomMap& fields);

    void Move(const std::string& targetAet,
              const DicomMap& fields);

  public:
    DicomUserConnection();

    ~DicomUserConnection();

    void CopyParameters(const DicomUserConnection& other);

    void SetLocalApplicationEntityTitle(const std::string& aet);

    const std::string& GetLocalApplicationEntityTitle() const
    {
      return localAet_;
    }

    void SetDistantApplicationEntityTitle(const std::string& aet);

    const std::string& GetDistantApplicationEntityTitle() const
    {
      return distantAet_;
    }

    void SetDistantHost(const std::string& host);

    const std::string& GetDistantHost() const
    {
      return distantHost_;
    }

    void SetDistantPort(uint16_t port);

    uint16_t GetDistantPort() const
    {
      return distantPort_;
    }

    void Open();

    void Close();

    bool IsOpen() const;

    bool Echo();

    void Store(const char* buffer, size_t size);

    void Store(const std::string& buffer);

    void StoreFile(const std::string& path);

    void FindPatient(DicomFindAnswers& result,
                     const DicomMap& fields);

    void FindStudy(DicomFindAnswers& result,
                   const DicomMap& fields);

    void FindSeries(DicomFindAnswers& result,
                    const DicomMap& fields);

    void FindInstance(DicomFindAnswers& result,
                      const DicomMap& fields);

    void MoveSeries(const std::string& targetAet,
                    const DicomMap& findResult);

    void MoveSeries(const std::string& targetAet,
                    const std::string& studyUid,
                    const std::string& seriesUid);

    void MoveInstance(const std::string& targetAet,
                      const DicomMap& findResult);

    void MoveInstance(const std::string& targetAet,
                      const std::string& studyUid,
                      const std::string& seriesUid,
                      const std::string& instanceUid);

    static void SetConnectionTimeout(uint32_t seconds);
  };
}
