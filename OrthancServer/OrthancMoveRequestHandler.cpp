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


#include "PrecompiledHeadersServer.h"
#include "OrthancMoveRequestHandler.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Logging.h"
#include "../Core/MetricsRegistry.h"
#include "OrthancConfiguration.h"
#include "ServerContext.h"
#include "ServerJobs/DicomModalityStoreJob.h"


namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules

    class SynchronousMove : public IMoveRequestIterator
    {
    private:
      ServerContext& context_;
      const std::string& localAet_;
      std::vector<std::string> instances_;
      size_t position_;
      RemoteModalityParameters remote_;
      std::string originatorAet_;
      uint16_t originatorId_;
      std::unique_ptr<DicomUserConnection> connection_;

    public:
      SynchronousMove(ServerContext& context,
                      const std::string& targetAet,
                      const std::vector<std::string>& publicIds,
                      const std::string& originatorAet,
                      uint16_t originatorId) :
        context_(context),
        localAet_(context.GetDefaultLocalApplicationEntityTitle()),
        position_(0),
        originatorAet_(originatorAet),
        originatorId_(originatorId)
      {
        {
          OrthancConfiguration::ReaderLock lock;
          remote_ = lock.GetConfiguration().GetModalityUsingAet(targetAet);
        }

        for (size_t i = 0; i < publicIds.size(); i++)
        {
          LOG(INFO) << "Sending resource " << publicIds[i] << " to modality \""
                    << targetAet << "\" in synchronous mode";

          std::list<std::string> tmp;
          context_.GetIndex().GetChildInstances(tmp, publicIds[i]);

          instances_.reserve(tmp.size());
          for (std::list<std::string>::iterator it = tmp.begin(); it != tmp.end(); ++it)
          {
            instances_.push_back(*it);
          }
        }
      }

      virtual unsigned int GetSubOperationCount() const
      {
        return instances_.size();
      }

      virtual Status DoNext()
      {
        if (position_ >= instances_.size())
        {
          return Status_Failure;
        }

        const std::string& id = instances_[position_++];

        std::string dicom;
        context_.ReadDicom(dicom, id);

        if (connection_.get() == NULL)
        {
          connection_.reset(new DicomUserConnection(localAet_, remote_));
        }

        std::string sopClassUid, sopInstanceUid;  // Unused
        connection_->Store(sopClassUid, sopInstanceUid, dicom, originatorAet_, originatorId_);

        return Status_Success;
      }
    };


    class AsynchronousMove : public IMoveRequestIterator
    {
    private:
      ServerContext&                          context_;
      std::unique_ptr<DicomModalityStoreJob>  job_;
      size_t                                  position_;
      size_t                                  countInstances_;
      
    public:
      AsynchronousMove(ServerContext& context,
                       const std::string& targetAet,
                       const std::vector<std::string>& publicIds,
                       const std::string& originatorAet,
                       uint16_t originatorId) :
        context_(context),
        job_(new DicomModalityStoreJob(context)),
        position_(0)
      {
        job_->SetDescription("C-MOVE");
        //job_->SetPermissive(true);  // This was the behavior of Orthanc < 1.6.0
        job_->SetPermissive(false);
        job_->SetLocalAet(context.GetDefaultLocalApplicationEntityTitle());

        {
          OrthancConfiguration::ReaderLock lock;
          job_->SetRemoteModality(lock.GetConfiguration().GetModalityUsingAet(targetAet));
        }

        if (originatorId != 0)
        {
          job_->SetMoveOriginator(originatorAet, originatorId);
        }

        for (size_t i = 0; i < publicIds.size(); i++)
        {
          LOG(INFO) << "Sending resource " << publicIds[i] << " to modality \""
                    << targetAet << "\" in asynchronous mode";

          std::list<std::string> tmp;
          context_.GetIndex().GetChildInstances(tmp, publicIds[i]);

          countInstances_ = tmp.size();

          job_->Reserve(job_->GetCommandsCount() + tmp.size());

          for (std::list<std::string>::iterator it = tmp.begin(); it != tmp.end(); ++it)
          {
            job_->AddInstance(*it);
          }
        }
      }

      virtual unsigned int GetSubOperationCount() const
      {
        return countInstances_;
      }

      virtual Status DoNext()
      {
        if (position_ >= countInstances_)
        {
          return Status_Failure;
        }
        
        if (position_ == 0)
        {
          context_.GetJobsEngine().GetRegistry().Submit(job_.release(), 0 /* priority */);
        }
        
        position_ ++;
        return Status_Success;
      }
    };
  }


  bool OrthancMoveRequestHandler::LookupIdentifiers(std::vector<std::string>& publicIds,
                                                    ResourceType level,
                                                    const DicomMap& input)
  {
    DicomTag tag(0, 0);   // Dummy initialization

    switch (level)
    {
      case ResourceType_Patient:
        tag = DICOM_TAG_PATIENT_ID;
        break;

      case ResourceType_Study:
        tag = (input.HasTag(DICOM_TAG_ACCESSION_NUMBER) ? 
               DICOM_TAG_ACCESSION_NUMBER : DICOM_TAG_STUDY_INSTANCE_UID);
        break;
        
      case ResourceType_Series:
        tag = DICOM_TAG_SERIES_INSTANCE_UID;
        break;
        
      case ResourceType_Instance:
        tag = DICOM_TAG_SOP_INSTANCE_UID;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (!input.HasTag(tag))
    {
      return false;
    }

    const DicomValue& value = input.GetValue(tag);
    if (value.IsNull() ||
        value.IsBinary())
    {
      return false;
    }
    else
    {
      const std::string& content = value.GetContent();

      /**
       * This tokenization fixes issue 154 ("Matching against list of
       * UID-s by C-MOVE").
       * https://bitbucket.org/sjodogne/orthanc/issues/154/
       **/

      std::vector<std::string> tokens;
      Toolbox::TokenizeString(tokens, content, '\\');
      for (size_t i = 0; i < tokens.size(); i++)
      {
        std::vector<std::string> matches;
        context_.GetIndex().LookupIdentifierExact(matches, level, tag, tokens[i]);

        // Concatenate "publicIds" with "matches"
        publicIds.insert(publicIds.end(), matches.begin(), matches.end());
      }

      return true;
    }
  }


  static IMoveRequestIterator* CreateIterator(ServerContext& context,
                                              const std::string& targetAet,
                                              const std::vector<std::string>& publicIds,
                                              const std::string& originatorAet,
                                              uint16_t originatorId)
  {
    if (publicIds.empty())
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "C-MOVE request matching no resource stored in Orthanc");
    }
    
    bool synchronous;

    {
      OrthancConfiguration::ReaderLock lock;
      synchronous = lock.GetConfiguration().GetBooleanParameter("SynchronousCMove", true);
    }

    if (synchronous)
    {
      return new SynchronousMove(context, targetAet, publicIds, originatorAet, originatorId);
    }
    else
    {
      return new AsynchronousMove(context, targetAet, publicIds, originatorAet, originatorId);
    }
  }


  IMoveRequestIterator* OrthancMoveRequestHandler::Handle(const std::string& targetAet,
                                                          const DicomMap& input,
                                                          const std::string& originatorIp,
                                                          const std::string& originatorAet,
                                                          const std::string& calledAet,
                                                          uint16_t originatorId)
  {
    MetricsRegistry::Timer timer(context_.GetMetricsRegistry(), "orthanc_move_scp_duration_ms");

    LOG(WARNING) << "Move-SCU request received for AET \"" << targetAet << "\"";

    {
      DicomArray query(input);
      for (size_t i = 0; i < query.GetSize(); i++)
      {
        if (!query.GetElement(i).GetValue().IsNull())
        {
          LOG(INFO) << "  " << query.GetElement(i).GetTag()
                    << "  " << FromDcmtkBridge::GetTagName(query.GetElement(i))
                    << " = " << query.GetElement(i).GetValue().GetContent();
        }
      }
    }

    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);

    if (levelTmp == NULL ||
        levelTmp->IsNull() ||
        levelTmp->IsBinary())
    {
      // The query level is not present in the C-Move request, which
      // does not follow the DICOM standard. This is for instance the
      // behavior of Tudor DICOM. Try and automatically deduce the
      // query level: Start from the instance level, going up to the
      // patient level until a valid DICOM identifier is found.

      std::vector<std::string> publicIds;

      if (LookupIdentifiers(publicIds, ResourceType_Instance, input) ||
          LookupIdentifiers(publicIds, ResourceType_Series, input) ||
          LookupIdentifiers(publicIds, ResourceType_Study, input) ||
          LookupIdentifiers(publicIds, ResourceType_Patient, input))
      {
        return CreateIterator(context_, targetAet, publicIds, originatorAet, originatorId);
      }
      else
      {
        // No identifier is present in the request.
        throw OrthancException(ErrorCode_BadRequest, "Invalid fields in a C-MOVE request");
      }
    }

    assert(levelTmp != NULL);
    ResourceType level = StringToResourceType(levelTmp->GetContent().c_str());      


    /**
     * Lookup for the resource to be sent.
     **/

    std::vector<std::string> publicIds;

    if (LookupIdentifiers(publicIds, level, input))
    {
      return CreateIterator(context_, targetAet, publicIds, originatorAet, originatorId);
    }
    else
    {
      throw OrthancException(ErrorCode_BadRequest, "Invalid fields in a C-MOVE request");
    }
  }
}
