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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include <orthanc/OrthancCPlugin.h>

#include <string>

static OrthancPluginContext* context_ = NULL;


static bool ReadFile(std::string& result,
                     const std::string& path)
{
  OrthancPluginMemoryBuffer tmp;
  if (OrthancPluginReadFile(context_, &tmp, path.c_str()) == OrthancPluginErrorCode_Success)
  {
    result.assign(reinterpret_cast<const char*>(tmp.data), tmp.size);
    OrthancPluginFreeMemoryBuffer(context_, &tmp);
    return true;
  }
  else
  {
    return false;
  }
}


OrthancPluginErrorCode OnStoredCallback(OrthancPluginDicomInstance* instance,
                                        const char* instanceId)
{
  char buffer[1024];
  sprintf(buffer, "Just received a DICOM instance of size %d and ID %s from origin %d (AET %s)", 
          (int) OrthancPluginGetInstanceSize(context_, instance), instanceId, 
          OrthancPluginGetInstanceOrigin(context_, instance),
          OrthancPluginGetInstanceRemoteAet(context_, instance));
  OrthancPluginLogInfo(context_, buffer);

  if (OrthancPluginGetInstanceOrigin(context_, instance) == OrthancPluginInstanceOrigin_Plugin)
  {
    // Do not compress twice the same file
    return OrthancPluginErrorCode_Success;
  }

  // Write the uncompressed DICOM content to some temporary file
  std::string uncompressed = "uncompressed-" + std::string(instanceId) + ".dcm";
  OrthancPluginErrorCode error = OrthancPluginWriteFile(context_, uncompressed.c_str(), 
                                                        OrthancPluginGetInstanceData(context_, instance),
                                                        OrthancPluginGetInstanceSize(context_, instance));
  if (error)
  {
    return error;
  }

  // Remove the original DICOM instance
  std::string uri = "/instances/" + std::string(instanceId);
  error = OrthancPluginRestApiDelete(context_, uri.c_str());
  if (error)
  {
    return error;
  }

  // Path to the temporary file that will contain the compressed DICOM content
  std::string compressed = "compressed-" + std::string(instanceId) + ".dcm";

  // Compress to JPEG2000 using gdcm
  std::string command1 = "gdcmconv --j2k " + uncompressed + " " + compressed;

  // Generate a new SOPInstanceUID for the JPEG2000 file, as gdcmconv
  // does not do this by itself
  std::string command2 = "dcmodify --no-backup -gin " + compressed;

  // Make the required system calls
  system(command1.c_str());
  system(command2.c_str());

  // Read the result of the JPEG2000 compression
  std::string j2k;
  bool ok = ReadFile(j2k, compressed);

  // Remove the two temporary files
  remove(compressed.c_str());
  remove(uncompressed.c_str());

  if (!ok)
  {
    return OrthancPluginErrorCode_Plugin;
  }

  // Upload the JPEG2000 file through the REST API
  OrthancPluginMemoryBuffer tmp;
  if (OrthancPluginRestApiPost(context_, &tmp, "/instances", j2k.c_str(), j2k.size()))
  {
    ok = false;
  }

  if (ok)
  {
    OrthancPluginFreeMemoryBuffer(context_, &tmp);
  }

  return ok ? OrthancPluginErrorCode_Success : OrthancPluginErrorCode_Plugin;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context_ = c;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    OrthancPluginRegisterOnStoredInstanceCallback(context_, OnStoredCallback);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "sample-jpeg2k";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.0";
  }
}
