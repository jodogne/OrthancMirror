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


  TimeoutDicomConnectionManager::Lock::Lock(TimeoutDicomConnectionManager& that,
                                            const std::string& localAet,
                                            const RemoteModalityParameters& remote) : 
    that_(that),
    lock_(that_.mutex_)
  {
    // Calling "Touch()" will be done by the "~Lock()" destructor
    that_.OpenInternal(localAet, remote);
  }

  
  TimeoutDicomConnectionManager::Lock::~Lock()
  {
    that_.TouchInternal();
  }

  
  DicomStoreUserConnection& TimeoutDicomConnectionManager::Lock::GetConnection()
  {
    if (that_.connection_.get() == NULL)
    {
      // The allocation should have been done by "that_.Open()" in the constructor
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      return *that_.connection_;
    }
  }


  // Mutex must be locked
  void TimeoutDicomConnectionManager::TouchInternal()
  {
    lastUse_ = GetNow();
  }


  // Mutex must be locked
  void TimeoutDicomConnectionManager::OpenInternal(const std::string& localAet,
                                                   const RemoteModalityParameters& remote)
  {
    DicomAssociationParameters other(localAet, remote);
    
    if (connection_.get() == NULL ||
        !connection_->GetParameters().IsEqual(other))
    {
      connection_.reset(new DicomStoreUserConnection(other));
    }
  }


  // Mutex must be locked
  void TimeoutDicomConnectionManager::CloseInternal()
  {
    if (connection_.get() != NULL)
    {
      LOG(INFO) << "Closing inactive DICOM association with modality: "
                << connection_->GetParameters().GetRemoteModality().GetApplicationEntityTitle();

      connection_.reset(NULL);
    }
  }


  void TimeoutDicomConnectionManager::SetInactivityTimeout(unsigned int milliseconds)
  {
    boost::mutex::scoped_lock lock(mutex_);
    timeout_ = boost::posix_time::milliseconds(milliseconds);
    CloseInternal();
  }


  unsigned int TimeoutDicomConnectionManager::GetInactivityTimeout()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return static_cast<unsigned int>(timeout_.total_milliseconds());
  }


  void TimeoutDicomConnectionManager::CloseIfInactive()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (connection_.get() != NULL &&
        (GetNow() - lastUse_) >= timeout_)
    {
      CloseInternal();
    }
  }
}
