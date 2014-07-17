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


#include "PluginsHttpHandler.h"

#include "../../Core/ChunkedBuffer.h"
#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "../../Core/HttpServer/HttpOutput.h"
#include "../../Core/ImageFormats/PngWriter.h"

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



  struct PluginsHttpHandler::PImpl
  {
    typedef std::pair<boost::regex*, OrthancPluginRestCallback> Callback;
    typedef std::list<Callback>  Callbacks;

    ServerContext& context_;
    Callbacks callbacks_;
    OrthancRestApi* restApi_;

    PImpl(ServerContext& context) : context_(context), restApi_(NULL)
    {
    }
  };


  PluginsHttpHandler::PluginsHttpHandler(ServerContext& context)
  {
    pimpl_.reset(new PImpl(context));
  }

  
  PluginsHttpHandler::~PluginsHttpHandler()
  {
    for (PImpl::Callbacks::iterator it = pimpl_->callbacks_.begin(); 
         it != pimpl_->callbacks_.end(); ++it)
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


  bool PluginsHttpHandler::Handle(HttpOutput& output,
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
    for (PImpl::Callbacks::const_iterator it = pimpl_->callbacks_.begin(); 
         it != pimpl_->callbacks_.end() && !found; ++it)
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


  void PluginsHttpHandler::RegisterRestCallback(const void* parameters)
  {
    const _OrthancPluginRestCallback& p = 
      *reinterpret_cast<const _OrthancPluginRestCallback*>(parameters);

    LOG(INFO) << "Plugin has registered a REST callback on: " << p.pathRegularExpression;
    pimpl_->callbacks_.push_back(std::make_pair(new boost::regex(p.pathRegularExpression), p.callback));
  }



  void PluginsHttpHandler::AnswerBuffer(const void* parameters)
  {
    const _OrthancPluginAnswerBuffer& p = 
      *reinterpret_cast<const _OrthancPluginAnswerBuffer*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->AnswerBufferWithContentType(p.answer, p.answerSize, p.mimeType);
  }


  void PluginsHttpHandler::Redirect(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->Redirect(p.argument);
  }


  void PluginsHttpHandler::SendHttpStatusCode(const void* parameters)
  {
    const _OrthancPluginSendHttpStatusCode& p = 
      *reinterpret_cast<const _OrthancPluginSendHttpStatusCode*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendHeader(static_cast<HttpStatus>(p.status));
  }


  void PluginsHttpHandler::SendUnauthorized(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendUnauthorized(p.argument);
  }


  void PluginsHttpHandler::SendMethodNotAllowed(const void* parameters)
  {
    const _OrthancPluginOutputPlusArgument& p = 
      *reinterpret_cast<const _OrthancPluginOutputPlusArgument*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SendMethodNotAllowed(p.argument);
  }


  void PluginsHttpHandler::SetCookie(const void* parameters)
  {
    const _OrthancPluginSetCookie& p = 
      *reinterpret_cast<const _OrthancPluginSetCookie*>(parameters);

    HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
    translatedOutput->SetCookie(p.cookie, p.value);
  }


  void PluginsHttpHandler::CompressAndAnswerPngImage(const void* parameters)
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

    translatedOutput->AnswerBufferWithContentType(png, "image/png");
  }


  void PluginsHttpHandler::GetDicomForInstance(const void* parameters)
  {
    const _OrthancPluginGetDicomForInstance& p = 
      *reinterpret_cast<const _OrthancPluginGetDicomForInstance*>(parameters);

    std::string dicom;
    pimpl_->context_.ReadFile(dicom, p.instanceId, FileContentType_Dicom);
    CopyToMemoryBuffer(*p.target, dicom);
  }


  void PluginsHttpHandler::RestApiGet(const void* parameters)
  {
    const _OrthancPluginRestApiGet& p = 
      *reinterpret_cast<const _OrthancPluginRestApiGet*>(parameters);
        
    HttpHandler::Arguments headers;  // No HTTP header
    std::string body;  // No body for a GET request

    UriComponents uri;
    HttpHandler::Arguments getArguments;
    HttpHandler::ParseGetQuery(uri, getArguments, p.uri);

    StringHttpOutput stream;
    HttpOutput http(stream);

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


  void PluginsHttpHandler::RestApiPostPut(bool isPost, const void* parameters)
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
    HttpOutput http(stream);

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


  void PluginsHttpHandler::RestApiDelete(const void* parameters)
  {
    // The "parameters" point to the URI
    UriComponents uri;
    Toolbox::SplitUriComponents(uri, reinterpret_cast<const char*>(parameters));

    HttpHandler::Arguments headers;  // No HTTP header
    HttpHandler::Arguments getArguments;  // No GET argument for POST/PUT
    std::string body;  // No body for DELETE

    StringHttpOutput stream;
    HttpOutput http(stream);

    LOG(INFO) << "Plugin making REST DELETE call on URI " 
              << reinterpret_cast<const char*>(parameters);

    if (pimpl_->restApi_ == NULL ||
        !pimpl_->restApi_->Handle(http, HttpMethod_Delete, uri, headers, getArguments, body))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
  }


  void PluginsHttpHandler::LookupResource(ResourceType level,
                                          const void* parameters)
  {
    const _OrthancPluginLookupResource& p = 
      *reinterpret_cast<const _OrthancPluginLookupResource*>(parameters);

    DicomTag tag(0, 0);
    switch (level)
    {
      case ResourceType_Patient:
        tag = DICOM_TAG_PATIENT_ID;
        break;

      case ResourceType_Study:
        tag = DICOM_TAG_STUDY_INSTANCE_UID;
        break;

      case ResourceType_Series:
        tag = DICOM_TAG_SERIES_INSTANCE_UID;
        break;

      case ResourceType_Instance:
        tag = DICOM_TAG_SOP_INSTANCE_UID;
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


  char* PluginsHttpHandler::CopyString(const std::string& str) const
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


  bool PluginsHttpHandler::InvokeService(_OrthancPluginService service,
                                         const void* parameters)
  {
    switch (service)
    {
      case _OrthancPluginService_RegisterRestCallback:
        RegisterRestCallback(parameters);
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

      case _OrthancPluginService_LookupPatient:
        LookupResource(ResourceType_Patient, parameters);
        return true;

      case _OrthancPluginService_LookupStudy:
        LookupResource(ResourceType_Study, parameters);
        return true;

      case _OrthancPluginService_LookupSeries:
        LookupResource(ResourceType_Series, parameters);
        return true;

      case _OrthancPluginService_LookupInstance:
        LookupResource(ResourceType_Instance, parameters);
        return true;

      default:
        return false;
    }
  }


  void PluginsHttpHandler::SetOrthancRestApi(OrthancRestApi& restApi)
  {
    pimpl_->restApi_ = &restApi;
  }

}
