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

    public:
      OrthancMoveRequestIterator(ServerContext& context,
                                 const std::string& aet,
                                 const std::string& publicId) :
        context_(context),
        localAet_(context.GetDefaultLocalApplicationEntityTitle()),
        position_(0)
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
          locker.GetConnection().Store(dicom);
        }

        return Status_Success;
      }
    };
  }


  bool OrthancMoveRequestHandler::LookupIdentifier(std::string& publicId,
                                                   DicomTag tag,
                                                   const DicomMap& input)
  {
    if (!input.HasTag(tag))
    {
      return false;
    }

    std::string value = input.GetValue(tag).AsString();

    std::list<std::string> ids;
    context_.GetIndex().LookupIdentifier(ids, tag, value);

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


  IMoveRequestIterator* OrthancMoveRequestHandler::Handle(const std::string& aet,
                                                          const DicomMap& input)
  {
    LOG(WARNING) << "Move-SCU request received for AET \"" << aet << "\"";

    {
      DicomArray query(input);
      for (size_t i = 0; i < query.GetSize(); i++)
      {
        if (!query.GetElement(i).GetValue().IsNull())
        {
          LOG(INFO) << "  " << query.GetElement(i).GetTag()
                    << "  " << FromDcmtkBridge::GetName(query.GetElement(i).GetTag())
                    << " = " << query.GetElement(i).GetValue().AsString();
        }
      }
    }



    /**
     * Retrieve the query level.
     **/

    ResourceType level;
    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);

    if (levelTmp != NULL) 
    {
      level = StringToResourceType(levelTmp->AsString().c_str());
    }
    else
    {
      // The query level is not present in the C-Move request, which
      // does not follow the DICOM standard. This is for instance the
      // behavior of Tudor DICOM. Try and automatically deduce the
      // query level: Start from the instance level, going up to the
      // patient level until a valid DICOM identifier is found.

      std::string publicId;

      if (LookupIdentifier(publicId, DICOM_TAG_SOP_INSTANCE_UID, input) ||
          LookupIdentifier(publicId, DICOM_TAG_SERIES_INSTANCE_UID, input) ||
          LookupIdentifier(publicId, DICOM_TAG_STUDY_INSTANCE_UID, input) ||
          LookupIdentifier(publicId, DICOM_TAG_PATIENT_ID, input))
      {
        return new OrthancMoveRequestIterator(context_, aet, publicId);
      }
      else
      {
        // No identifier is present in the request.
        throw OrthancException(ErrorCode_BadRequest);
      }
    }



    /**
     * Lookup for the resource to be sent.
     **/

    bool ok;
    std::string publicId;

    switch (level)
    {
      case ResourceType_Patient:
        ok = LookupIdentifier(publicId, DICOM_TAG_PATIENT_ID, input);
        break;

      case ResourceType_Study:
        ok = LookupIdentifier(publicId, DICOM_TAG_STUDY_INSTANCE_UID, input);
        break;

      case ResourceType_Series:
        ok = LookupIdentifier(publicId, DICOM_TAG_SERIES_INSTANCE_UID, input);
        break;

      case ResourceType_Instance:
        ok = LookupIdentifier(publicId, DICOM_TAG_SOP_INSTANCE_UID, input);
        break;

      default:
        ok = false;
    }

    if (!ok)
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    return new OrthancMoveRequestIterator(context_, aet, publicId);
  }
}
