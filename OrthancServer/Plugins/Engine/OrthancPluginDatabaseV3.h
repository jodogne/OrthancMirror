/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../../../OrthancFramework/Sources/SharedLibrary.h"
#include "../../Sources/Database/BaseDatabaseWrapper.h"
#include "../Include/orthanc/OrthancCDatabasePlugin.h"
#include "PluginsErrorDictionary.h"

namespace Orthanc
{
  class OrthancPluginDatabaseV3 : public BaseDatabaseWrapper
  {
  private:
    class Transaction;

    SharedLibrary&                  library_;
    PluginsErrorDictionary&         errorDictionary_;
    OrthancPluginDatabaseBackendV3  backend_;
    void*                           database_;
    std::string                     serverIdentifier_;
    IDatabaseWrapper::Capabilities  dbCapabilities_;

    void CheckSuccess(OrthancPluginErrorCode code) const;

  public:
    OrthancPluginDatabaseV3(SharedLibrary& library,
                            PluginsErrorDictionary&  errorDictionary,
                            const OrthancPluginDatabaseBackendV3* backend,
                            size_t backendSize,
                            void* database,
                            const std::string& serverIdentifier);

    virtual ~OrthancPluginDatabaseV3();

    virtual void Open() ORTHANC_OVERRIDE;

    virtual void Close() ORTHANC_OVERRIDE;

    const SharedLibrary& GetSharedLibrary() const
    {
      return library_;
    }

    virtual void FlushToDisk() ORTHANC_OVERRIDE
    {
    }

    virtual IDatabaseWrapper::ITransaction* StartTransaction(TransactionType type,
                                                             IDatabaseListener& listener)
      ORTHANC_OVERRIDE;

    virtual unsigned int GetDatabaseVersion() ORTHANC_OVERRIDE;

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea) ORTHANC_OVERRIDE;    

    const IDatabaseWrapper::Capabilities& GetDatabaseCapabilities() const ORTHANC_OVERRIDE
    {
      return dbCapabilities_;
    }

  };
}

#endif
