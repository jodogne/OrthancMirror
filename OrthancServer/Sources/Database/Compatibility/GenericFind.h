/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../IDatabaseWrapper.h"

namespace Orthanc
{
  namespace Compatibility
  {
    class GenericFind : public boost::noncopyable
    {
    private:
      IDatabaseWrapper::ITransaction&  transaction_;
      IDatabaseWrapper::ICompatibilityTransaction&  compatibilityTransaction_;

      void RetrieveMainDicomTags(FindResponse::Resource& target,
                                 ResourceType level,
                                 int64_t internalId);

    public:
      explicit GenericFind(IDatabaseWrapper::ITransaction& transaction,
                           IDatabaseWrapper::ICompatibilityTransaction& compatibilityTransaction) :
        transaction_(transaction),
        compatibilityTransaction_(compatibilityTransaction)
      {
      }

      void ExecuteFind(std::list<std::string>& identifiers,
                       const IDatabaseWrapper::Capabilities& capabilities,
                       const FindRequest& request);

      void ExecuteExpand(FindResponse& response,
                         const IDatabaseWrapper::Capabilities& capabilities,
                         const FindRequest& request,
                         const std::string& identifier);
    };
  }
}
