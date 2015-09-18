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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <orthanc/OrthancCPlugin.h>

#include "../../../Core/Images/ImageBuffer.h"

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
