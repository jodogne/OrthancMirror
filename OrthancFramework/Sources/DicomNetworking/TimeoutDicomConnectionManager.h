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

#if !defined(ORTHANC_ENABLE_DCMTK_NETWORKING)
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be defined
#endif

#if ORTHANC_ENABLE_DCMTK_NETWORKING != 1
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be 1 to use this file
#endif


#include "../Compatibility.h"
#include "DicomStoreUserConnection.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>

namespace Orthanc
{
  /**
   * This class corresponds to a singleton to a DICOM SCU connection.
   **/
  class TimeoutDicomConnectionManager : public boost::noncopyable
  {
  private:
    boost::mutex                               mutex_;
    std::unique_ptr<DicomStoreUserConnection>  connection_;
    boost::posix_time::ptime                   lastUse_;
    boost::posix_time::time_duration           timeout_;

    // Mutex must be locked
    void TouchInternal();

    // Mutex must be locked
    void OpenInternal(const std::string& localAet,
                      const RemoteModalityParameters& remote);

    // Mutex must be locked
    void CloseInternal();

  public:
    class Lock : public boost::noncopyable
    {
    private:
      TimeoutDicomConnectionManager&  that_;
      boost::mutex::scoped_lock       lock_;

    public:
      Lock(TimeoutDicomConnectionManager& that,
           const std::string& localAet,
           const RemoteModalityParameters& remote);
      
      ~Lock();

      DicomStoreUserConnection& GetConnection();
    };

    TimeoutDicomConnectionManager() :
      timeout_(boost::posix_time::milliseconds(1000))
    {
    }

    void SetInactivityTimeout(unsigned int milliseconds);

    unsigned int GetInactivityTimeout();  // In milliseconds

    void Close();

    void CloseIfInactive();
  };
}
