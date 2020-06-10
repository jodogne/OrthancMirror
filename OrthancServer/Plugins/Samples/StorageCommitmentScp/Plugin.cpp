/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../Common/OrthancPluginCppWrapper.h"

#include <json/value.h>
#include <json/reader.h>



class StorageCommitmentSample : public OrthancPlugins::IStorageCommitmentScpHandler
{
private:
  int count_;
  
public:
  StorageCommitmentSample() : count_(0)
  {
  }
  
  virtual OrthancPluginStorageCommitmentFailureReason Lookup(const std::string& sopClassUid,
                                                             const std::string& sopInstanceUid)
  {
    printf("?? [%s] [%s]\n", sopClassUid.c_str(), sopInstanceUid.c_str());
    if (count_++ % 2 == 0)
      return OrthancPluginStorageCommitmentFailureReason_Success;
    else
      return OrthancPluginStorageCommitmentFailureReason_NoSuchObjectInstance;
  }
};


static OrthancPluginErrorCode StorageCommitmentScp(void**              handler /* out */,
                                                   const char*         jobId,
                                                   const char*         transactionUid,
                                                   const char* const*  sopClassUids,
                                                   const char* const*  sopInstanceUids,
                                                   uint32_t            countInstances,
                                                   const char*         remoteAet,
                                                   const char*         calledAet)
{
  /*std::string s;
    OrthancPlugins::RestApiPost(s, "/jobs/" + std::string(jobId) + "/pause", NULL, 0, false);*/
  
  printf("[%s] [%s] [%s] [%s]\n", jobId, transactionUid, remoteAet, calledAet);

  for (uint32_t i = 0; i < countInstances; i++)
  {
    printf("++ [%s] [%s]\n", sopClassUids[i], sopInstanceUids[i]);
  }

  *handler = new StorageCommitmentSample;
  return OrthancPluginErrorCode_Success;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPluginSetDescription(c, "Sample storage commitment SCP plugin.");

    OrthancPluginRegisterStorageCommitmentScpCallback(
      c, StorageCommitmentScp,
      OrthancPlugins::IStorageCommitmentScpHandler::Destructor,
      OrthancPlugins::IStorageCommitmentScpHandler::Lookup);
    
    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "storage-commitment-scp";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return PLUGIN_VERSION;
  }
}
