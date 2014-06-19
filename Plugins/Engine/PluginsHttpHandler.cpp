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

#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "../../Core/HttpServer/HttpOutput.h"
#include "../../Core/ImageFormats/PngWriter.h"

#include <boost/regex.hpp> 
#include <glog/logging.h>

namespace Orthanc
{
  struct PluginsHttpHandler::PImpl
  {
    typedef std::pair<boost::regex*, OrthancPluginRestCallback> Callback;
    typedef std::list<Callback>  Callbacks;

    ServerContext& context_;
    Callbacks callbacks_;

    PImpl(ServerContext& context) : context_(context)
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
         it != pimpl_->callbacks_.end(); it++)
    {
      delete it->first;
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

    bool found = false;
    for (PImpl::Callbacks::const_iterator it = pimpl_->callbacks_.begin(); 
         it != pimpl_->callbacks_.end() && !found; it++)
    {
      boost::cmatch what;
      if (boost::regex_match(flatUri.c_str(), what, *(it->first)))
      {
        callback = it->second;

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

    std::vector<const char*> getKeys(getArguments.size());
    std::vector<const char*> getValues(getArguments.size());

    OrthancPluginHttpRequest request;
    memset(&request, 0, sizeof(OrthancPluginHttpRequest));

    switch (method)
    {
      case HttpMethod_Get:
      {
        request.method = OrthancPluginHttpMethod_Get;

        size_t i = 0;
        for (Arguments::const_iterator it = getArguments.begin(); 
             it != getArguments.end(); it++, i++)
        {
          getKeys[i] = it->first.c_str();
          getValues[i] = it->second.c_str();
        }

        break;
      }

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

    if (getArguments.size() > 0)
    {
      request.getKeys = &getKeys[0];
      request.getValues = &getValues[0];
    }

    assert(callback != NULL);
    int32_t error = callback(reinterpret_cast<OrthancPluginRestOutput*>(&output), 
                             flatUri.c_str(), 
                             &request);

    if (error < 0)
    {
      LOG(ERROR) << "Plugin failed with error code " << error;
      return false;
    }
    else
    {
      if (error > 0)
      {
        LOG(WARNING) << "Plugin finished with warning code " << error;
      }

      return true;
    }
  }


  bool PluginsHttpHandler::InvokeService(_OrthancPluginService service,
                                         const void* parameters)
  {
    switch (service)
    {
      case _OrthancPluginService_RegisterRestCallback:
      {
        const _OrthancPluginRestCallback& p = 
          *reinterpret_cast<const _OrthancPluginRestCallback*>(parameters);

        LOG(INFO) << "Plugin has registered a REST callback on: " << p.pathRegularExpression;
        pimpl_->callbacks_.push_back(std::make_pair(new boost::regex(p.pathRegularExpression), p.callback));

        return true;
      }

      case _OrthancPluginService_AnswerBuffer:
      {
        const _OrthancPluginAnswerBuffer& p = 
          *reinterpret_cast<const _OrthancPluginAnswerBuffer*>(parameters);

        HttpOutput* translatedOutput = reinterpret_cast<HttpOutput*>(p.output);
        translatedOutput->AnswerBufferWithContentType(p.answer, p.answerSize, p.mimeType);

        return true;
      }

      case _OrthancPluginService_CompressAndAnswerPngImage:
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

        return true;
      }

      case _OrthancPluginService_GetDicomForInstance:
      {
        const _OrthancPluginGetDicomForInstance& p = 
          *reinterpret_cast<const _OrthancPluginGetDicomForInstance*>(parameters);

        std::string dicom;
        pimpl_->context_.ReadFile(dicom, p.instanceId, FileContentType_Dicom);

        p.target->size = dicom.size();

        if (dicom.size() == 0)
        {
          p.target->data = NULL;
        }
        else
        {
          p.target->data = malloc(dicom.size());
          if (p.target->data != NULL)
          {
            memcpy(p.target->data, &dicom[0], dicom.size());
          }
          else
          {
            return false;
          }
        }

        return true;
      }

      default:
        return false;
    }
  }

}
