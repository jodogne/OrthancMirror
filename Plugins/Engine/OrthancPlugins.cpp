/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "OrthancPlugins.h"

#include "../../Core/ChunkedBuffer.h"
#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "../../Core/HttpServer/HttpOutput.h"
#include "../../Core/ImageFormats/PngWriter.h"
#include "../../OrthancServer/ServerToolbox.h"

#include <boost/regex.hpp> 
#include <glog/logging.h>

namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    class StringHttpOutput : public IHttpOutputStream
    {
    private:
      ChunkedBuffer buffer_;

    public:
      void GetOutput(std::string& output)
      {
        buffer_.Flatten(output);
      }

      virtual void OnHttpStatusReceived(HttpStatus status)
      {
        if (status != HttpStatus_200_Ok)
        {
          throw OrthancException(ErrorCode_BadRequest);
        }
      }

      virtual void Send(bool isHeader, const void* buffer, size_t length)
      {
        if (!isHeader)
        {
          buffer_.AddChunk(reinterpret_cast<const char*>(buffer), length);
        }
      }
    };
  }



  struct OrthancPlugins::PImpl
  {
    typedef std::pair<boost::regex*, OrthancPluginRestCallback> RestCallback;
    typedef std::list<RestCallback>  RestCallbacks;
    typedef std::list<OrthancPluginOnStoredInstanceCallback>  OnStoredCallbacks;

    ServerContext& context_;
    RestCallbacks restCallbacks_;
    OrthancRestApi* restApi_;
    OnStoredCallbacks  onStoredCallbacks_;
    bool hasStorageArea_;
    _OrthancPluginRegisterStorageArea storageArea_;

    PImpl(ServerContext& context) : 
      context_(context), 
      restApi_(NULL),
      hasStorageArea_(false)
    {
      memset(&storageArea_, 0, sizeof(storageArea_));
    }
  };


  static char* CopyString(const std::string& str)
  {
    char *result = reinterpret_cast<char*>(malloc(str.size() + 1));
    if (result == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    if (str.size() == 0)
    {
      result[0] = '\0';
    }
    else
    {
      memcpy(result, &str[0], str.size() + 1);
    }

    return result;
  }


  OrthancPlugins::OrthancPlugins(ServerContext& context)
  {
    pimpl_.reset(new PImpl(context));
  }

  
  OrthancPlugins::~OrthancPlugins()
  {
    for (PImpl::RestCallbacks::iterator it = pimpl_->restCallbacks_.begin(); 
         it != pimpl_->restCallbacks_.end(); ++it)
    {
      // Delete the regular expression associated with this callback
      delete it->first;
    }
  }


  static void ArgumentsToPlugin(std::vector<const char*>& keys,
                                std::vector<const char*>& values,
                                const HttpHandler::Arguments& arguments)
  {
    keys.resize(arguments.size());
    values.resize(arguments.size());

    size_t pos = 0;
    for (HttpHandler::Arguments::const_iterator 
           it = arguments.begin(); it != arguments.end(); ++it)
    {
      keys[pos] = it->first.c_str();
      values[pos] = it->second.c_str();
      pos++;
    }
  }


  bool OrthancPlugins::Handle(HttpOutput& output,
                                  HttpMethod method,
                                  const UriComponents& uri,
                                  const Arguments& headers,
                                  const Arguments& getArguments,
                                  const std::string& postData)
  {
    std::string flatUri = Toolbox::FlattenUri(uri);
    OrthancPluginRestCallback callback = NULL;

    std::vector<std::string> groups;
    std::vector<const char*> cgroups;

    // Loop over the callbacks registered by the plugins
    bool found = false;
    for (PImpl::RestCallbacks::const_iterator it = pimpl_->restCallbacks_.begin(); 
         it != pimpl_->restCallbacks_.end() && !found; ++it)
    {
      // Check whether the regular expression associated to this
      // callback matches the URI
      boost::cmatch what;
      if (boost::regex_match(flatUri.c_str(), what, *(it->first)))
      {
        callback = it->second;

        // Extract the value of the free parameters of the regular expression
        if (what.size() > 1)
        {
          groups.resize(what.size() - 1);
          cgroups.resize(what.size() - 1);
          for (size_t i = 1; i < what.size(); i++)
          {
            groups[i - 1] = what[i];
            cgroups[i - 1] = groups[i - 1].c_str();
          }
        }

        found = true;
      }
    }

    if (!found)
    {
      return false;
    }

    LOG(INFO) << "Delegating HTTP request to plugin for URI: " << flatUri;

    std::vector<const char*> getKeys, getValues, headersKeys, headersValues;

    OrthancPluginHttpRequest request;
    memset(&request, 0, sizeof(OrthancPluginHttpRequest));

    ArgumentsToPlugin(headersKeys, headersValues, headers);

    switch (method)
    {
      case HttpMethod_Get:
        request.method = OrthancPluginHttpMethod_Get;
        ArgumentsToPlugin(getKeys, getValues, getArguments);
        break;

      case HttpMethod_Post:
        request.method = OrthancPluginHttpMethod_Post;
        break;

      case HttpMethod_Delete:
        request.method = OrthancPluginHttpMethod_Delete;
        break;

      case HttpMethod_Put:
        request.method = OrthancPluginHttpMethod_Put;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }


    request.groups = (cgroups.size() ? &cgroups[0] : NULL);
    request.groupsCount = cgroups.size();
    request.getCount = getArguments.size();
    request.body = (postData.size() ? &postData[0] : NULL);
    request.bodySize = postData.size();
    request.headersCount = headers.size();
    
    if (getArguments.size() > 0)
    {
      request.getKeys = &getKeys[0];
      request.getValues = &getValues[0];
    }
    
    if (headers.size() > 0)
    {
      request.headersKeys = &headersKeys[0];
      request.headersValues = &headersValues[0];
    }

    assert(callback != NULL);
    int32_t error = callback(reinterpret_cast<OrthancPluginRestOutput*>(&output), 
                             flatUri.c_str(), 
                             &request);

    if (error < 0)
    {
      LOG(ERROR) << "Plugin callback failed with error code " << error;
      return false;
    }
    else
    {
      if (error > 0)
      {
        LOG(WARNING) << "Plugin callback finished with warning code " << error;
      }

      return true;
    }
  }


  void OrthancPlugins::SignalStoredInstance(DicomInstanceToStore& instance,
                                                const std::string& instanceId)                                                  
  {
    for (PImpl::OnStoredCallbacks::const_iterator
           callback = pimpl_->onStoredCallbacks_.begin(); 
         callback != pimpl_->onStoredCallbacks_.end(); ++callback)
    {
      (*callback) (reinterpret_cast<OrthancPluginDicomInstance*>(&instance),
                   instanceId.c_str());
    }
  }



  static void CopyToMemoryBuffer(OrthancPluginMemoryBuffer& target,
                                 const void* data,
                                 size_t size)
  {
    target.size = size;

    if (size == 0)
    {
      target.data = NULL;
    }
    else
    {
      target.data = malloc(size);
      if (target.data != NULL)
      {
        memcpy(target.data, data, size);
      }
      else
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
    }
  }


  static void CopyToMemoryBuffer(OrthancPluginMemoryBuffer& target,
                                 const std::string& str)
  {
    if (str.size() == 0)
    {
      target.size = 0;
      target.data = NULL;
    }
    else
    {
      CopyToMemoryBuffer(target, str.c_str(), str.size());
    }
  }


  void OrthancPlugins::RegisterRestCallback(const void* parameters)
  {
    const _OrthancPluginRestCallback& p = 
      *reinterpret_cast<const _OrthancPluginRestCallback*>(parameters);

    LOG(INFO) << "Plugin has registered a REST callback on: " << p.pathRegularExpression;
    pimpl_->restCallbacks_.push_back(std::make_pair(new boost::regex(p.pathRegularExpression), p.callback));
  }



  void OrthancPlugins::RegisterOnStoredInstanceCallback(const void* parameters)
  {
    const _OrthancPluginOnStoredInstanceCallback& p = 
      *reinterpret_cast<const _OrthancPluginOnStoredInstanceCallback*>(parameters);

    LOG(INFO) << "Plugin has registered an OnStoredInstance callback";
    pimpl_->onStoredCallbacks_.push_back(p.callback);
  }



  void OrthancPlugins::AnswerBuffer(const void* parameters)
  {
    const _OrthancPluginAnswerBuffer& p = 
      *reinterpret_cast<const _OrthancPluginAnswerBuffer*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SetContentType(p.mimeType);
    translatedOutput->SendBody(p.answer, p.answerSize);
  }


  void OrthancPlugins::Redirect(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->Redirect(p.argument);
  }


  void OrthancPlugins::SendHttpStatusCode(const void* parameters)
  {
    const _OrthancPluginSendHttpStatusCode& p = 
      *reinterpret_cast<const _OrthancPluginSendHttpStatusCode*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendStatus(static_cast<HttpStatus>(p.status));
  }


  void OrthancPlugins::SendUnauthorized(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendUnauthorized(p.argument);
  }


  void OrthancPlugins::SendMethodNotAllowed(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendMethodNotAllowed(p.argument);
  }


  void OrthancPlugins::SetCookie(const void* parameters)
  {
    const _OrthancPluginSetHttpHeader& p = 
      *reinterpret_cast<const _OrthancPluginSetHttpHeader*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SetCookie(p.key, p.value);
  }


  void OrthancPlugins::SetHttpHeader(const void* parameters)
  {
    const _OrthancPluginSetHttpHeader& p = 
      *reinterpret_cast<const _OrthancPluginSetHttpHeader*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->AddHeader(p.key, p.value);
  }


  void OrthancPlugins::CompressAndAnswerPngImage(const void* parameters)
  {
    const _OrthancPluginCompressAndAnswerPngImage& p = 
      *reinterpret_cast<const _OrthancPluginCompressAndAnswerPngImage*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);

    PixelFormat format;
    switch (p.format)
    {
      case OrthancPluginPixelFormat_Grayscale8:  
        format = PixelFormat_Grayscale8;
        break;

      case OrthancPluginPixelFormat_Grayscale16:  
        format = PixelFormat_Grayscale16;
        break;

      case OrthancPluginPixelFormat_SignedGrayscale16:  
        format = PixelFormat_SignedGrayscale16;
        break;

      case OrthancPluginPixelFormat_RGB24:  
        format = PixelFormat_RGB24;
        break;

      case OrthancPluginPixelFormat_RGBA32:  
        format = PixelFormat_RGBA32;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    ImageAccessor accessor;
    accessor.AssignReadOnly(format, p.width, p.height, p.pitch, p.buffer);

    PngWriter writer;
    std::string png;
    writer.WriteToMemory(png, accessor);

    translatedOutput->SetContentType("image/png");
    translatedOutput->SendBody(png);
  }


  void OrthancPlugins::GetDicomForInstance(const void* parameters)
  {
    const _OrthancPluginGetDicomForInstance& p = 
      *reinterpret_cast<const _OrthancPluginGetDicomForInstance*>(parameters);

    std::string dicom;
    pimpl_->context_.ReadFile(dicom, p.instanceId, FileContentType_Dicom);
    CopyToMemoryBuffer(*p.target, dicom);
  }


  void OrthancPlugins::RestApiGet(const void* parameters)
  {
    const _OrthancPluginRestApiGet& p = 
      *reinterpret_cast<const _OrthancPluginRestApiGet*>(parameters);
        
    HttpHandler::Arguments headers;  // No HTTP header
    std::string body;  // No body for a GET request

    UriComponents uri;
    HttpHandler::Arguments getArguments;
    HttpHandler::ParseGetQuery(uri, getArguments, p.uri);

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    LOG(INFO) << "Plugin making REST GET call on URI " << p.uri;

    if (pimpl_->restApi_ != NULL &&
        pimpl_->restApi_->Handle(http, HttpMethod_Get, uri, headers, getArguments, body))
    {
      std::string result;
      stream.GetOutput(result);
      CopyToMemoryBuffer(*p.target, result);
    }
    else
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
  }


  void OrthancPlugins::RestApiPostPut(bool isPost, const void* parameters)
  {
    const _OrthancPluginRestApiPostPut& p = 
      *reinterpret_cast<const _OrthancPluginRestApiPostPut*>(parameters);

    HttpHandler::Arguments headers;  // No HTTP header
    HttpHandler::Arguments getArguments;  // No GET argument for POST/PUT

    UriComponents uri;
    Toolbox::SplitUriComponents(uri, p.uri);

    // TODO Avoid unecessary memcpy
    std::string body(p.body, p.bodySize);

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    HttpMethod method = (isPost ? HttpMethod_Post : HttpMethod_Put);
    LOG(INFO) << "Plugin making REST " << EnumerationToString(method) << " call on URI " << p.uri;

    if (pimpl_->restApi_ != NULL &&
        pimpl_->restApi_->Handle(http, method, uri, headers, getArguments, body))
    {
      std::string result;
      stream.GetOutput(result);
      CopyToMemoryBuffer(*p.target, result);
    }
    else
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
  }


  void OrthancPlugins::RestApiDelete(const void* parameters)
  {
    // The "parameters" point to the URI
    UriComponents uri;
    Toolbox::SplitUriComponents(uri, reinterpret_cast<const char*>(parameters));

    HttpHandler::Arguments headers;  // No HTTP header
    HttpHandler::Arguments getArguments;  // No GET argument for POST/PUT
    std::string body;  // No body for DELETE

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    LOG(INFO) << "Plugin making REST DELETE call on URI " 
              << reinterpret_cast<const char*>(parameters);

    if (pimpl_->restApi_ == NULL ||
        !pimpl_->restApi_->Handle(http, HttpMethod_Delete, uri, headers, getArguments, body))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
  }


  void OrthancPlugins::LookupResource(_OrthancPluginService service,
                                          const void* parameters)
  {
    const _OrthancPluginLookupResource& p = 
      *reinterpret_cast<const _OrthancPluginLookupResource*>(parameters);

    /**
     * The enumeration below only uses the tags that are indexed in
     * the Orthanc database. It reflects the
     * "CandidateResources::ApplyFilter()" method of the
     * "OrthancFindRequestHandler" class.
     **/

    DicomTag tag(0, 0);
    ResourceType level;
    switch (service)
    {
      case _OrthancPluginService_LookupPatient:
        tag = DICOM_TAG_PATIENT_ID;
        level = ResourceType_Patient;
        break;

      case _OrthancPluginService_LookupStudy:
        tag = DICOM_TAG_STUDY_INSTANCE_UID;
        level = ResourceType_Study;
        break;

      case _OrthancPluginService_LookupStudyWithAccessionNumber:
        tag = DICOM_TAG_ACCESSION_NUMBER;
        level = ResourceType_Study;
        break;

      case _OrthancPluginService_LookupSeries:
        tag = DICOM_TAG_SERIES_INSTANCE_UID;
        level = ResourceType_Series;
        break;

      case _OrthancPluginService_LookupInstance:
        tag = DICOM_TAG_SOP_INSTANCE_UID;
        level = ResourceType_Instance;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::list<std::string> result;
    pimpl_->context_.GetIndex().LookupTagValue(result, tag, p.identifier, level);

    if (result.size() == 1)
    {
      *p.result = CopyString(result.front());
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  static void AccessInstanceMetadataInternal(bool checkExistence,
                                             const _OrthancPluginAccessDicomInstance& params,
                                             const DicomInstanceToStore& instance)
  {
    MetadataType metadata;

    try
    {
      metadata = StringToMetadata(params.key);
    }
    catch (OrthancException&)
    {
      // Unknown metadata
      if (checkExistence)
      {
        *params.resultInt64 = -1;
      }
      else
      {
        *params.resultString = NULL;
      }

      return;
    }

    ServerIndex::MetadataMap::const_iterator it = 
      instance.GetMetadata().find(std::make_pair(ResourceType_Instance, metadata));

    if (checkExistence)
    {
      if (it != instance.GetMetadata().end())
      {
        *params.resultInt64 = 1;
      }
      else
      {
        *params.resultInt64 = 0;
      }
    }
    else
    {
      if (it != instance.GetMetadata().end())
      {      
        *params.resultString = it->second.c_str();
      }
      else
      {
        // Error: Missing metadata
        *params.resultString = NULL;
      }
    }
  }


  static void AccessDicomInstance(_OrthancPluginService service,
                                  const void* parameters)
  {
    const _OrthancPluginAccessDicomInstance& p = 
      *reinterpret_cast<const _OrthancPluginAccessDicomInstance*>(parameters);

    DicomInstanceToStore& instance =
      *reinterpret_cast<DicomInstanceToStore*>(p.instance);

    switch (service)
    {
      case _OrthancPluginService_GetInstanceRemoteAet:
        *p.resultString = instance.GetRemoteAet().c_str();
        return;

      case _OrthancPluginService_GetInstanceSize:
        *p.resultInt64 = instance.GetBufferSize();
        return;

      case _OrthancPluginService_GetInstanceData:
        *p.resultString = instance.GetBufferData();
        return;

      case _OrthancPluginService_HasInstanceMetadata:
        AccessInstanceMetadataInternal(true, p, instance);
        return;

      case _OrthancPluginService_GetInstanceMetadata:
        AccessInstanceMetadataInternal(false, p, instance);
        return;

      case _OrthancPluginService_GetInstanceJson:
      case _OrthancPluginService_GetInstanceSimplifiedJson:
      {
        Json::StyledWriter writer;
        std::string s;

        if (service == _OrthancPluginService_GetInstanceJson)
        {
          s = writer.write(instance.GetJson());
        }
        else
        {
          Json::Value simplified;
          SimplifyTags(simplified, instance.GetJson());
          s = writer.write(simplified);
        }

        *p.resultStringToFree = CopyString(s);
        return;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  bool OrthancPlugins::InvokeService(_OrthancPluginService service,
                                         const void* parameters)
  {
    switch (service)
    {
      case _OrthancPluginService_RegisterRestCallback:
        RegisterRestCallback(parameters);
        return true;

      case _OrthancPluginService_RegisterOnStoredInstanceCallback:
        RegisterOnStoredInstanceCallback(parameters);
        return true;

      case _OrthancPluginService_AnswerBuffer:
        AnswerBuffer(parameters);
        return true;

      case _OrthancPluginService_CompressAndAnswerPngImage:
        CompressAndAnswerPngImage(parameters);
        return true;

      case _OrthancPluginService_GetDicomForInstance:
        GetDicomForInstance(parameters);
        return true;

      case _OrthancPluginService_RestApiGet:
        RestApiGet(parameters);
        return true;

      case _OrthancPluginService_RestApiPost:
        RestApiPostPut(true, parameters);
        return true;

      case _OrthancPluginService_RestApiDelete:
        RestApiDelete(parameters);
        return true;

      case _OrthancPluginService_RestApiPut:
        RestApiPostPut(false, parameters);
        return true;

      case _OrthancPluginService_Redirect:
        Redirect(parameters);
        return true;

      case _OrthancPluginService_SendUnauthorized:
        SendUnauthorized(parameters);
        return true;

      case _OrthancPluginService_SendMethodNotAllowed:
        SendMethodNotAllowed(parameters);
        return true;

      case _OrthancPluginService_SendHttpStatusCode:
        SendHttpStatusCode(parameters);
        return true;

      case _OrthancPluginService_SetCookie:
        SetCookie(parameters);
        return true;

      case _OrthancPluginService_SetHttpHeader:
        SetHttpHeader(parameters);
        return true;

      case _OrthancPluginService_LookupPatient:
      case _OrthancPluginService_LookupStudy:
      case _OrthancPluginService_LookupStudyWithAccessionNumber:
      case _OrthancPluginService_LookupSeries:
      case _OrthancPluginService_LookupInstance:
        LookupResource(service, parameters);
        return true;

      case _OrthancPluginService_GetInstanceRemoteAet:
      case _OrthancPluginService_GetInstanceSize:
      case _OrthancPluginService_GetInstanceData:
      case _OrthancPluginService_GetInstanceJson:
      case _OrthancPluginService_GetInstanceSimplifiedJson:
      case _OrthancPluginService_HasInstanceMetadata:
      case _OrthancPluginService_GetInstanceMetadata:
        AccessDicomInstance(service, parameters);
        return true;

      case _OrthancPluginService_RegisterStorageArea:
      {
        const _OrthancPluginRegisterStorageArea& p = 
          *reinterpret_cast<const _OrthancPluginRegisterStorageArea*>(parameters);
        
        pimpl_->storageArea_ = p;
        pimpl_->hasStorageArea_ = true;
        return true;
      }

      default:
        return false;
    }
  }


  void OrthancPlugins::SetOrthancRestApi(OrthancRestApi& restApi)
  {
    pimpl_->restApi_ = &restApi;
  }

  
  bool OrthancPlugins::HasStorageArea() const
  {
    return pimpl_->hasStorageArea_;
  }


  namespace
  {
    class PluginStorageArea : public IStorageArea
    {
    private:
      _OrthancPluginRegisterStorageArea params_;

      void Free(void* buffer) const
      {
        if (buffer != NULL)
        {
          params_.free_(buffer);
        }
      }

      OrthancPluginContentType Convert(FileContentType type) const
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

    public:
      PluginStorageArea(const _OrthancPluginRegisterStorageArea& params) : params_(params)
      {
      }

      virtual void Create(const std::string& uuid,
                          const void* content, 
                          size_t size,
                          FileContentType type)
      {
        if (params_.create_(uuid.c_str(), content, size, Convert(type)) != 0)
        {
          throw OrthancException(ErrorCode_Plugin);
        }
      }

      virtual void Read(std::string& content,
                        const std::string& uuid,
                        FileContentType type) const
      {
        void* buffer = NULL;
        int64_t size = 0;

        if (params_.read_(&buffer, &size, uuid.c_str(), Convert(type)) != 0)
        {
          throw OrthancException(ErrorCode_Plugin);
        }        

        try
        {
          content.resize(size);
        }
        catch (OrthancException&)
        {
          Free(buffer);
          throw;
        }

        if (size > 0)
        {
          memcpy(&content[0], buffer, size);
        }

        Free(buffer);
      }

      virtual void Remove(const std::string& uuid,
                          FileContentType type) 
      {
        if (params_.remove_(uuid.c_str(), Convert(type)) != 0)
        {
          throw OrthancException(ErrorCode_Plugin);
        }        
      }
    };
  }


  IStorageArea* OrthancPlugins::GetStorageArea()
  {
    if (!HasStorageArea())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    return new PluginStorageArea(pimpl_->storageArea_);;
  }
}
