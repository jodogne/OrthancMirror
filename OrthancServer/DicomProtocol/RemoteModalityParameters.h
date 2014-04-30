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

#include "../ServerEnumerations.h"

#include <string>

namespace Orthanc
{
  class RemoteModalityParameters
  {
    // TODO Use the flyweight pattern for this class

  private:
    std::string  symbolicName_;
    std::string  aet_;
    std::string  host_;
    int  port_;
    ModalityManufacturer  manufacturer_;

  public:
    RemoteModalityParameters() :
      symbolicName_(""),
      aet_(""),
      host_(""),
      port_(104),
      manufacturer_(ModalityManufacturer_Generic)
    {
    }

    RemoteModalityParameters(const std::string& symbolic,
                             const std::string& aet,
                             const std::string& host,
                             int port,
                             ModalityManufacturer manufacturer) :
      symbolicName_(symbolic),
      aet_(aet),
      host_(host),
      port_(port),
      manufacturer_(manufacturer)
    {
    }

    RemoteModalityParameters(const std::string& aet,
                             const std::string& host,
                             int port,
                             ModalityManufacturer manufacturer) :
      symbolicName_(""),
      aet_(aet),
      host_(host),
      port_(port),
      manufacturer_(manufacturer)
    {
    }

    const std::string& GetSymbolicName() const
    {
      return symbolicName_;
    }

    const std::string& GetApplicationEntityTitle() const
    {
      return aet_;
    }

    const std::string& GetHost() const
    {
      return host_;
    }

    int GetPort() const
    {
      return port_;
    }

    ModalityManufacturer GetManufacturer() const
    {
      return manufacturer_;
    }
  };
}
