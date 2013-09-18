/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "../../../../Core/HttpClient.h"
#include "../../../../OrthancCppClient/OrthancConnection.h"

int main()
{
  // Prepare a simple call to a Web service
  Orthanc::HttpClient c;
  c.SetUrl("http://nominatim.openstreetmap.org/search?format=json&q=chu+liege+belgium");
  
  // Do the request and store the result in a JSON structure
  Json::Value result;
  c.Apply(result);

  // Display the JSON answer
  std::cout << result << std::endl;

  // Display the content of the local Orthanc instance
  OrthancClient::OrthancConnection orthanc("http://localhost:8042");

  for (unsigned int i = 0; i < orthanc.GetPatientCount(); i++)
  {
    OrthancClient::Patient& patient = orthanc.GetPatient(i);
    std::cout << "Patient: " << patient.GetId() << std::endl;

    for (unsigned int j = 0; j < patient.GetStudyCount(); j++)
    {
      OrthancClient::Study& study = patient.GetStudy(j);
      std::cout << "  Study: " << study.GetId() << std::endl;

      for (unsigned int k = 0; k < study.GetSeriesCount(); k++)
      {
        OrthancClient::Series& series = study.GetSeries(k);
        std::cout << "    Series: " << series.GetId() << std::endl;

        for (unsigned int l = 0; l < series.GetInstanceCount(); l++)
        {
          std::cout << "      Instance: " << series.GetInstance(l).GetId() << std::endl;
        }
      }
    }
  }

  return 0;
}
