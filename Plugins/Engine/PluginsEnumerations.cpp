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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "PluginsEnumerations.h"

#if ORTHANC_ENABLE_PLUGINS != 1
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

        case ChangeType_UpdatedAttachment:
          return OrthancPluginChangeType_UpdatedAttachment;

        case ChangeType_UpdatedMetadata:
          return OrthancPluginChangeType_UpdatedMetadata;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginPixelFormat Convert(PixelFormat format)
    {
      switch (format)
      {
        case PixelFormat_BGRA32:
          return OrthancPluginPixelFormat_BGRA32;

        case PixelFormat_Float32:
          return OrthancPluginPixelFormat_Float32;

        case PixelFormat_Grayscale16:
          return OrthancPluginPixelFormat_Grayscale16;

        case PixelFormat_Grayscale32:
          return OrthancPluginPixelFormat_Grayscale32;

        case PixelFormat_Grayscale8:
          return OrthancPluginPixelFormat_Grayscale8;

        case PixelFormat_RGB24:
          return OrthancPluginPixelFormat_RGB24;

        case PixelFormat_RGB48:
          return OrthancPluginPixelFormat_RGB48;

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
        case OrthancPluginPixelFormat_BGRA32:
          return PixelFormat_BGRA32;

        case OrthancPluginPixelFormat_Float32:
          return PixelFormat_Float32;

        case OrthancPluginPixelFormat_Grayscale16:
          return PixelFormat_Grayscale16;

        case OrthancPluginPixelFormat_Grayscale32:
          return PixelFormat_Grayscale32;

        case OrthancPluginPixelFormat_Grayscale8:
          return PixelFormat_Grayscale8;

        case OrthancPluginPixelFormat_RGB24:
          return PixelFormat_RGB24;

        case OrthancPluginPixelFormat_RGB48:
          return PixelFormat_RGB48;

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


    DicomToJsonFormat Convert(OrthancPluginDicomToJsonFormat format)
    {
      switch (format)
      {
        case OrthancPluginDicomToJsonFormat_Full:
          return DicomToJsonFormat_Full;

        case OrthancPluginDicomToJsonFormat_Short:
          return DicomToJsonFormat_Short;

        case OrthancPluginDicomToJsonFormat_Human:
          return DicomToJsonFormat_Human;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginIdentifierConstraint Convert(IdentifierConstraintType constraint)
    {
      switch (constraint)
      {
        case IdentifierConstraintType_Equal:
          return OrthancPluginIdentifierConstraint_Equal;

        case IdentifierConstraintType_GreaterOrEqual:
          return OrthancPluginIdentifierConstraint_GreaterOrEqual;

        case IdentifierConstraintType_SmallerOrEqual:
          return OrthancPluginIdentifierConstraint_SmallerOrEqual;

        case IdentifierConstraintType_Wildcard:
          return OrthancPluginIdentifierConstraint_Wildcard;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    IdentifierConstraintType Convert(OrthancPluginIdentifierConstraint constraint)
    {
      switch (constraint)
      {
        case OrthancPluginIdentifierConstraint_Equal:
          return IdentifierConstraintType_Equal;

        case OrthancPluginIdentifierConstraint_GreaterOrEqual:
          return IdentifierConstraintType_GreaterOrEqual;

        case OrthancPluginIdentifierConstraint_SmallerOrEqual:
          return IdentifierConstraintType_SmallerOrEqual;

        case OrthancPluginIdentifierConstraint_Wildcard:
          return IdentifierConstraintType_Wildcard;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginInstanceOrigin Convert(RequestOrigin origin)
    {
      switch (origin)
      {
        case RequestOrigin_DicomProtocol:
          return OrthancPluginInstanceOrigin_DicomProtocol;

        case RequestOrigin_RestApi:
          return OrthancPluginInstanceOrigin_RestApi;

        case RequestOrigin_Lua:
          return OrthancPluginInstanceOrigin_Lua;

        case RequestOrigin_Plugins:
          return OrthancPluginInstanceOrigin_Plugin;

        case RequestOrigin_Unknown:
          return OrthancPluginInstanceOrigin_Unknown;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    OrthancPluginHttpMethod Convert(HttpMethod method)
    {
      switch (method)
      {
        case HttpMethod_Get:
          return OrthancPluginHttpMethod_Get;

        case HttpMethod_Post:
          return OrthancPluginHttpMethod_Post;

        case HttpMethod_Put:
          return OrthancPluginHttpMethod_Put;

        case HttpMethod_Delete:
          return OrthancPluginHttpMethod_Delete;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    ValueRepresentation Convert(OrthancPluginValueRepresentation vr)
    {
      switch (vr)
      {
        case OrthancPluginValueRepresentation_AE:
          return ValueRepresentation_ApplicationEntity;

        case OrthancPluginValueRepresentation_AS:
          return ValueRepresentation_AgeString;

        case OrthancPluginValueRepresentation_AT:
          return ValueRepresentation_AttributeTag;

        case OrthancPluginValueRepresentation_CS:
          return ValueRepresentation_CodeString;

        case OrthancPluginValueRepresentation_DA:
          return ValueRepresentation_Date;

        case OrthancPluginValueRepresentation_DS:
          return ValueRepresentation_DecimalString;

        case OrthancPluginValueRepresentation_DT:
          return ValueRepresentation_DateTime;

        case OrthancPluginValueRepresentation_FD:
          return ValueRepresentation_FloatingPointDouble;

        case OrthancPluginValueRepresentation_FL:
          return ValueRepresentation_FloatingPointSingle;

        case OrthancPluginValueRepresentation_IS:
          return ValueRepresentation_IntegerString;

        case OrthancPluginValueRepresentation_LO:
          return ValueRepresentation_LongString;

        case OrthancPluginValueRepresentation_LT:
          return ValueRepresentation_LongText;

        case OrthancPluginValueRepresentation_OB:
          return ValueRepresentation_OtherByte;

        case OrthancPluginValueRepresentation_OF:
          return ValueRepresentation_OtherFloat;

        case OrthancPluginValueRepresentation_OW:
          return ValueRepresentation_OtherWord;

        case OrthancPluginValueRepresentation_PN:
          return ValueRepresentation_PersonName;

        case OrthancPluginValueRepresentation_SH:
          return ValueRepresentation_ShortString;

        case OrthancPluginValueRepresentation_SL:
          return ValueRepresentation_SignedLong;

        case OrthancPluginValueRepresentation_SQ:
          return ValueRepresentation_Sequence;

        case OrthancPluginValueRepresentation_SS:
          return ValueRepresentation_SignedShort;

        case OrthancPluginValueRepresentation_ST:
          return ValueRepresentation_ShortText;

        case OrthancPluginValueRepresentation_TM:
          return ValueRepresentation_Time;

        case OrthancPluginValueRepresentation_UI:
          return ValueRepresentation_UniqueIdentifier;

        case OrthancPluginValueRepresentation_UL:
          return ValueRepresentation_UnsignedLong;

        case OrthancPluginValueRepresentation_UN:
          return ValueRepresentation_Unknown;

        case OrthancPluginValueRepresentation_US:
          return ValueRepresentation_UnsignedShort;

        case OrthancPluginValueRepresentation_UT:
          return ValueRepresentation_UnlimitedText;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);

          /*
          Not supported as of DCMTK 3.6.0:
          return ValueRepresentation_OtherDouble
          return ValueRepresentation_OtherLong
          return ValueRepresentation_UniversalResource
          return ValueRepresentation_UnlimitedCharacters
          */
      }
    }


    OrthancPluginValueRepresentation Convert(ValueRepresentation vr)
    {
      switch (vr)
      {
        case ValueRepresentation_ApplicationEntity:
          return OrthancPluginValueRepresentation_AE;

        case ValueRepresentation_AgeString:
          return OrthancPluginValueRepresentation_AS;

        case ValueRepresentation_AttributeTag:
          return OrthancPluginValueRepresentation_AT;

        case ValueRepresentation_CodeString:
          return OrthancPluginValueRepresentation_CS;

        case ValueRepresentation_Date:
          return OrthancPluginValueRepresentation_DA;

        case ValueRepresentation_DecimalString:
          return OrthancPluginValueRepresentation_DS;

        case ValueRepresentation_DateTime:
          return OrthancPluginValueRepresentation_DT;

        case ValueRepresentation_FloatingPointDouble:
          return OrthancPluginValueRepresentation_FD;

        case ValueRepresentation_FloatingPointSingle:
          return OrthancPluginValueRepresentation_FL;

        case ValueRepresentation_IntegerString:
          return OrthancPluginValueRepresentation_IS;

        case ValueRepresentation_LongString:
          return OrthancPluginValueRepresentation_LO;

        case ValueRepresentation_LongText:
          return OrthancPluginValueRepresentation_LT;

        case ValueRepresentation_OtherByte:
          return OrthancPluginValueRepresentation_OB;

        case ValueRepresentation_OtherFloat:
          return OrthancPluginValueRepresentation_OF;

        case ValueRepresentation_OtherWord:
          return OrthancPluginValueRepresentation_OW;

        case ValueRepresentation_PersonName:
          return OrthancPluginValueRepresentation_PN;

        case ValueRepresentation_ShortString:
          return OrthancPluginValueRepresentation_SH;

        case ValueRepresentation_SignedLong:
          return OrthancPluginValueRepresentation_SL;

        case ValueRepresentation_Sequence:
          return OrthancPluginValueRepresentation_SQ;

        case ValueRepresentation_SignedShort:
          return OrthancPluginValueRepresentation_SS;

        case ValueRepresentation_ShortText:
          return OrthancPluginValueRepresentation_ST;

        case ValueRepresentation_Time:
          return OrthancPluginValueRepresentation_TM;

        case ValueRepresentation_UniqueIdentifier:
          return OrthancPluginValueRepresentation_UI;

        case ValueRepresentation_UnsignedLong:
          return OrthancPluginValueRepresentation_UL;

        case ValueRepresentation_UnsignedShort:
          return OrthancPluginValueRepresentation_US;

        case ValueRepresentation_UnlimitedText:
          return OrthancPluginValueRepresentation_UT;

        case ValueRepresentation_Unknown:
          return OrthancPluginValueRepresentation_UN;  // Unknown

          // These VR are not supported as of DCMTK 3.6.0, so they are
          // mapped to "UN" (unknown) VR in the plugins
        case ValueRepresentation_OtherDouble:          
        case ValueRepresentation_OtherLong:
        case ValueRepresentation_UniversalResource:
        case ValueRepresentation_UnlimitedCharacters:
          return OrthancPluginValueRepresentation_UN;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }
}
