/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "OrthancFindRequestHandler.h"

#include <glog/logging.h>

#include "../Core/DicomFormat/DicomArray.h"

namespace Orthanc
{
  void OrthancFindRequestHandler::Handle(const DicomMap& input,
                                         DicomFindAnswers& answers)
  {
    LOG(WARNING) << "Find-SCU request received";

    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    if (levelTmp == NULL) 
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    ResourceType level = StringToResourceType(levelTmp->AsString().c_str());

    if (level != ResourceType_Patient &&
        level != ResourceType_Study &&
        level != ResourceType_Series)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }


    /**
     * Retrieve the constraints of the query.
     **/

    DicomArray query(input);

    DicomMap constraintsTmp;
    DicomMap wildcardConstraintsTmp;

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (!query.GetElement(i).GetValue().IsNull() &&
          query.GetElement(i).GetTag() != DICOM_TAG_QUERY_RETRIEVE_LEVEL &&
          query.GetElement(i).GetTag() != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        DicomTag tag = query.GetElement(i).GetTag();
        std::string value = query.GetElement(i).GetValue().AsString();

        if (value.find('*') != std::string::npos ||
            value.find('?') != std::string::npos ||
            value.find('\\') != std::string::npos ||
            value.find('-') != std::string::npos)
        {
          wildcardConstraintsTmp.SetValue(tag, value);
        }
        else
        {
          constraintsTmp.SetValue(tag, value);
        }
      }
    }

    DicomArray constraints(constraintsTmp);
    DicomArray wildcardConstraints(wildcardConstraintsTmp);

    // http://www.itk.org/Wiki/DICOM_QueryRetrieve_Explained
    // http://dicomiseasy.blogspot.be/2012/01/dicom-queryretrieve-part-i.html

    constraints.Print(stdout);
    printf("\n"); fflush(stdout);
    wildcardConstraints.Print(stdout);
    printf("\n"); fflush(stdout);
  }
}
