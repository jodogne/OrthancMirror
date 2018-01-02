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


#pragma once

#if ORTHANC_ENABLE_DCMTK_NETWORKING != 1
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be set to 1
#endif

#include "DicomFindAnswers.h"
#include "../Enumerations.h"
#include "RemoteModalityParameters.h"

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <list>

namespace Orthanc
{
  class DicomUserConnection : public boost::noncopyable
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    // Connection parameters
    std::string preferredTransferSyntax_;
    std::string localAet_;
    std::string remoteAet_;
    std::string remoteHost_;
    uint16_t remotePort_;
    ModalityManufacturer manufacturer_;
    std::set<std::string> storageSOPClasses_;
    std::list<std::string> reservedStorageSOPClasses_;
    std::set<std::string> defaultStorageSOPClasses_;

    void CheckIsOpen() const;

    void SetupPresentationContexts(const std::string& preferredTransferSyntax);

    void MoveInternal(const std::string& targetAet,
                      ResourceType level,
                      const DicomMap& fields);

    void ResetStorageSOPClasses();

    void CheckStorageSOPClassesInvariant() const;

  public:
    DicomUserConnection();

    ~DicomUserConnection();

    void SetRemoteModality(const RemoteModalityParameters& parameters);

    void SetLocalApplicationEntityTitle(const std::string& aet);

    const std::string& GetLocalApplicationEntityTitle() const
    {
      return localAet_;
    }

    void SetRemoteApplicationEntityTitle(const std::string& aet);

    const std::string& GetRemoteApplicationEntityTitle() const
    {
      return remoteAet_;
    }

    void SetRemoteHost(const std::string& host);

    const std::string& GetRemoteHost() const
    {
      return remoteHost_;
    }

    void SetRemotePort(uint16_t port);

    uint16_t GetRemotePort() const
    {
      return remotePort_;
    }

    void SetRemoteManufacturer(ModalityManufacturer manufacturer);

    ModalityManufacturer GetRemoteManufacturer() const
    {
      return manufacturer_;
    }

    void ResetPreferredTransferSyntax();

    void SetPreferredTransferSyntax(const std::string& preferredTransferSyntax);

    const std::string& GetPreferredTransferSyntax() const
    {
      return preferredTransferSyntax_;
    }

    void AddStorageSOPClass(const char* sop);

    void Open();

    void Close();

    bool IsOpen() const;

    bool Echo();

    void Store(const char* buffer, 
               size_t size,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void Store(const char* buffer, 
               size_t size)
    {
      Store(buffer, size, "", 0);  // Not a C-Move
    }

    void Store(const std::string& buffer,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void Store(const std::string& buffer)
    {
      Store(buffer, "", 0);  // Not a C-Move
    }

    void StoreFile(const std::string& path,
                   const std::string& moveOriginatorAET,
                   uint16_t moveOriginatorID);

    void StoreFile(const std::string& path)
    {
      StoreFile(path, "", 0);  // Not a C-Move
    }

    void Find(DicomFindAnswers& result,
              ResourceType level,
              const DicomMap& fields);

    void Move(const std::string& targetAet,
              ResourceType level,
              const DicomMap& findResult);

    void Move(const std::string& targetAet,
              const DicomMap& findResult);

    void MovePatient(const std::string& targetAet,
                     const std::string& patientId);

    void MoveStudy(const std::string& targetAet,
                   const std::string& studyUid);

    void MoveSeries(const std::string& targetAet,
                    const std::string& studyUid,
                    const std::string& seriesUid);

    void MoveInstance(const std::string& targetAet,
                      const std::string& studyUid,
                      const std::string& seriesUid,
                      const std::string& instanceUid);

    void SetTimeout(uint32_t seconds);

    void DisableTimeout();

    void FindWorklist(DicomFindAnswers& result,
                      ParsedDicomFile& query);

    static void SetDefaultTimeout(uint32_t seconds);
  };
}
