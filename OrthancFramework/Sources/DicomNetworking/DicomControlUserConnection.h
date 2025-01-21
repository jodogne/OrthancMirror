/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#if ORTHANC_ENABLE_DCMTK_NETWORKING != 1
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be set to 1
#endif

#include "DicomAssociationParameters.h"
#include "DicomFindAnswers.h"

#include <boost/noncopyable.hpp>
#include <list>

namespace Orthanc
{
  class DicomAssociation;  // Forward declaration for PImpl design pattern
  
  typedef uint16_t (*CGetInstanceReceivedCallback) (void *callbackContext,
                                                    DcmDataset& dataset,
                                                    const std::string& remoteAet,
                                                    const std::string& remoteIp,
                                                    const std::string& calledAet);


  enum ScuOperationFlags
  {
    ScuOperationFlags_Echo = 1 << 0,
    ScuOperationFlags_FindPatient = 1 << 1,
    ScuOperationFlags_FindStudy = 1 << 2,
    ScuOperationFlags_FindWorklist = 1 << 3,
    ScuOperationFlags_MoveStudy = 1 << 4,
    ScuOperationFlags_MovePatient = 1 << 5,
    // C-Store is not using DicomControlUserConnection but DicomStoreUserConnection
    ScuOperationFlags_Get = 1 << 6,

    ScuOperationFlags_Find = ScuOperationFlags_FindPatient | ScuOperationFlags_FindStudy | ScuOperationFlags_FindWorklist,
    ScuOperationFlags_Move = ScuOperationFlags_MoveStudy | ScuOperationFlags_MovePatient,
    ScuOperationFlags_All = ScuOperationFlags_Echo | ScuOperationFlags_Find | ScuOperationFlags_Move | ScuOperationFlags_Get
  };

  class DicomControlUserConnection : public boost::noncopyable
  {
  public:
    class IProgressListener
    {
    public:
      virtual void OnProgressUpdated(uint16_t nbRemainingSubOperations,
                                     uint16_t nbCompletedSubOperations,
                                     uint16_t nbFailedSubOperations,
                                     uint16_t nbWarningSubOperations) = 0;
    };

  private:
    DicomAssociationParameters           parameters_;
    boost::shared_ptr<DicomAssociation>  association_;
    IProgressListener*                   progressListener_;

    void SetupPresentationContexts(ScuOperationFlags scuOperation,
                                   const std::set<std::string>& acceptedStorageSopClasses,
                                   const std::list<DicomTransferSyntax>& proposedStorageTransferSyntaxes);

    void FindInternal(DicomFindAnswers& answers,
                      DcmDataset* dataset,
                      const char* sopClass,
                      bool isWorklist,
                      const char* level);
    
    void MoveInternal(const std::string& targetAet,
                      ResourceType level,
                      const DicomMap& fields);
    
  public:
    explicit DicomControlUserConnection(const DicomAssociationParameters& params, ScuOperationFlags scuOperation);

    // specific constructor for CGet SCU
    explicit DicomControlUserConnection(const DicomAssociationParameters& params, 
                                        ScuOperationFlags scuOperation,
                                        const std::set<std::string>& acceptedStorageSopClasses,
                                        const std::list<DicomTransferSyntax>& proposedStorageTransferSyntaxes);

    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }

    void Close();

    bool Echo();

    void SetProgressListener(IProgressListener* progressListener)
    {
      progressListener_ = progressListener;
    }

    void Find(DicomFindAnswers& result,
              ResourceType level,
              const DicomMap& originalFields,
              bool normalize);

    void Get(const DicomMap& getQuery,
             CGetInstanceReceivedCallback instanceReceivedCallback,
             void* callbackContext);

    void Move(const std::string& targetAet,
              ResourceType level,
              const DicomMap& findResult);
    
    void Move(const std::string& targetAet,
              const DicomMap& findResult);
    
    void MovePatient(const std::string& targetAet,
                     const std::string& patientId);

    void MoveStudy(const std::string& targetAet,
                   const std::string& studyUid);

    void MoveSeries(const std::string& targetAet,
                    const std::string& studyUid,
                    const std::string& seriesUid);

    void MoveInstance(const std::string& targetAet,
                      const std::string& studyUid,
                      const std::string& seriesUid,
                      const std::string& instanceUid);

    void FindWorklist(DicomFindAnswers& result,
                      ParsedDicomFile& query);
  };
}
