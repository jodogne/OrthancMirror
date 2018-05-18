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


#include "../PrecompiledHeaders.h"
#include "TimeoutDicomConnectionManager.h"

#include "../OrthancException.h"

namespace Orthanc
{
  static boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::microsec_clock::universal_time();
  }

  class TimeoutDicomConnectionManager::Resource : public IDicomConnectionManager::IResource
  {
  private:
    TimeoutDicomConnectionManager&  that_;
    boost::mutex::scoped_lock        lock_;

  public:
    Resource(TimeoutDicomConnectionManager& that) : 
    that_(that),
    lock_(that.mutex_)
    {
    }

    ~Resource()
    {
      that_.Touch();
    }

    DicomUserConnection& GetConnection()
    {
      if (that_.connection_.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return *that_.connection_;
    }
  };


  void TimeoutDicomConnectionManager::Touch()
  {
    lastUse_ = GetNow();
  }


  void TimeoutDicomConnectionManager::CheckTimeoutInternal()
  {
    if (connection_.get() != NULL &&
        (GetNow() - lastUse_) >= timeout_)
    {
      connection_.reset(NULL);
    }
  }


  void TimeoutDicomConnectionManager::SetTimeout(unsigned int timeout)
  {
    boost::mutex::scoped_lock lock(mutex_);

    timeout_ = boost::posix_time::milliseconds(timeout);
    CheckTimeoutInternal();
  }


  unsigned int TimeoutDicomConnectionManager::GetTimeout()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return timeout_.total_milliseconds();
  }


  void TimeoutDicomConnectionManager::Close()
  {
    boost::mutex::scoped_lock lock(mutex_);
    connection_.reset(NULL);
  }


  void TimeoutDicomConnectionManager::CheckTimeout()
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckTimeoutInternal();
  }


  IDicomConnectionManager::IResource* 
  TimeoutDicomConnectionManager::AcquireConnection(const std::string& localAet,
                                                   const RemoteModalityParameters& remote)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (connection_.get() == NULL ||
        !connection_->IsSameAssociation(localAet, remote))
    {
      connection_.reset(new DicomUserConnection(localAet, remote));
    }

    return new Resource(*this);
  }
}
