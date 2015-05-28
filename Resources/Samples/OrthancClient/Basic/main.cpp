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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include <iostream>
#include <orthanc/OrthancCppClient.h>

int main()
{
  try
  {
    // The following explicit initialization is not required, except
    // if you wish to specify the full path to the shared library
    OrthancClient::Initialize();

    // Display the content of the local Orthanc instance
    OrthancClient::OrthancConnection orthanc("http://localhost:8042");

    for (unsigned int i = 0; i < orthanc.GetPatientCount(); i++)
    {
      OrthancClient::Patient patient(orthanc.GetPatient(i));
      std::cout << "Patient: " << patient.GetId() << std::endl;

      for (unsigned int j = 0; j < patient.GetStudyCount(); j++)
      {
        OrthancClient::Study study(patient.GetStudy(j));
        std::cout << "  Study: " << study.GetId() << std::endl;

        for (unsigned int k = 0; k < study.GetSeriesCount(); k++)
        {
          OrthancClient::Series series(study.GetSeries(k));
          std::cout << "    Series: " << series.GetId() << std::endl;

          if (series.Is3DImage())
          {
            std::cout << "    This is a 3D image whose voxel size is " 
                      << series.GetVoxelSizeX() << " x " 
                      << series.GetVoxelSizeY() << " x " 
                      << series.GetVoxelSizeZ() << ", and slice thickness is " 
                      << series.GetSliceThickness() << std::endl;
          }

          for (unsigned int l = 0; l < series.GetInstanceCount(); l++)
          {
            std::cout << "      Instance: " << series.GetInstance(l).GetId() << std::endl;

            // Load and display some raw DICOM tag
            series.GetInstance(l).LoadTagContent("0020-000d");
            std::cout << "        SOP instance UID: " << series.GetInstance(l).GetLoadedTagContent() << std::endl;
          }
        }
      }
    }

    OrthancClient::Finalize();

    return 0;
  }
  catch (OrthancClient::OrthancClientException& e)
  {
    std::cerr << "EXCEPTION: [" << e.What() << "]" << std::endl;
    return -1;
  }
}
