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


#include "GenericFind.h"

#include "../../../../OrthancFramework/Sources/OrthancException.h"


namespace Orthanc
{
  namespace Compatibility
  {
    void GenericFind::Execute(FindResponse& response,
                              const FindRequest& request)
    {
      if (!request.GetOrthancIdentifiers().HasPatientId() &&
          !request.GetOrthancIdentifiers().HasStudyId() &&
          !request.GetOrthancIdentifiers().HasSeriesId() &&
          !request.GetOrthancIdentifiers().HasInstanceId() &&
          request.GetDicomTagConstraintsCount() == 0 &&
          request.GetMetadataConstraintsCount() == 0 &&
          !request.IsRetrieveTagsAtLevel(ResourceType_Patient) &&
          !request.IsRetrieveTagsAtLevel(ResourceType_Study) &&
          !request.IsRetrieveTagsAtLevel(ResourceType_Series) &&
          !request.IsRetrieveTagsAtLevel(ResourceType_Instance) &&
          request.GetOrdering().empty() &&
          request.GetLabels().empty())
      {
        std::list<std::string> ids;

        if (request.HasLimits())
        {
          transaction_.GetAllPublicIds(ids, request.GetLevel(), request.GetLimitsSince(), request.GetLimitsCount());
        }
        else
        {
          transaction_.GetAllPublicIds(ids, request.GetLevel());
        }

        for (std::list<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++it)
        {
          response.Add(new FindResponse::Resource(request.GetLevel(), *it));
        }
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }


      /**
       * Sanity checks
       **/

      for (size_t i = 0; i < response.GetSize(); i++)
      {
        const FindResponse::Resource& resource = response.GetResource(i);

        if (request.IsRetrieveTagsAtLevel(request.GetLevel()))
        {
          DicomMap tmp;
          resource.GetDicomTagsAtLevel(tmp, request.GetLevel());
          if (tmp.GetSize() == 0)
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }

        // TODO: other sanity checks
      }
    }
  }
}
