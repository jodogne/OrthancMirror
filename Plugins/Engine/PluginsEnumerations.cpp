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
    ErrorCode Convert(OrthancPluginErrorCode error)
    {
      switch (error)
      {
        case OrthancPluginErrorCode_InternalError:
          return ErrorCode_InternalError;

        case OrthancPluginErrorCode_Success:
          return ErrorCode_Success;

        case OrthancPluginErrorCode_Plugin:
          return ErrorCode_Plugin;

        case OrthancPluginErrorCode_NotImplemented:
          return ErrorCode_NotImplemented;

        case OrthancPluginErrorCode_ParameterOutOfRange:
          return ErrorCode_ParameterOutOfRange;

        case OrthancPluginErrorCode_NotEnoughMemory:
          return ErrorCode_NotEnoughMemory;

        case OrthancPluginErrorCode_BadParameterType:
          return ErrorCode_BadParameterType;

        case OrthancPluginErrorCode_BadSequenceOfCalls:
          return ErrorCode_BadSequenceOfCalls;

        case OrthancPluginErrorCode_InexistentItem:
          return ErrorCode_InexistentItem;

        case OrthancPluginErrorCode_BadRequest:
          return ErrorCode_BadRequest;

        case OrthancPluginErrorCode_NetworkProtocol:
          return ErrorCode_NetworkProtocol;

        case OrthancPluginErrorCode_SystemCommand:
          return ErrorCode_SystemCommand;

        case OrthancPluginErrorCode_Database:
          return ErrorCode_Database;

        case OrthancPluginErrorCode_UriSyntax:
          return ErrorCode_UriSyntax;

        case OrthancPluginErrorCode_InexistentFile:
          return ErrorCode_InexistentFile;

        case OrthancPluginErrorCode_CannotWriteFile:
          return ErrorCode_CannotWriteFile;

        case OrthancPluginErrorCode_BadFileFormat:
          return ErrorCode_BadFileFormat;

        case OrthancPluginErrorCode_Timeout:
          return ErrorCode_Timeout;

        case OrthancPluginErrorCode_UnknownResource:
          return ErrorCode_UnknownResource;

        case OrthancPluginErrorCode_IncompatibleDatabaseVersion:
          return ErrorCode_IncompatibleDatabaseVersion;

        case OrthancPluginErrorCode_FullStorage:
          return ErrorCode_FullStorage;

        case OrthancPluginErrorCode_CorruptedFile:
          return ErrorCode_CorruptedFile;

        case OrthancPluginErrorCode_InexistentTag:
          return ErrorCode_InexistentTag;

        case OrthancPluginErrorCode_ReadOnly:
          return ErrorCode_ReadOnly;

        case OrthancPluginErrorCode_IncompatibleImageFormat:
          return ErrorCode_IncompatibleImageFormat;

        case OrthancPluginErrorCode_IncompatibleImageSize:
          return ErrorCode_IncompatibleImageSize;

        case OrthancPluginErrorCode_SharedLibrary:
          return ErrorCode_SharedLibrary;

        case OrthancPluginErrorCode_UnknownPluginService:
          return ErrorCode_UnknownPluginService;

        case OrthancPluginErrorCode_UnknownDicomTag:
          return ErrorCode_UnknownDicomTag;

        case OrthancPluginErrorCode_BadJson:
          return ErrorCode_BadJson;

        case OrthancPluginErrorCode_Unauthorized:
          return ErrorCode_Unauthorized;

        case OrthancPluginErrorCode_BadFont:
          return ErrorCode_BadFont;

        case OrthancPluginErrorCode_DatabasePlugin:
          return ErrorCode_DatabasePlugin;

        case OrthancPluginErrorCode_StorageAreaPlugin:
          return ErrorCode_StorageAreaPlugin;

        case OrthancPluginErrorCode_SQLiteNotOpened:
          return ErrorCode_SQLiteNotOpened;

        case OrthancPluginErrorCode_SQLiteAlreadyOpened:
          return ErrorCode_SQLiteAlreadyOpened;

        case OrthancPluginErrorCode_SQLiteCannotOpen:
          return ErrorCode_SQLiteCannotOpen;

        case OrthancPluginErrorCode_SQLiteStatementAlreadyUsed:
          return ErrorCode_SQLiteStatementAlreadyUsed;

        case OrthancPluginErrorCode_SQLiteExecute:
          return ErrorCode_SQLiteExecute;

        case OrthancPluginErrorCode_SQLiteRollbackWithoutTransaction:
          return ErrorCode_SQLiteRollbackWithoutTransaction;

        case OrthancPluginErrorCode_SQLiteCommitWithoutTransaction:
          return ErrorCode_SQLiteCommitWithoutTransaction;

        case OrthancPluginErrorCode_SQLiteRegisterFunction:
          return ErrorCode_SQLiteRegisterFunction;

        case OrthancPluginErrorCode_SQLiteFlush:
          return ErrorCode_SQLiteFlush;

        case OrthancPluginErrorCode_SQLiteCannotRun:
          return ErrorCode_SQLiteCannotRun;

        case OrthancPluginErrorCode_SQLiteCannotStep:
          return ErrorCode_SQLiteCannotStep;

        case OrthancPluginErrorCode_SQLiteBindOutOfRange:
          return ErrorCode_SQLiteBindOutOfRange;

        case OrthancPluginErrorCode_SQLitePrepareStatement:
          return ErrorCode_SQLitePrepareStatement;

        case OrthancPluginErrorCode_SQLiteTransactionAlreadyStarted:
          return ErrorCode_SQLiteTransactionAlreadyStarted;

        case OrthancPluginErrorCode_SQLiteTransactionCommit:
          return ErrorCode_SQLiteTransactionCommit;

        case OrthancPluginErrorCode_SQLiteTransactionBegin:
          return ErrorCode_SQLiteTransactionBegin;

        case OrthancPluginErrorCode_DirectoryOverFile:
          return ErrorCode_DirectoryOverFile;

        case OrthancPluginErrorCode_FileStorageCannotWrite:
          return ErrorCode_FileStorageCannotWrite;

        case OrthancPluginErrorCode_DirectoryExpected:
          return ErrorCode_DirectoryExpected;

        case OrthancPluginErrorCode_HttpPortInUse:
          return ErrorCode_HttpPortInUse;

        case OrthancPluginErrorCode_DicomPortInUse:
          return ErrorCode_DicomPortInUse;

        case OrthancPluginErrorCode_BadHttpStatusInRest:
          return ErrorCode_BadHttpStatusInRest;

        case OrthancPluginErrorCode_RegularFileExpected:
          return ErrorCode_RegularFileExpected;

        case OrthancPluginErrorCode_PathToExecutable:
          return ErrorCode_PathToExecutable;

        case OrthancPluginErrorCode_MakeDirectory:
          return ErrorCode_MakeDirectory;

        case OrthancPluginErrorCode_BadApplicationEntityTitle:
          return ErrorCode_BadApplicationEntityTitle;

        case OrthancPluginErrorCode_NoCFindHandler:
          return ErrorCode_NoCFindHandler;

        case OrthancPluginErrorCode_NoCMoveHandler:
          return ErrorCode_NoCMoveHandler;

        case OrthancPluginErrorCode_NoCStoreHandler:
          return ErrorCode_NoCStoreHandler;

        case OrthancPluginErrorCode_NoApplicationEntityFilter:
          return ErrorCode_NoApplicationEntityFilter;

        case OrthancPluginErrorCode_NoSopClassOrInstance:
          return ErrorCode_NoSopClassOrInstance;

        case OrthancPluginErrorCode_NoPresentationContext:
          return ErrorCode_NoPresentationContext;

        case OrthancPluginErrorCode_DicomFindUnavailable:
          return ErrorCode_DicomFindUnavailable;

        case OrthancPluginErrorCode_DicomMoveUnavailable:
          return ErrorCode_DicomMoveUnavailable;

        case OrthancPluginErrorCode_CannotStoreInstance:
          return ErrorCode_CannotStoreInstance;

        case OrthancPluginErrorCode_CreateDicomNotString:
          return ErrorCode_CreateDicomNotString;

        case OrthancPluginErrorCode_CreateDicomOverrideTag:
          return ErrorCode_CreateDicomOverrideTag;

        case OrthancPluginErrorCode_CreateDicomUseContent:
          return ErrorCode_CreateDicomUseContent;

        case OrthancPluginErrorCode_CreateDicomNoPayload:
          return ErrorCode_CreateDicomNoPayload;

        case OrthancPluginErrorCode_CreateDicomUseDataUriScheme:
          return ErrorCode_CreateDicomUseDataUriScheme;

        case OrthancPluginErrorCode_CreateDicomBadParent:
          return ErrorCode_CreateDicomBadParent;

        case OrthancPluginErrorCode_CreateDicomParentIsInstance:
          return ErrorCode_CreateDicomParentIsInstance;

        case OrthancPluginErrorCode_CreateDicomParentEncoding:
          return ErrorCode_CreateDicomParentEncoding;

        case OrthancPluginErrorCode_UnknownModality:
          return ErrorCode_UnknownModality;

        case OrthancPluginErrorCode_BadJobOrdering:
          return ErrorCode_BadJobOrdering;

        case OrthancPluginErrorCode_JsonToLuaTable:
          return ErrorCode_JsonToLuaTable;

        case OrthancPluginErrorCode_CannotCreateLua:
          return ErrorCode_CannotCreateLua;

        case OrthancPluginErrorCode_CannotExecuteLua:
          return ErrorCode_CannotExecuteLua;

        case OrthancPluginErrorCode_LuaAlreadyExecuted:
          return ErrorCode_LuaAlreadyExecuted;

        case OrthancPluginErrorCode_LuaBadOutput:
          return ErrorCode_LuaBadOutput;

        case OrthancPluginErrorCode_NotLuaPredicate:
          return ErrorCode_NotLuaPredicate;

        case OrthancPluginErrorCode_LuaReturnsNoString:
          return ErrorCode_LuaReturnsNoString;

        case OrthancPluginErrorCode_StorageAreaAlreadyRegistered:
          return ErrorCode_StorageAreaAlreadyRegistered;

        case OrthancPluginErrorCode_DatabaseBackendAlreadyRegistered:
          return ErrorCode_DatabaseBackendAlreadyRegistered;

        case OrthancPluginErrorCode_DatabaseNotInitialized:
          return ErrorCode_DatabaseNotInitialized;

        default:
          return ErrorCode_Plugin;
      }
    }


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
  }
}
