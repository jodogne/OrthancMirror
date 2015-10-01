/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "PluginsEnumerations.h"

#if ORTHANC_PLUGINS_ENABLED != 1
#error The plugin support is disabled
#endif


#include "../../Core/OrthancException.h"

namespace Orthanc
{
  namespace Plugins
  {
    OrthancPluginResourceType Convert(ResourceType type)
    {
      switch (type)
      {
        case ResourceType_Patient:
          return OrthancPluginResourceType_Patient;

        case ResourceType_Study:
          return OrthancPluginResourceType_Study;

        case ResourceType_Series:
          return OrthancPluginResourceType_Series;

        case ResourceType_Instance:
          return OrthancPluginResourceType_Instance;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    ResourceType Convert(OrthancPluginResourceType type)
    {
      switch (type)
      {
        case OrthancPluginResourceType_Patient:
          return ResourceType_Patient;

        case OrthancPluginResourceType_Study:
          return ResourceType_Study;

        case OrthancPluginResourceType_Series:
          return ResourceType_Series;

        case OrthancPluginResourceType_Instance:
          return ResourceType_Instance;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginChangeType Convert(ChangeType type)
    {
      switch (type)
      {
        case ChangeType_CompletedSeries:
          return OrthancPluginChangeType_CompletedSeries;

        case ChangeType_Deleted:
          return OrthancPluginChangeType_Deleted;

        case ChangeType_NewChildInstance:
          return OrthancPluginChangeType_NewChildInstance;

        case ChangeType_NewInstance:
          return OrthancPluginChangeType_NewInstance;

        case ChangeType_NewPatient:
          return OrthancPluginChangeType_NewPatient;

        case ChangeType_NewSeries:
          return OrthancPluginChangeType_NewSeries;

        case ChangeType_NewStudy:
          return OrthancPluginChangeType_NewStudy;

        case ChangeType_StablePatient:
          return OrthancPluginChangeType_StablePatient;

        case ChangeType_StableSeries:
          return OrthancPluginChangeType_StableSeries;

        case ChangeType_StableStudy:
          return OrthancPluginChangeType_StableStudy;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginPixelFormat Convert(PixelFormat format)
    {
      switch (format)
      {
        case PixelFormat_Grayscale16:
          return OrthancPluginPixelFormat_Grayscale16;

        case PixelFormat_Grayscale8:
          return OrthancPluginPixelFormat_Grayscale8;

        case PixelFormat_RGB24:
          return OrthancPluginPixelFormat_RGB24;

        case PixelFormat_RGBA32:
          return OrthancPluginPixelFormat_RGBA32;

        case PixelFormat_SignedGrayscale16:
          return OrthancPluginPixelFormat_SignedGrayscale16;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    PixelFormat Convert(OrthancPluginPixelFormat format)
    {
      switch (format)
      {
        case OrthancPluginPixelFormat_Grayscale16:
          return PixelFormat_Grayscale16;

        case OrthancPluginPixelFormat_Grayscale8:
          return PixelFormat_Grayscale8;

        case OrthancPluginPixelFormat_RGB24:
          return PixelFormat_RGB24;

        case OrthancPluginPixelFormat_RGBA32:
          return PixelFormat_RGBA32;

        case OrthancPluginPixelFormat_SignedGrayscale16:
          return PixelFormat_SignedGrayscale16;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginContentType Convert(FileContentType type)
    {
      switch (type)
      {
        case FileContentType_Dicom:
          return OrthancPluginContentType_Dicom;

        case FileContentType_DicomAsJson:
          return OrthancPluginContentType_DicomAsJson;

        default:
          return OrthancPluginContentType_Unknown;
      }
    }


    FileContentType Convert(OrthancPluginContentType type)
    {
      switch (type)
      {
        case OrthancPluginContentType_Dicom:
          return FileContentType_Dicom;

        case OrthancPluginContentType_DicomAsJson:
          return FileContentType_DicomAsJson;

        default:
          return FileContentType_Unknown;
      }
    }



    DcmEVR Convert(OrthancPluginValueRepresentation vr)
    {
      switch (vr)
      {
        case OrthancPluginValueRepresentation_AE:
          return EVR_AE;

        case OrthancPluginValueRepresentation_AS:
          return EVR_AS;

        case OrthancPluginValueRepresentation_AT:
          return EVR_AT;

        case OrthancPluginValueRepresentation_CS:
          return EVR_CS;

        case OrthancPluginValueRepresentation_DA:
          return EVR_DA;

        case OrthancPluginValueRepresentation_DS:
          return EVR_DS;

        case OrthancPluginValueRepresentation_DT:
          return EVR_DT;

        case OrthancPluginValueRepresentation_FD:
          return EVR_FD;

        case OrthancPluginValueRepresentation_FL:
          return EVR_FL;

        case OrthancPluginValueRepresentation_IS:
          return EVR_IS;

        case OrthancPluginValueRepresentation_LO:
          return EVR_LO;

        case OrthancPluginValueRepresentation_LT:
          return EVR_LT;

        case OrthancPluginValueRepresentation_OB:
          return EVR_OB;

        case OrthancPluginValueRepresentation_OF:
          return EVR_OF;

        case OrthancPluginValueRepresentation_OW:
          return EVR_OW;

        case OrthancPluginValueRepresentation_PN:
          return EVR_PN;

        case OrthancPluginValueRepresentation_SH:
          return EVR_SH;

        case OrthancPluginValueRepresentation_SL:
          return EVR_SL;

        case OrthancPluginValueRepresentation_SQ:
          return EVR_SQ;

        case OrthancPluginValueRepresentation_SS:
          return EVR_SS;

        case OrthancPluginValueRepresentation_ST:
          return EVR_ST;

        case OrthancPluginValueRepresentation_TM:
          return EVR_TM;

        case OrthancPluginValueRepresentation_UI:
          return EVR_UI;

        case OrthancPluginValueRepresentation_UL:
          return EVR_UL;

        case OrthancPluginValueRepresentation_UN:
          return EVR_UN;

        case OrthancPluginValueRepresentation_US:
          return EVR_US;

        case OrthancPluginValueRepresentation_UT:
          return EVR_UT;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }
}
