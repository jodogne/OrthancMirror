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
#include "DicomAssociationParameters.h"


#ifdef _WIN32
/**
 * "The maximum length, in bytes, of the string returned in the buffer 
 * pointed to by the name parameter is dependent on the namespace provider,
 * but this string must be 256 bytes or less.
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738527(v=vs.85).aspx
 **/
#  define HOST_NAME_MAX 256
#  include <winsock.h>
#endif 


#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
/**
 * TO IMPROVE: "_POSIX_HOST_NAME_MAX is only the minimum value that
 * HOST_NAME_MAX can ever have [...] Therefore you cannot allocate an
 * array of size _POSIX_HOST_NAME_MAX, invoke gethostname() and expect
 * that the result will fit."
 * http://lists.gnu.org/archive/html/bug-gnulib/2009-08/msg00128.html
 **/
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif


#include "../Logging.h"
#include "../OrthancException.h"

#include <boost/thread/mutex.hpp>

// By default, the timeout for client DICOM connections is set to 10 seconds
static boost::mutex  defaultTimeoutMutex_;
static uint32_t defaultTimeout_ = 10;


namespace Orthanc
{
  void DicomAssociationParameters::ReadDefaultTimeout()
  {
    boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
    timeout_ = defaultTimeout_;
  }


  DicomAssociationParameters::DicomAssociationParameters() :
    localAet_("STORESCU"),
    remoteAet_("ANY-SCP"),
    remoteHost_("127.0.0.1"),
    remotePort_(104),
    manufacturer_(ModalityManufacturer_Generic)
  {
    ReadDefaultTimeout();
  }

    
  DicomAssociationParameters::DicomAssociationParameters(const std::string& localAet,
                                                         const RemoteModalityParameters& remote) :
    localAet_(localAet),
    remoteAet_(remote.GetApplicationEntityTitle()),
    remoteHost_(remote.GetHost()),
    remotePort_(remote.GetPortNumber()),
    manufacturer_(remote.GetManufacturer()),
    timeout_(defaultTimeout_)
  {
    ReadDefaultTimeout();
  }

    
  void DicomAssociationParameters::SetRemoteHost(const std::string& host)
  {
    if (host.size() > HOST_NAME_MAX - 10)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Invalid host name (too long): " + host);
    }

    remoteHost_ = host;
  }


  void DicomAssociationParameters::SetRemoteModality(const RemoteModalityParameters& parameters)
  {
    SetRemoteApplicationEntityTitle(parameters.GetApplicationEntityTitle());
    SetRemoteHost(parameters.GetHost());
    SetRemotePort(parameters.GetPortNumber());
    SetRemoteManufacturer(parameters.GetManufacturer());
  }


  bool DicomAssociationParameters::IsEqual(const DicomAssociationParameters& other) const
  {
    return (localAet_ == other.localAet_ &&
            remoteAet_ == other.remoteAet_ &&
            remoteHost_ == other.remoteHost_ &&
            remotePort_ == other.remotePort_ &&
            manufacturer_ == other.manufacturer_);
  }

    
  void DicomAssociationParameters::SetDefaultTimeout(uint32_t seconds)
  {
    LOG(INFO) << "Default timeout for DICOM connections if Orthanc acts as SCU (client): " 
              << seconds << " seconds (0 = no timeout)";

    {
      boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
      defaultTimeout_ = seconds;
    }
  }


  size_t DicomAssociationParameters::GetMaxHostNameSize()
  {
    return HOST_NAME_MAX;
  }  
}
