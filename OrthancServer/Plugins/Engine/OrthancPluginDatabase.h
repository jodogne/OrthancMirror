/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <boost/thread/recursive_mutex.hpp>

namespace Orthanc
{
  /**
   * This class is for backward compatibility with database plugins
   * that don't use the primitives introduced in Orthanc 1.9.2 to deal
   * with concurrent read-only transactions.
   *
   * In Orthanc <= 1.9.1, Orthanc assumed that at most 1 single thread
   * was accessing the database plugin at anytime, in order to match
   * the SQLite model. Read-write accesses assumed the plugin to run
   * the SQL statement "START TRANSACTION SERIALIZABLE" so as to be
   * able to rollback the modifications. Read-only accesses didn't
   * start a transaction, as they were protected by the global mutex.
   **/
  class OrthancPluginDatabase : public BaseDatabaseWrapper
  {
  private:
    class Transaction;

    /**
     * We need a "recursive_mutex" because of "AnswerReceived()" that
     * is called by the "answer" primitives of the database SDK once a
     * transaction is running.
     **/
    boost::recursive_mutex          mutex_;
    
    SharedLibrary&                  library_;
    PluginsErrorDictionary&         errorDictionary_;
    OrthancPluginDatabaseBackend    backend_;
    OrthancPluginDatabaseExtensions extensions_;
    void*                           payload_;
    Transaction*                    activeTransaction_;
    bool                            fastGetTotalSize_;
    uint64_t                        currentDiskSize_;
    IDatabaseWrapper::Capabilities  dbCapabilities_;

    OrthancPluginDatabaseContext* GetContext()
    {
      return reinterpret_cast<OrthancPluginDatabaseContext*>(this);
    }

    void CheckSuccess(OrthancPluginErrorCode code);

  public:
    OrthancPluginDatabase(SharedLibrary& library,
                          PluginsErrorDictionary&  errorDictionary,
                          const OrthancPluginDatabaseBackend& backend,
                          const OrthancPluginDatabaseExtensions* extensions,
                          size_t extensionsSize,
                          void *payload);

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

    virtual const Capabilities GetDatabaseCapabilities() const ORTHANC_OVERRIDE
    {
      return dbCapabilities_;
    }

    virtual uint64_t MeasureLatency() ORTHANC_OVERRIDE
    {
      throw OrthancException(ErrorCode_NotImplemented);  // only implemented in V4
    }

    void AnswerReceived(const _OrthancPluginDatabaseAnswer& answer);
  };
}

#endif
