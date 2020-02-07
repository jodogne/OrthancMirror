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


#include "../PrecompiledHeaders.h"
#include "TimeoutDicomConnectionManager.h"

#include "../Logging.h"
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

  public:
    Resource(TimeoutDicomConnectionManager& that) : 
      that_(that)
    {
      if (that_.connection_.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    ~Resource()
    {
      that_.Touch();
    }

    DicomUserConnection& GetConnection()
    {
      assert(that_.connection_.get() != NULL);
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
      Close();
    }
  }


  void TimeoutDicomConnectionManager::SetTimeout(unsigned int timeout)
  {
    timeout_ = boost::posix_time::milliseconds(timeout);
    CheckTimeoutInternal();
  }


  unsigned int TimeoutDicomConnectionManager::GetTimeout()
  {
    return static_cast<unsigned int>(timeout_.total_milliseconds());
  }


  void TimeoutDicomConnectionManager::Close()
  {
    if (connection_.get() != NULL)
    {
      LOG(INFO) << "Closing inactive DICOM association with modality: "
                << connection_->GetRemoteApplicationEntityTitle();

      connection_.reset(NULL);
    }
  }


  void TimeoutDicomConnectionManager::CheckTimeout()
  {
    CheckTimeoutInternal();
  }


  IDicomConnectionManager::IResource* 
  TimeoutDicomConnectionManager::AcquireConnection(const std::string& localAet,
                                                   const RemoteModalityParameters& remote)
  {
    if (connection_.get() == NULL ||
        !connection_->IsSameAssociation(localAet, remote))
    {
      connection_.reset(new DicomUserConnection(localAet, remote));
    }

    return new Resource(*this);
  }
}
