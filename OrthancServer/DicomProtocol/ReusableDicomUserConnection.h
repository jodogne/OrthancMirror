/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "DicomUserConnection.h"
#include "../../Core/MultiThreading/Locker.h"

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace Orthanc
{
  class ReusableDicomUserConnection : public ILockable
  {
  private:
    boost::mutex mutex_;
    DicomUserConnection* connection_;
    bool continue_;
    boost::posix_time::time_duration timeBeforeClose_;
    boost::posix_time::ptime lastUse_;
    boost::thread closeThread_;
    std::string localAet_;

    void Open(const std::string& remoteAet,
              const std::string& address,
              int port,
              ModalityManufacturer manufacturer);
    
    void Close();

    static void CloseThread(ReusableDicomUserConnection* that);

  protected:
    virtual void Lock();

    virtual void Unlock();
    
  public:
    class Locker : public ::Orthanc::Locker
    {
    private:
      DicomUserConnection* connection_;

    public:
      Locker(ReusableDicomUserConnection& that,
             const RemoteModalityParameters& remote);

      Locker(ReusableDicomUserConnection& that,
             const std::string& aet,
             const std::string& address,
             int port,
             ModalityManufacturer manufacturer);

      DicomUserConnection& GetConnection();
    };

    ReusableDicomUserConnection();

    virtual ~ReusableDicomUserConnection();

    unsigned int GetMillisecondsBeforeClose() const
    {
      return timeBeforeClose_.total_milliseconds();
    }

    void SetMillisecondsBeforeClose(unsigned int ms);

    const std::string& GetLocalApplicationEntityTitle() const;

    void SetLocalApplicationEntityTitle(const std::string& aet);
  };
}

