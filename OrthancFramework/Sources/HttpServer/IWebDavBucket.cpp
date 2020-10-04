/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "IWebDavBucket.h"

#include "../OrthancException.h"
#include "../Toolbox.h"


static boost::posix_time::ptime GetNow()
{
  return boost::posix_time::second_clock::universal_time();
}


static std::string AddTrailingSlash(const std::string& s)
{
  if (s.empty() ||
      s[s.size() - 1] != '/')
  {
    return s + '/';
  }
  else
  {
    return s;
  }
}
  

namespace Orthanc
{
  void IWebDavBucket::Resource::SetNameInternal(const std::string& name)
  {
    if (name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find('\0') != std::string::npos)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Bad resource name for WebDAV: " + name);
    }
        
    name_ = name;
  }


  IWebDavBucket::Resource::Resource() :
    hasModificationTime_(false),
    creationTime_(GetNow()),
    modificationTime_(GetNow())
  {
  }


  void IWebDavBucket::Resource::SetCreationTime(const boost::posix_time::ptime& t)
  {
    creationTime_ = t;

    if (!hasModificationTime_)
    {
      modificationTime_ = t;
    }
  }


  void IWebDavBucket::Resource::SetModificationTime(const boost::posix_time::ptime& t)
  {
    modificationTime_ = t;
    hasModificationTime_ = true;
  }


  void IWebDavBucket::Resource::Format(pugi::xml_node& node,
                                        const std::string& parentPath) const
  {
    node.set_name("D:response");

    std::string s = AddTrailingSlash(parentPath) + GetName();
    node.append_child("D:href").append_child(pugi::node_pcdata).set_value(s.c_str());

    pugi::xml_node propstat = node.append_child("D:propstat");
    propstat.append_child("D:status").append_child(pugi::node_pcdata).
      set_value("HTTP/1.1 200 OK");

    pugi::xml_node prop = propstat.append_child("D:prop");

    // IMPORTANT: The "Z" suffix is mandatory on Windows >= 7
    s = boost::posix_time::to_iso_extended_string(GetCreationTime()) + "Z";
    prop.append_child("D:creationdate").append_child(pugi::node_pcdata).set_value(s.c_str());

    s = boost::posix_time::to_iso_extended_string(GetModificationTime()) + "Z";
    prop.append_child("D:getlastmodified").append_child(pugi::node_pcdata).set_value(s.c_str());

#if 0
    prop.append_child("D:lockdiscovery");
    pugi::xml_node lock = prop.append_child("D:supportedlock");

    pugi::xml_node lockentry = lock.append_child("D:lockentry");
    lockentry.append_child("D:lockscope").append_child("D:exclusive");
    lockentry.append_child("D:locktype").append_child("D:write");

    lockentry = lock.append_child("D:lockentry");
    lockentry.append_child("D:lockscope").append_child("D:shared");
    lockentry.append_child("D:locktype").append_child("D:write");
#endif
  }


  IWebDavBucket::File::File(const std::string& name) :
    contentLength_(0),
    mime_(MimeType_Binary)
  {
    if (name.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Cannot use an empty filename in WebDAV");          
    }
        
    SetNameInternal(name);
  }


  void IWebDavBucket::File::Format(pugi::xml_node& node,
                                   const std::string& parentPath) const
  {
    Resource::Format(node, parentPath);

    pugi::xml_node prop = node.first_element_by_path("D:propstat/D:prop");
    prop.append_child("D:resourcetype");

    std::string s = boost::lexical_cast<std::string>(contentLength_);
    prop.append_child("D:getcontentlength").append_child(pugi::node_pcdata).set_value(s.c_str());

    s = EnumerationToString(mime_);
    prop.append_child("D:getcontenttype").append_child(pugi::node_pcdata).set_value(s.c_str());

    prop.append_child("D:displayname").append_child(pugi::node_pcdata).set_value(GetName().c_str());
  }


  void IWebDavBucket::Folder::Format(pugi::xml_node& node,
                                     const std::string& parentPath) const
  {
    Resource::Format(node, parentPath);
        
    pugi::xml_node prop = node.first_element_by_path("D:propstat/D:prop");
    prop.append_child("D:resourcetype").append_child("D:collection");

    //prop.append_child("D:getcontenttype").append_child(pugi::node_pcdata).set_value("httpd/unix-directory");

    std::string s = GetName();
    prop.append_child("D:displayname").append_child(pugi::node_pcdata).set_value(s.c_str());
  }


  IWebDavBucket::Collection::~Collection()
  {
    for (std::list<Resource*>::iterator it = resources_.begin(); it != resources_.end(); ++it)
    {
      assert(*it != NULL);
      delete(*it);
    }
  }


  void IWebDavBucket::Collection::AddResource(Resource* resource)  // Takes ownership
  {
    if (resource == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      resources_.push_back(resource);
    }
  }


  void IWebDavBucket::Collection::Format(std::string& target,
                                         const std::string& parentPath) const
  {
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("D:multistatus");
    root.append_attribute("xmlns:D").set_value("DAV:");

    for (std::list<Resource*>::const_iterator
           it = resources_.begin(); it != resources_.end(); ++it)
    {
      assert(*it != NULL);
      pugi::xml_node n = root.append_child();
      (*it)->Format(n, parentPath);
    }

    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("UTF-8");

    Toolbox::XmlToString(target, doc);
  }
}
