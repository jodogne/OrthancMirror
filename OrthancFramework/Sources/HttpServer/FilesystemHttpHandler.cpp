/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "FilesystemHttpHandler.h"

#include "../OrthancException.h"
#include "../SystemToolbox.h"
#include "FilesystemHttpSender.h"

#include <boost/filesystem.hpp>


namespace Orthanc
{
  struct FilesystemHttpHandler::PImpl
  {
    UriComponents baseUri_;
    boost::filesystem::path root_;
  };



  static void OutputDirectoryContent(HttpOutput& output,
                                     const HttpToolbox::Arguments& headers,
                                     const UriComponents& uri,
                                     const boost::filesystem::path& p)
  {
    namespace fs = boost::filesystem;

    std::string s;
    s += "<html>";
    s += "  <body>";
    s += "    <h1>Subdirectories</h1>";
    s += "    <ul>";

    if (uri.size() > 0)
    {
      std::string h = Toolbox::FlattenUri(uri) + "/..";
      s += "<li><a href=\"" + h + "\">..</a></li>";
    }

    fs::directory_iterator end;
    for (fs::directory_iterator it(p) ; it != end; ++it)
    {
#if BOOST_HAS_FILESYSTEM_V3 == 1
      std::string f = it->path().filename().string();
#else
      std::string f = it->path().filename();
#endif

      std::string h = Toolbox::FlattenUri(uri) + "/" + f;
      if (fs::is_directory(it->status()))
        s += "<li><a href=\"" + h + "\">" + f + "</a></li>";
    }      

    s += "    </ul>";      
    s += "    <h1>Files</h1>";
    s += "    <ul>";

    for (fs::directory_iterator it(p) ; it != end; ++it)
    {
#if BOOST_HAS_FILESYSTEM_V3 == 1
      std::string f = it->path().filename().string();
#else
      std::string f = it->path().filename();
#endif

      std::string h = Toolbox::FlattenUri(uri) + "/" + f;
      if (SystemToolbox::IsRegularFile(it->path().string()))
      {
        s += "<li><a href=\"" + h + "\">" + f + "</a></li>";
      }
    }      

    s += "    </ul>";
    s += "  </body>";
    s += "</html>";

    output.SetContentType(MimeType_Html);
    output.Answer(s);
  }


  FilesystemHttpHandler::FilesystemHttpHandler(const std::string& baseUri,
                                               const std::string& root) : pimpl_(new PImpl)
  {
    Toolbox::SplitUriComponents(pimpl_->baseUri_, baseUri);
    pimpl_->root_ = root;
    listDirectoryContent_ = false;
    
    namespace fs = boost::filesystem;
    if (!fs::exists(pimpl_->root_) || 
        !fs::is_directory(pimpl_->root_))
    {
      throw OrthancException(ErrorCode_DirectoryExpected);
    }
  }


  bool FilesystemHttpHandler::Handle(HttpOutput& output,
                                     RequestOrigin /*origin*/,
                                     const char* /*remoteIp*/,
                                     const char* /*username*/,
                                     HttpMethod method,
                                     const UriComponents& uri,
                                     const HttpToolbox::Arguments& headers,
                                     const HttpToolbox::GetArguments& arguments,
                                     const void* /*bodyData*/,
                                     size_t /*bodySize*/)
  {
    if (!Toolbox::IsChildUri(pimpl_->baseUri_, uri))
    {
      // This URI is not served by this handler
      return false;
    }

    if (method != HttpMethod_Get)
    {
      output.SendMethodNotAllowed("GET");
      return true;
    }

    namespace fs = boost::filesystem;

    fs::path p = pimpl_->root_;
    for (size_t i = pimpl_->baseUri_.size(); i < uri.size(); i++)
    {
      p /= uri[i];
    }

    if (SystemToolbox::IsRegularFile(p.string()))
    {
      FilesystemHttpSender sender(p);
      sender.SetContentType(SystemToolbox::AutodetectMimeType(p.string()));
      output.Answer(sender);   // TODO COMPRESSION
    }
    else if (listDirectoryContent_ &&
             fs::exists(p) && 
             fs::is_directory(p))
    {
      OutputDirectoryContent(output, headers, uri, p);
    }
    else
    {
      output.SendStatus(HttpStatus_404_NotFound);
    }

    return true;
  } 
}
