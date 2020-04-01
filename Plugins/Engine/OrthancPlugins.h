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

#include "PluginsErrorDictionary.h"

#if !defined(ORTHANC_ENABLE_PLUGINS)
#  error The macro ORTHANC_ENABLE_PLUGINS must be defined
#endif


#if ORTHANC_ENABLE_PLUGINS != 1

#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class OrthancPlugins : public boost::noncopyable
  {
  };
}

#else

#include "../../Core/DicomNetworking/IFindRequestHandlerFactory.h"
#include "../../Core/DicomNetworking/IMoveRequestHandlerFactory.h"
#include "../../Core/DicomNetworking/IWorklistRequestHandlerFactory.h"
#include "../../Core/FileStorage/IStorageArea.h"
#include "../../Core/HttpServer/IHttpHandler.h"
#include "../../Core/HttpServer/IIncomingHttpRequestFilter.h"
#include "../../Core/JobsEngine/IJob.h"
#include "../../OrthancServer/IDicomImageDecoder.h"
#include "../../OrthancServer/IServerListener.h"
#include "../../OrthancServer/ServerJobs/IStorageCommitmentFactory.h"
#include "OrthancPluginDatabase.h"
#include "PluginsManager.h"

#include <list>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class ServerContext;

  class OrthancPlugins : 
    public IHttpHandler, 
    public IPluginServiceProvider, 
    public IServerListener,
    public IWorklistRequestHandlerFactory,
    public IDicomImageDecoder,
    public IIncomingHttpRequestFilter,
    public IFindRequestHandlerFactory,
    public IMoveRequestHandlerFactory,
    public IStorageCommitmentFactory
  {
  private:
    class PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    class WorklistHandler;
    class FindHandler;
    class MoveHandler;
    class HttpClientChunkedRequest;
    class HttpClientChunkedAnswer;
    class HttpServerChunkedReader;
    
    void RegisterRestCallback(const void* parameters,
                              bool lock);

    void RegisterChunkedRestCallback(const void* parameters);

    bool HandleChunkedGetDelete(HttpOutput& output,
                                HttpMethod method,
                                const UriComponents& uri,
                                const Arguments& headers,
                                const GetArguments& getArguments);

    void RegisterOnStoredInstanceCallback(const void* parameters);

    void RegisterOnChangeCallback(const void* parameters);

    void RegisterWorklistCallback(const void* parameters);

    void RegisterFindCallback(const void* parameters);

    void RegisterMoveCallback(const void* parameters);

    void RegisterDecodeImageCallback(const void* parameters);

    void RegisterJobsUnserializer(const void* parameters);

    void RegisterIncomingHttpRequestFilter(const void* parameters);

    void RegisterIncomingHttpRequestFilter2(const void* parameters);

    void RegisterRefreshMetricsCallback(const void* parameters);

    void RegisterStorageCommitmentScpCallback(const void* parameters);

    void AnswerBuffer(const void* parameters);

    void Redirect(const void* parameters);

    void CompressAndAnswerPngImage(const void* parameters);

    void CompressAndAnswerImage(const void* parameters);

    void GetDicomForInstance(const void* parameters);

    void RestApiGet(const void* parameters,
                    bool afterPlugins);

    void RestApiGet2(const void* parameters);

    void RestApiPostPut(bool isPost, 
                        const void* parameters,
                        bool afterPlugins);

    void RestApiDelete(const void* parameters,
                       bool afterPlugins);

    void LookupResource(_OrthancPluginService service,
                        const void* parameters);

    void SendHttpStatusCode(const void* parameters);

    void SendHttpStatus(const void* parameters);

    void SendUnauthorized(const void* parameters);

    void SendMethodNotAllowed(const void* parameters);

    void SetCookie(const void* parameters);

    void SetHttpHeader(const void* parameters);

    void SetHttpErrorDetails(const void* parameters);

    void BufferCompression(const void* parameters);

    void UncompressImage(const void* parameters);

    void CompressImage(const void* parameters);

    void ConvertPixelFormat(const void* parameters);

    void CallHttpClient(const void* parameters);

    void CallHttpClient2(const void* parameters);

    void ChunkedHttpClient(const void* parameters);

    void CallPeerApi(const void* parameters);
  
    void GetFontInfo(const void* parameters);

    void DrawText(const void* parameters);

    void DatabaseAnswer(const void* parameters);

    void ApplyDicomToJson(_OrthancPluginService service,
                          const void* parameters);

    void ApplyCreateDicom(_OrthancPluginService service,
                          const void* parameters);

    void ApplyCreateImage(_OrthancPluginService service,
                          const void* parameters);

    void ApplyLookupDictionary(const void* parameters);

    void ApplySendMultipartItem(const void* parameters);

    void ApplySendMultipartItem2(const void* parameters);

    void ComputeHash(_OrthancPluginService service,
                     const void* parameters);

    void GetTagName(const void* parameters);

    void SignalChangeInternal(OrthancPluginChangeType changeType,
                              OrthancPluginResourceType resourceType,
                              const char* resource);

    bool InvokeSafeService(SharedLibrary& plugin,
                           _OrthancPluginService service,
                           const void* parameters);

    bool InvokeProtectedService(SharedLibrary& plugin,
                                _OrthancPluginService service,
                                const void* parameters);

  public:
    OrthancPlugins();

    virtual ~OrthancPlugins();

    void SetServerContext(ServerContext& context);

    void ResetServerContext();

    virtual bool Handle(HttpOutput& output,
                        RequestOrigin origin,
                        const char* remoteIp,
                        const char* username,
                        HttpMethod method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const GetArguments& getArguments,
                        const void* bodyData,
                        size_t bodySize) ORTHANC_OVERRIDE;

    virtual bool InvokeService(SharedLibrary& plugin,
                               _OrthancPluginService service,
                               const void* parameters) ORTHANC_OVERRIDE;

    virtual void SignalChange(const ServerIndexChange& change) ORTHANC_OVERRIDE;
    
    virtual void SignalStoredInstance(const std::string& instanceId,
                                      DicomInstanceToStore& instance,
                                      const Json::Value& simplifiedTags) ORTHANC_OVERRIDE;

    virtual bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                        const Json::Value& simplified) ORTHANC_OVERRIDE
    {
      return true; // TODO Enable filtering of instances from plugins
    }

    bool HasStorageArea() const;

    IStorageArea* CreateStorageArea();  // To be freed after use

    const SharedLibrary& GetStorageAreaLibrary() const;

    bool HasDatabaseBackend() const;

    IDatabaseWrapper& GetDatabaseBackend();

    const SharedLibrary& GetDatabaseBackendLibrary() const;

    const char* GetProperty(const char* plugin,
                            _OrthancPluginProperty property) const;

    void SetCommandLineArguments(int argc, char* argv[]);

    PluginsManager& GetManager();

    const PluginsManager& GetManager() const;

    PluginsErrorDictionary& GetErrorDictionary();

    void SignalOrthancStarted()
    {
      SignalChangeInternal(OrthancPluginChangeType_OrthancStarted, OrthancPluginResourceType_None, NULL);
    }

    void SignalOrthancStopped()
    {
      SignalChangeInternal(OrthancPluginChangeType_OrthancStopped, OrthancPluginResourceType_None, NULL);
    }

    void SignalUpdatedPeers()
    {
      SignalChangeInternal(OrthancPluginChangeType_UpdatedPeers, OrthancPluginResourceType_None, NULL);
    }

    void SignalUpdatedModalities()
    {
      SignalChangeInternal(OrthancPluginChangeType_UpdatedModalities, OrthancPluginResourceType_None, NULL);
    }

    bool HasWorklistHandler();

    virtual IWorklistRequestHandler* ConstructWorklistRequestHandler() ORTHANC_OVERRIDE;

    bool HasCustomImageDecoder();

    // Contrarily to "Decode()", this method does not fallback to the
    // builtin image decoder, if no installed custom decoder can
    // handle the image (it returns NULL in this case).
    ImageAccessor* DecodeUnsafe(const void* dicom,
                                size_t size,
                                unsigned int frame);

    virtual ImageAccessor* Decode(const void* dicom,
                                  size_t size,
                                  unsigned int frame) ORTHANC_OVERRIDE;

    virtual bool IsAllowed(HttpMethod method,
                           const char* uri,
                           const char* ip,
                           const char* username,
                           const IHttpHandler::Arguments& httpHeaders,
                           const IHttpHandler::GetArguments& getArguments) ORTHANC_OVERRIDE;

    bool HasFindHandler();

    virtual IFindRequestHandler* ConstructFindRequestHandler() ORTHANC_OVERRIDE;

    bool HasMoveHandler();

    virtual IMoveRequestHandler* ConstructMoveRequestHandler() ORTHANC_OVERRIDE;

    IJob* UnserializeJob(const std::string& type,
                         const Json::Value& value);

    void RefreshMetrics();

    // New in Orthanc 1.5.7
    virtual bool CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                            RequestOrigin origin,
                                            const char* remoteIp,
                                            const char* username,
                                            HttpMethod method,
                                            const UriComponents& uri,
                                            const Arguments& headers) ORTHANC_OVERRIDE;

    // New in Orthanc 1.6.0
    IStorageCommitmentFactory::ILookupHandler* CreateStorageCommitment(
      const std::string& jobId,
      const std::string& transactionUid,
      const std::vector<std::string>& sopClassUids,
      const std::vector<std::string>& sopInstanceUids,
      const std::string& remoteAet,
      const std::string& calledAet) ORTHANC_OVERRIDE;
  };
}

#endif
