/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

namespace Orthanc
{
  class DicomAssociation;  // Forward declaration for PImpl design pattern
  
  class DicomControlUserConnection : public boost::noncopyable
  {
  private:
    DicomAssociationParameters           parameters_;
    boost::shared_ptr<DicomAssociation>  association_;

    void SetupPresentationContexts();

    void FindInternal(DicomFindAnswers& answers,
                      DcmDataset* dataset,
                      const char* sopClass,
                      bool isWorklist,
                      const char* level);
    
    void MoveInternal(const std::string& targetAet,
                      ResourceType level,
                      const DicomMap& fields);
    
  public:
    explicit DicomControlUserConnection(const DicomAssociationParameters& params);
    
    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }

    void Close();

    bool Echo();

    void Find(DicomFindAnswers& result,
              ResourceType level,
              const DicomMap& originalFields,
              bool normalize);

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
