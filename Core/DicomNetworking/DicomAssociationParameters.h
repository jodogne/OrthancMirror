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


#pragma once

#include "RemoteModalityParameters.h"

class OFCondition;  // From DCMTK

namespace Orthanc
{
  class DicomAssociationParameters
  {
  private:
    std::string           localAet_;
    std::string           remoteAet_;
    std::string           remoteHost_;
    uint16_t              remotePort_;
    ModalityManufacturer  manufacturer_;
    uint32_t              timeout_;

    void ReadDefaultTimeout();

  public:
    DicomAssociationParameters();
    
    DicomAssociationParameters(const std::string& localAet,
                               const RemoteModalityParameters& remote);
    
    const std::string& GetLocalApplicationEntityTitle() const
    {
      return localAet_;
    }

    const std::string& GetRemoteApplicationEntityTitle() const
    {
      return remoteAet_;
    }

    const std::string& GetRemoteHost() const
    {
      return remoteHost_;
    }

    uint16_t GetRemotePort() const
    {
      return remotePort_;
    }

    ModalityManufacturer GetRemoteManufacturer() const
    {
      return manufacturer_;
    }

    void SetLocalApplicationEntityTitle(const std::string& aet)
    {
      localAet_ = aet;
    }

    void SetRemoteApplicationEntityTitle(const std::string& aet)
    {
      remoteAet_ = aet;
    }

    void SetRemoteHost(const std::string& host);

    void SetRemotePort(uint16_t port)
    {
      remotePort_ = port;
    }

    void SetRemoteManufacturer(ModalityManufacturer manufacturer)
    {
      manufacturer_ = manufacturer;
    }

    void SetRemoteModality(const RemoteModalityParameters& parameters);

    bool IsEqual(const DicomAssociationParameters& other) const;

    void SetTimeout(uint32_t seconds)
    {
      timeout_ = seconds;
    }

    uint32_t GetTimeout() const
    {
      return timeout_;
    }

    bool HasTimeout() const
    {
      return timeout_ != 0;
    }
    
    static void SetDefaultTimeout(uint32_t seconds);

    static size_t GetMaxHostNameSize();
  };
}
