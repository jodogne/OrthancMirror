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
    DicomControlUserConnection(const DicomAssociationParameters& params);
    
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

    void SetTimeout(uint32_t seconds); // 0 = no timeout
  };
}
