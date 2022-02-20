/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

/**
 * NB: Conversions to/from "OrthancPluginConstraintType" and
 * "OrthancPluginResourceType" are located in file
 * "../../Sources/Search/DatabaseConstraint.h" to be shared with the
 * "orthanc-databases" project.
 **/

#include "../../Sources/Search/DatabaseConstraint.h"
#include "../../Sources/ServerEnumerations.h"
#include "../Include/orthanc/OrthancCPlugin.h"

namespace Orthanc
{
  namespace Compatibility
  {
    OrthancPluginIdentifierConstraint Convert(IdentifierConstraintType constraint);

    IdentifierConstraintType Convert(OrthancPluginIdentifierConstraint constraint);
  }

  namespace Plugins
  {
    OrthancPluginChangeType Convert(ChangeType type);

    OrthancPluginPixelFormat Convert(PixelFormat format);

    PixelFormat Convert(OrthancPluginPixelFormat format);

    OrthancPluginContentType Convert(FileContentType type);

    FileContentType Convert(OrthancPluginContentType type);

    DicomToJsonFormat Convert(OrthancPluginDicomToJsonFormat format);

    OrthancPluginInstanceOrigin Convert(RequestOrigin origin);

    OrthancPluginHttpMethod Convert(HttpMethod method);

    ValueRepresentation Convert(OrthancPluginValueRepresentation vr);

    OrthancPluginValueRepresentation Convert(ValueRepresentation vr);

    OrthancPluginJobStepStatus Convert(JobStepCode step);

    JobStepCode Convert(OrthancPluginJobStepStatus step);

    StorageCommitmentFailureReason Convert(OrthancPluginStorageCommitmentFailureReason reason);
  }
}

#endif
