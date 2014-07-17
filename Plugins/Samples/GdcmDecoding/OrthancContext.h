/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


#pragma once

#include <OrthancCPlugin.h>

#include "../../../Core/ImageFormats/ImageBuffer.h"

#include <map>
#include <string>
#include <boost/noncopyable.hpp>


class OrthancContext : public boost::noncopyable
{
private:
  OrthancPluginContext* context_;

  OrthancContext() : context_(NULL)
  {
  }

  void Check();

public:
  typedef std::map<std::string, std::string>  Arguments;

  static OrthancContext& GetInstance();

  void Initialize(OrthancPluginContext* context)
  {
    context_ = context;
  }

  void Finalize()
  {
    context_ = NULL;
  }

  void ExtractGetArguments(Arguments& arguments,
                           const OrthancPluginHttpRequest& request);

  void LogError(const std::string& s);

  void LogWarning(const std::string& s);

  void LogInfo(const std::string& s);
  
  void Register(const std::string& uri,
                OrthancPluginRestCallback callback);

  void GetDicomForInstance(std::string& result,
                           const std::string& instanceId);

  void CompressAndAnswerPngImage(OrthancPluginRestOutput* output,
                                 const Orthanc::ImageAccessor& accessor);

  void Redirect(OrthancPluginRestOutput* output,
                const std::string& s);
};
