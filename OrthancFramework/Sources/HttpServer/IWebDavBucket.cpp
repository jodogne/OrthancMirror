/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "HttpOutput.h"
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
  IWebDavBucket::Resource::Resource(const std::string& displayName) :
    displayName_(displayName),
    hasModificationTime_(false),
    creationTime_(GetNow()),
    modificationTime_(GetNow())
  {
    if (displayName.empty() ||
        displayName.find('/') != std::string::npos ||
        displayName.find('\\') != std::string::npos ||
        displayName.find('\0') != std::string::npos)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Bad resource name for WebDAV: " + displayName);
    }
  }


  void IWebDavBucket::Resource::SetCreationTime(const boost::posix_time::ptime& t)
  {
    if (t.is_special())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Not a valid date-time");
    }
    else
    {
      creationTime_ = t;
      
      if (!hasModificationTime_)
      {
        modificationTime_ = t;
      }
    }
  }


  void IWebDavBucket::Resource::SetModificationTime(const boost::posix_time::ptime& t)
  {
    if (t.is_special())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Not a valid date-time");
    }
    else
    {
      modificationTime_ = t;
      hasModificationTime_ = true;
    }
  }


  static void FormatInternal(pugi::xml_node& node,
                             const std::string& href,
                             const std::string& displayName,
                             const boost::posix_time::ptime& creationTime,
                             const boost::posix_time::ptime& modificationTime)
  {
    node.set_name("D:response");

    node.append_child("D:href").append_child(pugi::node_pcdata).set_value(href.c_str());

    pugi::xml_node propstat = node.append_child("D:propstat");

    static const HttpStatus status = HttpStatus_200_Ok;
    std::string s = ("HTTP/1.1 " + boost::lexical_cast<std::string>(status) + " " +
                     std::string(EnumerationToString(status)));
    propstat.append_child("D:status").append_child(pugi::node_pcdata).set_value(s.c_str());

    pugi::xml_node prop = propstat.append_child("D:prop");
    prop.append_child("D:displayname").append_child(pugi::node_pcdata).set_value(displayName.c_str());

    // IMPORTANT: Adding the "Z" suffix is mandatory on Windows >= 7 (it indicates UTC)
    assert(!creationTime.is_special());
    s = boost::posix_time::to_iso_extended_string(creationTime) + "Z";
    prop.append_child("D:creationdate").append_child(pugi::node_pcdata).set_value(s.c_str());

    assert(!modificationTime.is_special());
    s = boost::posix_time::to_iso_extended_string(modificationTime) + "Z";
    prop.append_child("D:getlastmodified").append_child(pugi::node_pcdata).set_value(s.c_str());

#if 0
    // Maybe used by davfs2
    prop.append_child("D:quota-available-bytes");
    prop.append_child("D:quota-used-bytes");
#endif
    
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

  
  IWebDavBucket::File::File(const std::string& displayName) :
    Resource(displayName),
    contentLength_(0),
    mime_(MimeType_Binary)
  {
  }

  
  void IWebDavBucket::File::Format(pugi::xml_node& node,
                                   const std::string& parentPath) const
  {
    std::string href;
    Toolbox::UriEncode(href, AddTrailingSlash(parentPath) + GetDisplayName());
    FormatInternal(node, href, GetDisplayName(), GetCreationTime(), GetModificationTime());

    pugi::xml_node prop = node.first_element_by_path("D:propstat/D:prop");
    prop.append_child("D:resourcetype");

    std::string s = boost::lexical_cast<std::string>(contentLength_);
    prop.append_child("D:getcontentlength").append_child(pugi::node_pcdata).set_value(s.c_str());

    s = EnumerationToString(mime_);
    prop.append_child("D:getcontenttype").append_child(pugi::node_pcdata).set_value(s.c_str());
  }


  void IWebDavBucket::Folder::Format(pugi::xml_node& node,
                                     const std::string& parentPath) const
  {
    std::string href;
    Toolbox::UriEncode(href, AddTrailingSlash(parentPath) + GetDisplayName());
    FormatInternal(node, href, GetDisplayName(), GetCreationTime(), GetModificationTime());
        
    pugi::xml_node prop = node.first_element_by_path("D:propstat/D:prop");
    prop.append_child("D:resourcetype").append_child("D:collection");

    //prop.append_child("D:getcontenttype").append_child(pugi::node_pcdata).set_value("httpd/unix-directory");
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


  void IWebDavBucket::Collection::ListDisplayNames(std::set<std::string>& target)
  {
    for (std::list<Resource*>::iterator it = resources_.begin(); it != resources_.end(); ++it)
    {
      assert(*it != NULL);
      target.insert((*it)->GetDisplayName());
    }
  }


  void IWebDavBucket::Collection::Format(std::string& target,
                                         const std::string& parentPath) const
  {
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("D:multistatus");
    root.append_attribute("xmlns:D").set_value("DAV:");

    {
      pugi::xml_node self = root.append_child();

      std::vector<std::string> tokens;
      Toolbox::SplitUriComponents(tokens, parentPath);
      
      std::string folder;
      if (!tokens.empty())
      {
        folder = tokens.back();
      }
       
      std::string href;
      Toolbox::UriEncode(href, Toolbox::FlattenUri(tokens) + "/");

      boost::posix_time::ptime now = GetNow();
      FormatInternal(self, href, folder, now, now);

      pugi::xml_node prop = self.first_element_by_path("D:propstat/D:prop");
      prop.append_child("D:resourcetype").append_child("D:collection");
    }

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


  void IWebDavBucket::AnswerFakedProppatch(HttpOutput& output,
                                           const std::string& uri)
  {
    /**
     * This is a fake implementation. The goal is to make happy the
     * WebDAV clients that set properties (such as Windows >= 7).
     **/
            
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("D:multistatus");
    root.append_attribute("xmlns:D").set_value("DAV:");

    pugi::xml_node response = root.append_child("D:response");
    response.append_child("D:href").append_child(pugi::node_pcdata).set_value(uri.c_str());

    response.append_child("D:propstat");

    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("UTF-8");

    std::string s;
    Toolbox::XmlToString(s, doc);

    output.AddHeader("Content-Type", "application/xml");
    output.SendStatus(HttpStatus_207_MultiStatus, s);    
  }


  void IWebDavBucket::AnswerFakedLock(HttpOutput& output,
                                      const std::string& uri)
  {
    /**
     * This is a fake implementation. No lock is actually
     * created. The goal is to make happy the WebDAV clients
     * that use locking (such as Windows >= 7).
     **/
            
    pugi::xml_document doc;

    pugi::xml_node root = doc.append_child("D:prop");
    root.append_attribute("xmlns:D").set_value("DAV:");

    pugi::xml_node activelock = root.append_child("D:lockdiscovery").append_child("D:activelock");
    activelock.append_child("D:locktype").append_child("D:write");
    activelock.append_child("D:lockscope").append_child("D:exclusive");
    activelock.append_child("D:depth").append_child(pugi::node_pcdata).set_value("0");
    activelock.append_child("D:timeout").append_child(pugi::node_pcdata).set_value("Second-3599");

    activelock.append_child("D:lockroot").append_child("D:href")
      .append_child(pugi::node_pcdata).set_value(uri.c_str());
    activelock.append_child("D:owner");

    std::string token = Toolbox::GenerateUuid();
    boost::erase_all(token, "-");
    token = "opaquelocktoken:0x" + token;
            
    activelock.append_child("D:locktoken").append_child("D:href").
      append_child(pugi::node_pcdata).set_value(token.c_str());
            
    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("UTF-8");

    std::string s;
    Toolbox::XmlToString(s, doc);

    output.AddHeader("Lock-Token", token);  // Necessary for davfs2
    output.AddHeader("Content-Type", "application/xml");
    output.SendStatus(HttpStatus_201_Created, s);
  }


  void IWebDavBucket::AnswerFakedUnlock(HttpOutput& output)
  {
    output.SendStatus(HttpStatus_204_NoContent);
  }
}
