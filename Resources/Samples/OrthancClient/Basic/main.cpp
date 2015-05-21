/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
