/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "OrthancPluginCppWrapper.h"

#include <json/reader.h>
#include <json/writer.h>


namespace OrthancPlugins
{
  void MemoryBuffer::Check(OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      // Prevent using garbage information
      buffer_.data = NULL;
      buffer_.size = 0;
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(code);
    }
  }


  bool MemoryBuffer::CheckHttp(OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      // Prevent using garbage information
      buffer_.data = NULL;
      buffer_.size = 0;
    }

    if (code == OrthancPluginErrorCode_Success)
    {
      return true;
    }
    else if (code == OrthancPluginErrorCode_UnknownResource ||
             code == OrthancPluginErrorCode_InexistentItem)
    {
      return false;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(code);
    }
  }


  MemoryBuffer::MemoryBuffer(OrthancPluginContext* context) : 
    context_(context)
  {
    buffer_.data = NULL;
    buffer_.size = 0;
  }


  void MemoryBuffer::Clear()
  {
    if (buffer_.data != NULL)
    {
      OrthancPluginFreeMemoryBuffer(context_, &buffer_);
      buffer_.data = NULL;
      buffer_.size = 0;
    }
  }


  void MemoryBuffer::Assign(OrthancPluginMemoryBuffer& other)
  {
    Clear();

    buffer_.data = other.data;
    buffer_.size = other.size;

    other.data = NULL;
    other.size = 0;
  }


  OrthancPluginMemoryBuffer MemoryBuffer::Release()
  {
    OrthancPluginMemoryBuffer result = buffer_;

    buffer_.data = NULL;
    buffer_.size = 0;
    
    return result;
  }


  void MemoryBuffer::ToString(std::string& target) const
  {
    if (buffer_.size == 0)
    {
      target.clear();
    }
    else
    {
      target.assign(reinterpret_cast<const char*>(buffer_.data), buffer_.size);
    }
  }


  void MemoryBuffer::ToJson(Json::Value& target) const
  {
    if (buffer_.data == NULL ||
        buffer_.size == 0)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }

    const char* tmp = reinterpret_cast<const char*>(buffer_.data);

    Json::Reader reader;
    if (!reader.parse(tmp, tmp + buffer_.size, target))
    {
      OrthancPluginLogError(context_, "Cannot convert some memory buffer to JSON");
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }


  bool MemoryBuffer::RestApiGet(const std::string& uri,
                                bool applyPlugins)
  {
    Clear();

    if (applyPlugins)
    {
      return CheckHttp(OrthancPluginRestApiGetAfterPlugins(context_, &buffer_, uri.c_str()));
    }
    else
    {
      return CheckHttp(OrthancPluginRestApiGet(context_, &buffer_, uri.c_str()));
    }
  }

  
  bool MemoryBuffer::RestApiPost(const std::string& uri,
                                 const char* body,
                                 size_t bodySize,
                                 bool applyPlugins)
  {
    Clear();

    if (applyPlugins)
    {
      return CheckHttp(OrthancPluginRestApiPostAfterPlugins(context_, &buffer_, uri.c_str(), body, bodySize));
    }
    else
    {
      return CheckHttp(OrthancPluginRestApiPost(context_, &buffer_, uri.c_str(), body, bodySize));
    }
  }


  bool MemoryBuffer::RestApiPut(const std::string& uri,
                                const char* body,
                                size_t bodySize,
                                bool applyPlugins)
  {
    Clear();

    if (applyPlugins)
    {
      return CheckHttp(OrthancPluginRestApiPutAfterPlugins(context_, &buffer_, uri.c_str(), body, bodySize));
    }
    else
    {
      return CheckHttp(OrthancPluginRestApiPut(context_, &buffer_, uri.c_str(), body, bodySize));
    }
  }


  bool MemoryBuffer::RestApiPost(const std::string& uri,
                                 const Json::Value& body,
                                 bool applyPlugins)
  {
    Json::FastWriter writer;
    return RestApiPost(uri, writer.write(body), applyPlugins);
  }


  bool MemoryBuffer::RestApiPut(const std::string& uri,
                                const Json::Value& body,
                                bool applyPlugins)
  {
    Json::FastWriter writer;
    return RestApiPut(uri, writer.write(body), applyPlugins);
  }


  void MemoryBuffer::CreateDicom(const Json::Value& tags,
                                 OrthancPluginCreateDicomFlags flags)
  {
    Clear();

    Json::FastWriter writer;
    std::string s = writer.write(tags);
    
    Check(OrthancPluginCreateDicom(context_, &buffer_, s.c_str(), NULL, flags));
  }


  void MemoryBuffer::ReadFile(const std::string& path)
  {
    Clear();
    Check(OrthancPluginReadFile(context_, &buffer_, path.c_str()));
  }


  void MemoryBuffer::GetDicomQuery(const OrthancPluginWorklistQuery* query)
  {
    Clear();
    Check(OrthancPluginWorklistGetDicomQuery(context_, &buffer_, query));
  }


  void OrthancString::Assign(char* str)
  {
    if (str == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }
    else
    {
      Clear();
      str_ = str;
    }
  }


  void OrthancString::Clear()
  {
    if (str_ != NULL)
    {
      OrthancPluginFreeString(context_, str_);
      str_ = NULL;
    }
  }


  void OrthancString::ToString(std::string& target) const
  {
    if (str_ == NULL)
    {
      target.clear();
    }
    else
    {
      target.assign(str_);
    }
  }


  void OrthancString::ToJson(Json::Value& target) const
  {
    if (str_ == NULL)
    {
      OrthancPluginLogError(context_, "Cannot convert an empty memory buffer to JSON");
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }

    Json::Reader reader;
    if (!reader.parse(str_, target))
    {
      OrthancPluginLogError(context_, "Cannot convert some memory buffer to JSON");
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }

  
  void MemoryBuffer::DicomToJson(Json::Value& target,
                                 OrthancPluginDicomToJsonFormat format,
                                 OrthancPluginDicomToJsonFlags flags,
                                 uint32_t maxStringLength)
  {
    OrthancString str(context_);
    str.Assign(OrthancPluginDicomBufferToJson(context_, GetData(), GetSize(), format, flags, maxStringLength));
    str.ToJson(target);
  }


  bool MemoryBuffer::HttpGet(const std::string& url,
                             const std::string& username,
                             const std::string& password)
  {
    Clear();
    return CheckHttp(OrthancPluginHttpGet(context_, &buffer_, url.c_str(),
                                          username.empty() ? NULL : username.c_str(),
                                          password.empty() ? NULL : password.c_str()));
  }

  
  bool MemoryBuffer::HttpPost(const std::string& url,
                              const std::string& body,
                              const std::string& username,
                              const std::string& password)
  {
    Clear();
    return CheckHttp(OrthancPluginHttpPost(context_, &buffer_, url.c_str(),
                                           body.c_str(), body.size(),
                                           username.empty() ? NULL : username.c_str(),
                                           password.empty() ? NULL : password.c_str()));
  }
  
 
  bool MemoryBuffer::HttpPut(const std::string& url,
                             const std::string& body,
                             const std::string& username,
                             const std::string& password)
  {
    Clear();
    return CheckHttp(OrthancPluginHttpPut(context_, &buffer_, url.c_str(),
                                          body.empty() ? NULL : body.c_str(),
                                          body.size(),
                                          username.empty() ? NULL : username.c_str(),
                                          password.empty() ? NULL : password.c_str()));
  }
  

  void MemoryBuffer::GetDicomInstance(const std::string& instanceId)
  {
    Clear();
    Check(OrthancPluginGetDicomForInstance(context_, &buffer_, instanceId.c_str()));
  }

  
  bool HttpDelete(OrthancPluginContext* context_,
                  const std::string& url,
                  const std::string& username,
                  const std::string& password)
  {
    OrthancPluginErrorCode error = OrthancPluginHttpDelete
      (context_, url.c_str(),
       username.empty() ? NULL : username.c_str(),
       password.empty() ? NULL : password.c_str());
  
    if (error == OrthancPluginErrorCode_Success)
    {
      return true;
    }
    else if (error == OrthancPluginErrorCode_UnknownResource ||
             error == OrthancPluginErrorCode_InexistentItem)
    {
      return false;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(error);
    }
  }
  

  OrthancConfiguration::OrthancConfiguration(OrthancPluginContext* context) : 
    context_(context)
  {
    OrthancString str(context);
    str.Assign(OrthancPluginGetConfiguration(context));

    if (str.GetContent() == NULL)
    {
      OrthancPluginLogError(context, "Cannot access the Orthanc configuration");
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }

    str.ToJson(configuration_);

    if (configuration_.type() != Json::objectValue)
    {
      OrthancPluginLogError(context, "Unable to read the Orthanc configuration");
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }
  }


  OrthancPluginContext* OrthancConfiguration::GetContext() const
  {
    if (context_ == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(Plugin);
    }
    else
    {
      return context_;
    }
  }


  std::string OrthancConfiguration::GetPath(const std::string& key) const
  {
    if (path_.empty())
    {
      return key;
    }
    else
    {
      return path_ + "." + key;
    }
  }


  bool OrthancConfiguration::IsSection(const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    return (configuration_.isMember(key) &&
            configuration_[key].type() == Json::objectValue);
  }


  void OrthancConfiguration::GetSection(OrthancConfiguration& target,
                                        const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    target.context_ = context_;
    target.path_ = GetPath(key);

    if (!configuration_.isMember(key))
    {
      target.configuration_ = Json::objectValue;
    }
    else
    {
      if (configuration_[key].type() != Json::objectValue)
      {
        if (context_ != NULL)
        {
          std::string s = "The configuration section \"" + target.path_ + "\" is not an associative array as expected";
          OrthancPluginLogError(context_, s.c_str());
        }

        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
      }

      target.configuration_ = configuration_[key];
    }
  }


  bool OrthancConfiguration::LookupStringValue(std::string& target,
                                               const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    if (!configuration_.isMember(key))
    {
      return false;
    }

    if (configuration_[key].type() != Json::stringValue)
    {
      if (context_ != NULL)
      {
        std::string s = "The configuration option \"" + GetPath(key) + "\" is not a string as expected";
        OrthancPluginLogError(context_, s.c_str());
      }

      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    target = configuration_[key].asString();
    return true;
  }


  bool OrthancConfiguration::LookupIntegerValue(int& target,
                                                const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    if (!configuration_.isMember(key))
    {
      return false;
    }

    switch (configuration_[key].type())
    {
      case Json::intValue:
        target = configuration_[key].asInt();
        return true;
        
      case Json::uintValue:
        target = configuration_[key].asUInt();
        return true;
        
      default:
        if (context_ != NULL)
        {
          std::string s = "The configuration option \"" + GetPath(key) + "\" is not an integer as expected";
          OrthancPluginLogError(context_, s.c_str());
        }

        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }


  bool OrthancConfiguration::LookupUnsignedIntegerValue(unsigned int& target,
                                                        const std::string& key) const
  {
    int tmp;
    if (!LookupIntegerValue(tmp, key))
    {
      return false;
    }

    if (tmp < 0)
    {
      if (context_ != NULL)
      {
        std::string s = "The configuration option \"" + GetPath(key) + "\" is not a positive integer as expected";
        OrthancPluginLogError(context_, s.c_str());
      }

      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
    else
    {
      target = static_cast<unsigned int>(tmp);
      return true;
    }
  }


  bool OrthancConfiguration::LookupBooleanValue(bool& target,
                                                const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    if (!configuration_.isMember(key))
    {
      return false;
    }

    if (configuration_[key].type() != Json::booleanValue)
    {
      if (context_ != NULL)
      {
        std::string s = "The configuration option \"" + GetPath(key) + "\" is not a Boolean as expected";
        OrthancPluginLogError(context_, s.c_str());
      }

      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    target = configuration_[key].asBool();
    return true;
  }


  bool OrthancConfiguration::LookupFloatValue(float& target,
                                              const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    if (!configuration_.isMember(key))
    {
      return false;
    }

    switch (configuration_[key].type())
    {
      case Json::realValue:
        target = configuration_[key].asFloat();
        return true;
        
      case Json::intValue:
        target = static_cast<float>(configuration_[key].asInt());
        return true;
        
      case Json::uintValue:
        target = static_cast<float>(configuration_[key].asUInt());
        return true;
        
      default:
        if (context_ != NULL)
        {
          std::string s = "The configuration option \"" + GetPath(key) + "\" is not an integer as expected";
          OrthancPluginLogError(context_, s.c_str());
        }

        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }
  }


  bool OrthancConfiguration::LookupListOfStrings(std::list<std::string>& target,
                                                 const std::string& key,
                                                 bool allowSingleString) const
  {
    assert(configuration_.type() == Json::objectValue);

    target.clear();

    if (!configuration_.isMember(key))
    {
      return false;
    }

    switch (configuration_[key].type())
    {
      case Json::arrayValue:
      {
        bool ok = true;
    
        for (Json::Value::ArrayIndex i = 0; ok && i < configuration_[key].size(); i++)
        {
          if (configuration_[key][i].type() == Json::stringValue)
          {
            target.push_back(configuration_[key][i].asString());
          }
          else
          {
            ok = false;
          }
        }

        if (ok)
        {
          return true;
        }

        break;
      }

      case Json::stringValue:
        if (allowSingleString)
        {
          target.push_back(configuration_[key].asString());
          return true;
        }

        break;

      default:
        break;
    }

    if (context_ != NULL)
    {
      std::string s = ("The configuration option \"" + GetPath(key) +
                       "\" is not a list of strings as expected");
      OrthancPluginLogError(context_, s.c_str());
    }

    ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
  }


  bool OrthancConfiguration::LookupSetOfStrings(std::set<std::string>& target,
                                                const std::string& key,
                                                bool allowSingleString) const
  {
    std::list<std::string> lst;

    if (LookupListOfStrings(lst, key, allowSingleString))
    {
      target.clear();

      for (std::list<std::string>::const_iterator
             it = lst.begin(); it != lst.end(); ++it)
      {
        target.insert(*it);
      }

      return true;
    }
    else
    {
      return false;
    }
  }

  
  std::string OrthancConfiguration::GetStringValue(const std::string& key,
                                                   const std::string& defaultValue) const
  {
    std::string tmp;
    if (LookupStringValue(tmp, key))
    {
      return tmp;
    }
    else
    {
      return defaultValue;
    }
  }


  int OrthancConfiguration::GetIntegerValue(const std::string& key,
                                            int defaultValue) const
  {
    int tmp;
    if (LookupIntegerValue(tmp, key))
    {
      return tmp;
    }
    else
    {
      return defaultValue;
    }
  }


  unsigned int OrthancConfiguration::GetUnsignedIntegerValue(const std::string& key,
                                                             unsigned int defaultValue) const
  {
    unsigned int tmp;
    if (LookupUnsignedIntegerValue(tmp, key))
    {
      return tmp;
    }
    else
    {
      return defaultValue;
    }
  }


  bool OrthancConfiguration::GetBooleanValue(const std::string& key,
                                             bool defaultValue) const
  {
    bool tmp;
    if (LookupBooleanValue(tmp, key))
    {
      return tmp;
    }
    else
    {
      return defaultValue;
    }
  }


  float OrthancConfiguration::GetFloatValue(const std::string& key,
                                            float defaultValue) const
  {
    float tmp;
    if (LookupFloatValue(tmp, key))
    {
      return tmp;
    }
    else
    {
      return defaultValue;
    }
  }


  void OrthancConfiguration::GetDictionary(std::map<std::string, std::string>& target,
                                           const std::string& key) const
  {
    assert(configuration_.type() == Json::objectValue);

    target.clear();

    if (!configuration_.isMember(key))
    {
      return;
    }

    if (configuration_[key].type() != Json::objectValue)
    {
      if (context_ != NULL)
      {
        std::string s = "The configuration option \"" + GetPath(key) + "\" is not a string as expected";
        OrthancPluginLogError(context_, s.c_str());
      }

      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    Json::Value::Members members = configuration_[key].getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      const Json::Value& value = configuration_[key][members[i]];

      if (value.type() == Json::stringValue)
      {
        target[members[i]] = value.asString();
      }
      else
      {
        if (context_ != NULL)
        {
          std::string s = "The configuration option \"" + GetPath(key) + "\" is not a dictionary mapping strings to strings";
          OrthancPluginLogError(context_, s.c_str());
        }

        ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
      }
    }
  }


  void OrthancImage::Clear()
  {
    if (image_ != NULL)
    {
      OrthancPluginFreeImage(context_, image_);
      image_ = NULL;
    }
  }


  void OrthancImage::CheckImageAvailable()
  {
    if (image_ == NULL)
    {
      OrthancPluginLogError(context_, "Trying to access a NULL image");
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  OrthancImage::OrthancImage(OrthancPluginContext*  context) :
    context_(context),
    image_(NULL)
  {
    if (context == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  OrthancImage::OrthancImage(OrthancPluginContext*  context,
                             OrthancPluginImage*    image) :
    context_(context),
    image_(image)
  {
    if (context == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }
  

  OrthancImage::OrthancImage(OrthancPluginContext*     context,
                             OrthancPluginPixelFormat  format,
                             uint32_t                  width,
                             uint32_t                  height) :
    context_(context)
  {
    if (context == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
    else
    {
      image_ = OrthancPluginCreateImage(context, format, width, height);
    }
  }


  void OrthancImage::UncompressPngImage(const void* data,
                                        size_t size)
  {
    Clear();
    image_ = OrthancPluginUncompressImage(context_, data, size, OrthancPluginImageFormat_Png);
    if (image_ == NULL)
    {
      OrthancPluginLogError(context_, "Cannot uncompress a PNG image");
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  void OrthancImage::UncompressJpegImage(const void* data,
                                         size_t size)
  {
    Clear();
    image_ = OrthancPluginUncompressImage(context_, data, size, OrthancPluginImageFormat_Jpeg);
    if (image_ == NULL)
    {
      OrthancPluginLogError(context_, "Cannot uncompress a JPEG image");
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  void OrthancImage::DecodeDicomImage(const void* data,
                                      size_t size,
                                      unsigned int frame)
  {
    Clear();
    image_ = OrthancPluginDecodeDicomImage(context_, data, size, frame);
    if (image_ == NULL)
    {
      OrthancPluginLogError(context_, "Cannot uncompress a DICOM image");
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  OrthancPluginPixelFormat OrthancImage::GetPixelFormat()
  {
    CheckImageAvailable();
    return OrthancPluginGetImagePixelFormat(context_, image_);
  }


  unsigned int OrthancImage::GetWidth()
  {
    CheckImageAvailable();
    return OrthancPluginGetImageWidth(context_, image_);
  }


  unsigned int OrthancImage::GetHeight()
  {
    CheckImageAvailable();
    return OrthancPluginGetImageHeight(context_, image_);
  }


  unsigned int OrthancImage::GetPitch()
  {
    CheckImageAvailable();
    return OrthancPluginGetImagePitch(context_, image_);
  }

    
  const void* OrthancImage::GetBuffer()
  {
    CheckImageAvailable();
    return OrthancPluginGetImageBuffer(context_, image_);
  }


  void OrthancImage::CompressPngImage(MemoryBuffer& target)
  {
    CheckImageAvailable();
    
    OrthancPluginMemoryBuffer tmp;
    OrthancPluginCompressPngImage(context_, &tmp, GetPixelFormat(), 
                                  GetWidth(), GetHeight(), GetPitch(), GetBuffer());

    target.Assign(tmp);
  }


  void OrthancImage::CompressJpegImage(MemoryBuffer& target,
                                       uint8_t quality)
  {
    CheckImageAvailable();
    
    OrthancPluginMemoryBuffer tmp;
    OrthancPluginCompressJpegImage(context_, &tmp, GetPixelFormat(), 
                                   GetWidth(), GetHeight(), GetPitch(), GetBuffer(), quality);
    
    target.Assign(tmp);
  }


  void OrthancImage::AnswerPngImage(OrthancPluginRestOutput* output)
  {
    CheckImageAvailable();
    OrthancPluginCompressAndAnswerPngImage(context_, output, GetPixelFormat(),
                                           GetWidth(), GetHeight(), GetPitch(), GetBuffer());
  }


  void OrthancImage::AnswerJpegImage(OrthancPluginRestOutput* output,
                                     uint8_t quality)
  {
    CheckImageAvailable();
    OrthancPluginCompressAndAnswerJpegImage(context_, output, GetPixelFormat(),
                                            GetWidth(), GetHeight(), GetPitch(), GetBuffer(), quality);
  }



#if HAS_ORTHANC_PLUGIN_FIND_MATCHER == 1
  FindMatcher::FindMatcher(OrthancPluginContext*              context,
                           const OrthancPluginWorklistQuery*  worklist) :
    context_(context),
    matcher_(NULL),
    worklist_(worklist)
  {
    if (worklist_ == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  void FindMatcher::SetupDicom(OrthancPluginContext*  context,
                               const void*            query,
                               uint32_t               size)
  {
    context_ = context;
    worklist_ = NULL;

    matcher_ = OrthancPluginCreateFindMatcher(context_, query, size);
    if (matcher_ == NULL)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }
  }


  FindMatcher::~FindMatcher()
  {
    // The "worklist_" field 

    if (matcher_ != NULL)
    {
      OrthancPluginFreeFindMatcher(context_, matcher_);
    }
  }



  bool FindMatcher::IsMatch(const void*  dicom,
                            uint32_t     size) const
  {
    int32_t result;

    if (matcher_ != NULL)
    {
      result = OrthancPluginFindMatcherIsMatch(context_, matcher_, dicom, size);
    }
    else if (worklist_ != NULL)
    {
      result = OrthancPluginWorklistIsMatch(context_, worklist_, dicom, size);
    }
    else
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }

    if (result == 0)
    {
      return false;
    }
    else if (result == 1)
    {
      return true;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InternalError);
    }
  }

#endif /* HAS_ORTHANC_PLUGIN_FIND_MATCHER == 1 */


  bool RestApiGet(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  bool applyPlugins)
  {
    MemoryBuffer answer(context);
    if (!answer.RestApiGet(uri, applyPlugins))
    {
      return false;
    }
    else
    {
      answer.ToJson(result);
      return true;
    }
  }


  bool RestApiPost(Json::Value& result,
                   OrthancPluginContext* context,
                   const std::string& uri,
                   const char* body,
                   size_t bodySize,
                   bool applyPlugins)
  {
    MemoryBuffer answer(context);
    if (!answer.RestApiPost(uri, body, bodySize, applyPlugins))
    {
      return false;
    }
    else
    {
      answer.ToJson(result);
      return true;
    }
  }


  bool RestApiPost(Json::Value& result,
                   OrthancPluginContext* context,
                   const std::string& uri,
                   const Json::Value& body,
                   bool applyPlugins)
  {
    Json::FastWriter writer;
    return RestApiPost(result, context, uri, writer.write(body), applyPlugins);
  }


  bool RestApiPut(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  const char* body,
                  size_t bodySize,
                  bool applyPlugins)
  {
    MemoryBuffer answer(context);
    if (!answer.RestApiPut(uri, body, bodySize, applyPlugins))
    {
      return false;
    }
    else
    {
      answer.ToJson(result);
      return true;
    }
  }


  bool RestApiPut(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  const Json::Value& body,
                  bool applyPlugins)
  {
    Json::FastWriter writer;
    return RestApiPut(result, context, uri, writer.write(body), applyPlugins);
  }


  bool RestApiDelete(OrthancPluginContext* context,
                     const std::string& uri,
                     bool applyPlugins)
  {
    OrthancPluginErrorCode error;

    if (applyPlugins)
    {
      error = OrthancPluginRestApiDeleteAfterPlugins(context, uri.c_str());
    }
    else
    {
      error = OrthancPluginRestApiDelete(context, uri.c_str());
    }

    if (error == OrthancPluginErrorCode_Success)
    {
      return true;
    }
    else if (error == OrthancPluginErrorCode_UnknownResource ||
             error == OrthancPluginErrorCode_InexistentItem)
    {
      return false;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(error);
    }
  }


  void ReportMinimalOrthancVersion(OrthancPluginContext* context,
                                   unsigned int major,
                                   unsigned int minor,
                                   unsigned int revision)
  {
    std::string s = ("Your version of the Orthanc core (" +
                     std::string(context->orthancVersion) +
                     ") is too old to run this plugin (version " +
                     boost::lexical_cast<std::string>(major) + "." +
                     boost::lexical_cast<std::string>(minor) + "." +
                     boost::lexical_cast<std::string>(revision) + 
                     " is required)");
    
    OrthancPluginLogError(context, s.c_str());
  }


  bool CheckMinimalOrthancVersion(OrthancPluginContext* context,
                                  unsigned int major,
                                  unsigned int minor,
                                  unsigned int revision)
  {
    if (context == NULL)
    {
      OrthancPluginLogError(context, "Bad Orthanc context in the plugin");      
      return false;
    }

    if (!strcmp(context->orthancVersion, "mainline"))
    {
      // Assume compatibility with the mainline
      return true;
    }

    // Parse the version of the Orthanc core
    int aa, bb, cc;
    if ( 
#ifdef _MSC_VER
      sscanf_s
#else
      sscanf
#endif
      (context->orthancVersion, "%4d.%4d.%4d", &aa, &bb, &cc) != 3 ||
      aa < 0 ||
      bb < 0 ||
      cc < 0)
    {
      return false;
    }

    unsigned int a = static_cast<unsigned int>(aa);
    unsigned int b = static_cast<unsigned int>(bb);
    unsigned int c = static_cast<unsigned int>(cc);

    // Check the major version number

    if (a > major)
    {
      return true;
    }

    if (a < major)
    {
      return false;
    }


    // Check the minor version number
    assert(a == major);

    if (b > minor)
    {
      return true;
    }

    if (b < minor)
    {
      return false;
    }

    // Check the patch level version number
    assert(a == major && b == minor);

    if (c >= revision)
    {
      return true;
    }
    else
    {
      return false;
    }
  }




#if HAS_ORTHANC_PLUGIN_PEERS == 1
  OrthancPeers::OrthancPeers(OrthancPluginContext* context) :
    context_(context),
    peers_(NULL),
    timeout_(0)
  {
    if (context_ == NULL)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_NullPointer);          
    }

    peers_ = OrthancPluginGetPeers(context_);

    if (peers_ == NULL)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
    }

    uint32_t count = OrthancPluginGetPeersCount(context_, peers_);

    for (uint32_t i = 0; i < count; i++)
    {
      const char* name = OrthancPluginGetPeerName(context_, peers_, i);
      if (name == NULL)
      {
        OrthancPluginFreePeers(context_, peers_);
        ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
      }

      index_[name] = i;
    }
  }

  
  OrthancPeers::~OrthancPeers()
  {
    if (peers_ != NULL)
    {
      OrthancPluginFreePeers(context_, peers_);
    }
  }

  
  bool OrthancPeers::LookupName(size_t& target,
                                const std::string& name) const
  {
    Index::const_iterator found = index_.find(name);

    if (found == index_.end())
    {
      return false;
    }
    else
    {
      target = found->second;
      return true;
    }
  }

  
  std::string OrthancPeers::GetPeerName(size_t index) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }
    else
    {
      const char* s = OrthancPluginGetPeerName(context_, peers_, static_cast<uint32_t>(index));
      if (s == NULL)
      {
        ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
      }
      else
      {
        return s;
      }
    }
  }
  

  std::string OrthancPeers::GetPeerUrl(size_t index) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }
    else
    {
      const char* s = OrthancPluginGetPeerUrl(context_, peers_, static_cast<uint32_t>(index));
      if (s == NULL)
      {
        ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
      }
      else
      {
        return s;
      }
    }
  }

  
  std::string OrthancPeers::GetPeerUrl(const std::string& name) const
  {
    size_t index;
    if (LookupName(index, name))
    {
      return GetPeerUrl(index);
    }
    else
    {
      std::string s = "Inexistent peer: " + name;
      OrthancPluginLogError(context_, s.c_str());
      ORTHANC_PLUGINS_THROW_EXCEPTION(UnknownResource);
    }
  }


  bool OrthancPeers::DoGet(MemoryBuffer& target,
                           size_t index,
                           const std::string& uri) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }

    OrthancPluginMemoryBuffer answer;
    uint16_t status;
    OrthancPluginErrorCode code = OrthancPluginCallPeerApi
      (context_, &answer, NULL, &status, peers_, 
       static_cast<uint32_t>(index), OrthancPluginHttpMethod_Get, uri.c_str(),
       0, NULL, NULL, NULL, 0, timeout_);

    if (code == OrthancPluginErrorCode_Success)
    {
      target.Assign(answer);
      return (status == 200);
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancPeers::DoGet(MemoryBuffer& target,
                           const std::string& name,
                           const std::string& uri) const
  {
    size_t index;
    return (LookupName(index, name) &&
            DoGet(target, index, uri));
  }
  

  bool OrthancPeers::DoGet(Json::Value& target,
                           size_t index,
                           const std::string& uri) const
  {
    MemoryBuffer buffer(context_);
      
    if (DoGet(buffer, index, uri))
    {
      buffer.ToJson(target);
      return true;
    }
    else
    {
      return false;
    }
  }
  
      
  bool OrthancPeers::DoGet(Json::Value& target,
                           const std::string& name,
                           const std::string& uri) const
  {
    MemoryBuffer buffer(context_);
      
    if (DoGet(buffer, name, uri))
    {
      buffer.ToJson(target);
      return true;
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancPeers::DoPost(MemoryBuffer& target,
                            const std::string& name,
                            const std::string& uri,
                            const std::string& body) const
  {
    size_t index;
    return (LookupName(index, name) &&
            DoPost(target, index, uri, body));
  }
  

  bool OrthancPeers::DoPost(Json::Value& target,
                            size_t index,
                            const std::string& uri,
                            const std::string& body) const
  {
    MemoryBuffer buffer(context_);
      
    if (DoPost(buffer, index, uri, body))
    {
      buffer.ToJson(target);
      return true;
    }
    else
    {
      return false;
    }
  }
  
      
  bool OrthancPeers::DoPost(Json::Value& target,
                            const std::string& name,
                            const std::string& uri,
                            const std::string& body) const
  {
    MemoryBuffer buffer(context_);
      
    if (DoPost(buffer, name, uri, body))
    {
      buffer.ToJson(target);
      return true;
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancPeers::DoPost(MemoryBuffer& target,
                            size_t index,
                            const std::string& uri,
                            const std::string& body) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }

    OrthancPluginMemoryBuffer answer;
    uint16_t status;
    OrthancPluginErrorCode code = OrthancPluginCallPeerApi
      (context_, &answer, NULL, &status, peers_, 
       static_cast<uint32_t>(index), OrthancPluginHttpMethod_Post, uri.c_str(),
       0, NULL, NULL, body.empty() ? NULL : body.c_str(), body.size(), timeout_);

    if (code == OrthancPluginErrorCode_Success)
    {
      target.Assign(answer);
      return (status == 200);
    }
    else
    {
      return false;
    }
  }
  

  bool OrthancPeers::DoPut(size_t index,
                           const std::string& uri,
                           const std::string& body) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }

    OrthancPluginMemoryBuffer answer;
    uint16_t status;
    OrthancPluginErrorCode code = OrthancPluginCallPeerApi
      (context_, &answer, NULL, &status, peers_, 
       static_cast<uint32_t>(index), OrthancPluginHttpMethod_Put, uri.c_str(),
       0, NULL, NULL, body.empty() ? NULL : body.c_str(), body.size(), timeout_);

    if (code == OrthancPluginErrorCode_Success)
    {
      OrthancPluginFreeMemoryBuffer(context_, &answer);
      return (status == 200);
    }
    else
    {
      return false;
    }
  }
  

  bool OrthancPeers::DoPut(const std::string& name,
                           const std::string& uri,
                           const std::string& body) const
  {
    size_t index;
    return (LookupName(index, name) &&
            DoPut(index, uri, body));
  }
  

  bool OrthancPeers::DoDelete(size_t index,
                              const std::string& uri) const
  {
    if (index >= index_.size())
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }

    OrthancPluginMemoryBuffer answer;
    uint16_t status;
    OrthancPluginErrorCode code = OrthancPluginCallPeerApi
      (context_, &answer, NULL, &status, peers_, 
       static_cast<uint32_t>(index), OrthancPluginHttpMethod_Put, uri.c_str(),
       0, NULL, NULL, NULL, 0, timeout_);

    if (code == OrthancPluginErrorCode_Success)
    {
      OrthancPluginFreeMemoryBuffer(context_, &answer);
      return (status == 200);
    }
    else
    {
      return false;
    }
  }

  
  bool OrthancPeers::DoDelete(const std::string& name,
                              const std::string& uri) const
  {
    size_t index;
    return (LookupName(index, name) &&
            DoDelete(index, uri));
  }
#endif


  
#if HAS_ORTHANC_PLUGIN_JOB == 1
  void OrthancJob::CallbackFinalize(void* job)
  {
    if (job != NULL)
    {
      delete reinterpret_cast<OrthancJob*>(job);
    }
  }
  

  float OrthancJob::CallbackGetProgress(void* job)
  {
    assert(job != NULL);

    try
    {
      return reinterpret_cast<OrthancJob*>(job)->progress_;
    }
    catch (...)
    {
      return 0;
    }
  }
  

  const char* OrthancJob::CallbackGetContent(void* job)
  {
    assert(job != NULL);

    try
    {
      return reinterpret_cast<OrthancJob*>(job)->content_.c_str();
    }
    catch (...)
    {
      return 0;
    }
  }
  

  const char* OrthancJob::CallbackGetSerialized(void* job)
  {
    assert(job != NULL);

    try
    {
      const OrthancJob& tmp = *reinterpret_cast<OrthancJob*>(job);

      if (tmp.hasSerialized_)
      {
        return tmp.serialized_.c_str();
      }
      else
      {
        return NULL;
      }
    }
    catch (...)
    {
      return 0;
    }
  }
  

  OrthancPluginJobStepStatus OrthancJob::CallbackStep(void* job)
  {
    assert(job != NULL);

    try
    {
      return reinterpret_cast<OrthancJob*>(job)->Step();
    }
    catch (ORTHANC_PLUGINS_EXCEPTION_CLASS&)
    {
      return OrthancPluginJobStepStatus_Failure;
    }
    catch (...)
    {
      return OrthancPluginJobStepStatus_Failure;
    }
  }

  
  OrthancPluginErrorCode OrthancJob::CallbackStop(void* job,
                                                  OrthancPluginJobStopReason reason)
  {
    assert(job != NULL);

    try
    {
      reinterpret_cast<OrthancJob*>(job)->Stop(reason);
      return OrthancPluginErrorCode_Success;
    }
    catch (ORTHANC_PLUGINS_EXCEPTION_CLASS& e)
    {
      return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
    }
    catch (...)
    {
      return OrthancPluginErrorCode_Plugin;
    }
  }
  

  OrthancPluginErrorCode OrthancJob::CallbackReset(void* job)
  {
    assert(job != NULL);

    try
    {
      reinterpret_cast<OrthancJob*>(job)->Reset();
      return OrthancPluginErrorCode_Success;
    }
    catch (ORTHANC_PLUGINS_EXCEPTION_CLASS& e)
    {
      return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
    }
    catch (...)
    {
      return OrthancPluginErrorCode_Plugin;
    }
  }
  

  void OrthancJob::ClearContent()
  {
    Json::Value empty = Json::objectValue;
    UpdateContent(empty);
  }

  
  void OrthancJob::UpdateContent(const Json::Value& content)
  {
    if (content.type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_BadFileFormat);
    }
    else
    {
      Json::FastWriter writer;
      content_ = writer.write(content);
    }
  }
  

  void OrthancJob::ClearSerialized()
  {
    hasSerialized_ = false;
    serialized_.clear();
  }

  
  void OrthancJob::UpdateSerialized(const Json::Value& serialized)
  {
    if (serialized.type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_BadFileFormat);
    }
    else
    {
      Json::FastWriter writer;
      serialized_ = writer.write(serialized);
      hasSerialized_ = true;
    }
  }

  
  void OrthancJob::UpdateProgress(float progress)
  {
    if (progress < 0 ||
        progress > 1)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_ParameterOutOfRange);
    }
      
    progress_ = progress;
  }
    

  OrthancJob::OrthancJob(const std::string& jobType) :
    jobType_(jobType),
    progress_(0)
  {
    ClearContent();
    ClearSerialized();
  }

    
  OrthancPluginJob* OrthancJob::Create(OrthancPluginContext* context,
                                       OrthancJob* job)
  {
    if (job == NULL)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_NullPointer);
    }

    OrthancPluginJob* orthanc = OrthancPluginCreateJob(
      context, job, CallbackFinalize, job->jobType_.c_str(),
      CallbackGetProgress, CallbackGetContent, CallbackGetSerialized,
      CallbackStep, CallbackStop, CallbackReset);

    if (orthanc == NULL)
    {
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
    }
    else
    {
      return orthanc;
    }
  }

  
  std::string OrthancJob::Submit(OrthancPluginContext* context,
                                 OrthancJob* job,
                                 int priority)
  {
    OrthancPluginJob* orthanc = Create(context, job);
    
    char* id = OrthancPluginSubmitJob(context, orthanc, priority);

    if (id == NULL)
    {
      OrthancPluginLogError(context, "Plugin cannot submit job");
      OrthancPluginFreeJob(context, orthanc);
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(OrthancPluginErrorCode_Plugin);
    }
    else
    {
      std::string tmp(id);
      tmp.assign(id);
      OrthancPluginFreeString(context, id);

      return tmp;
    }
  }
#endif
}
