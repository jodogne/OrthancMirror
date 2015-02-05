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


#include "../Core/PrecompiledHeaders.h"
#include "OrthancConnection.h"

#include "../Core/Toolbox.h"

namespace OrthancClient
{
  void OrthancConnection::ReadPatients()
  {
    client_.SetMethod(Orthanc::HttpMethod_Get);
    client_.SetUrl(orthancUrl_ + "/patients");

    Json::Value v;
    if (!client_.Apply(content_))
    {
      throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
    }
  }

  Orthanc::IDynamicObject* OrthancConnection::GetFillerItem(size_t index)
  {
    Json::Value::ArrayIndex tmp = static_cast<Json::Value::ArrayIndex>(index);
    std::string id = content_[tmp].asString();
    return new Patient(*this, id.c_str());
  }

  Patient& OrthancConnection::GetPatient(unsigned int index)
  {
    return dynamic_cast<Patient&>(patients_.GetItem(index));
  }

  OrthancConnection::OrthancConnection(const char* orthancUrl) : 
    orthancUrl_(orthancUrl), patients_(*this)
  {
    ReadPatients();
  }
  
  OrthancConnection::OrthancConnection(const char* orthancUrl,
                                       const char* username, 
                                       const char* password) : 
    orthancUrl_(orthancUrl), patients_(*this)
  {
    client_.SetCredentials(username, password);
    ReadPatients();
  }


  void OrthancConnection::Store(const void* dicom, uint64_t size)
  {
    if (size == 0)
    {
      return;
    }

    client_.SetMethod(Orthanc::HttpMethod_Post);
    client_.SetUrl(orthancUrl_ + "/instances");

    // Copy the DICOM file in the POST body. TODO - Avoid memory copy
    client_.AccessPostData().resize(static_cast<size_t>(size));
    memcpy(&client_.AccessPostData()[0], dicom, static_cast<size_t>(size));

    Json::Value v;
    if (!client_.Apply(v))
    {
      throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
    }
    
    Reload();
  }


  void  OrthancConnection::StoreFile(const char* filename)
  {
    std::string content;
    Orthanc::Toolbox::ReadFile(content, filename);

    if (content.size() != 0)
    {
      Store(&content[0], content.size());
    }
  }

}
