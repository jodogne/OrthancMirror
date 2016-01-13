/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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


#include "PrecompiledHeadersServer.h"
#include "OrthancMoveRequestHandler.h"

#include "OrthancInitialization.h"
#include "FromDcmtkBridge.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Logging.h"

namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules

    class OrthancMoveRequestIterator : public IMoveRequestIterator
    {
    private:
      ServerContext& context_;
      const std::string& localAet_;
      std::vector<std::string> instances_;
      size_t position_;
      RemoteModalityParameters remote_;
      uint16_t moveRequestID_;

    public:
      OrthancMoveRequestIterator(ServerContext& context,
                                 const std::string& aet,
                                 const std::string& publicId,
                                 uint16_t moveRequestID) :
        context_(context),
        localAet_(context.GetDefaultLocalApplicationEntityTitle()),
        position_(0),
        moveRequestID_(moveRequestID)
      {
        LOG(INFO) << "Sending resource " << publicId << " to modality \"" << aet << "\"";

        std::list<std::string> tmp;
        context_.GetIndex().GetChildInstances(tmp, publicId);

        instances_.reserve(tmp.size());
        for (std::list<std::string>::iterator it = tmp.begin(); it != tmp.end(); ++it)
        {
          instances_.push_back(*it);
        }

        remote_ = Configuration::GetModalityUsingAet(aet);
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
        context_.ReadFile(dicom, id, FileContentType_Dicom);

        {
          ReusableDicomUserConnection::Locker locker
            (context_.GetReusableDicomUserConnection(), localAet_, remote_);
          locker.GetConnection().Store(dicom, moveRequestID_);
        }

        return Status_Success;
      }
    };
  }


  bool OrthancMoveRequestHandler::LookupIdentifier(std::string& publicId,
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

    const std::string& content = value.GetContent();

    std::list<std::string> ids;
    context_.GetIndex().LookupIdentifierExact(ids, level, tag, content);

    if (ids.size() != 1)
    {
      return false;
    }
    else
    {
      publicId = ids.front();
      return true;
    }
  }


  IMoveRequestIterator* OrthancMoveRequestHandler::Handle(const std::string& targetAet,
                                                          const DicomMap& input,
                                                          const std::string& remoteIp,
                                                          const std::string& remoteAet,
                                                          const std::string& calledAet,
                                                          uint16_t messageId)
  {
    LOG(WARNING) << "Move-SCU request received for AET \"" << targetAet << "\"";

    {
      DicomArray query(input);
      for (size_t i = 0; i < query.GetSize(); i++)
      {
        if (!query.GetElement(i).GetValue().IsNull())
        {
          LOG(INFO) << "  " << query.GetElement(i).GetTag()
                    << "  " << FromDcmtkBridge::GetName(query.GetElement(i).GetTag())
                    << " = " << query.GetElement(i).GetValue().GetContent();
        }
      }
    }


#if 0
    /**
     * Retrieve the Message ID (0000,0110) for this C-MOVE request, if
     * any. If present, this Message ID will be stored in the Move
     * Originator Message ID (0000,1031) field of the C-MOVE response.
     * http://dicom.nema.org/medical/dicom/current/output/html/part07.html#sect_9.3.1
     **/

    static const DicomTag MESSAGE_ID(0x0000, 0x0110);
    const DicomValue* messageIdTmp = input.TestAndGetValue(MESSAGE_ID);

    messageId = 0;

    if (messageIdTmp != NULL &&
        !messageIdTmp->IsNull() &&
        !messageIdTmp->IsBinary())
    {
      try
      {
        messageId = boost::lexical_cast<uint16_t>(messageIdTmp->GetContent());
      }
      catch (boost::bad_lexical_cast&)
      {
        LOG(WARNING) << "Cannot convert the Message ID (\"" << messageIdTmp ->GetContent()
                     << "\") of an incoming C-MOVE request to an integer, assuming zero";
      }
    }
#endif


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

      std::string publicId;

      if (LookupIdentifier(publicId, ResourceType_Instance, input) ||
          LookupIdentifier(publicId, ResourceType_Series, input) ||
          LookupIdentifier(publicId, ResourceType_Study, input) ||
          LookupIdentifier(publicId, ResourceType_Patient, input))
      {
        return new OrthancMoveRequestIterator(context_, targetAet, publicId, messageId);
      }
      else
      {
        // No identifier is present in the request.
        throw OrthancException(ErrorCode_BadRequest);
      }
    }

    assert(levelTmp != NULL);
    ResourceType level = StringToResourceType(levelTmp->GetContent().c_str());      


    /**
     * Lookup for the resource to be sent.
     **/

    std::string publicId;

    if (LookupIdentifier(publicId, level, input))
    {
      return new OrthancMoveRequestIterator(context_, targetAet, publicId, messageId);
    }
    else
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
  }
}
