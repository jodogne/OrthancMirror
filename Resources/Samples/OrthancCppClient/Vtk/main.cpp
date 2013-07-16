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

#include <vtkRenderWindow.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkFixedPointVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkOpenGLRenderer.h>
#include <vtkInteractorStyleTrackballCamera.h>

#include "../../../../OrthancCppClient/OrthancConnection.h"


class DisplayProgress : public Orthanc::ThreadedCommandProcessor::IListener
{
public:
  virtual void SignalProgress(unsigned int current,
                              unsigned int total)
  {
    std::cout << "Slice loaded (" << current << "/" << total << ")" << std::endl;
  }

  virtual void SignalSuccess(unsigned int total)
  {
    std::cout << "Success loading image (" << total << " images)" << std::endl;
  }

  virtual void SignalCancel()
  {
  }

  virtual void SignalFailure()
  {
    std::cout << "Error loading image" << std::endl;
  }
};



void Display(OrthancClient::Series& series)
{
  /**
   * Load the 3D image from Orthanc into VTK.
   **/

  vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(series.GetWidth(), series.GetHeight(), series.GetInstanceCount());
  image->SetScalarType(VTK_SHORT);
  image->AllocateScalars();

  if (series.GetWidth() != 0 &&
      series.GetHeight() != 0 && 
      series.GetInstanceCount() != 0)
  {
    DisplayProgress listener;
    series.Load3DImage(image->GetScalarPointer(0, 0, 0), Orthanc::PixelFormat_SignedGrayscale16,
                       2 * series.GetWidth(), 2 * series.GetHeight() * series.GetWidth(), listener);
  }

  float sx, sy, sz;
  series.GetVoxelSize(sx, sy, sz);
  image->SetSpacing(sx, sy, sz);


  /**
   * The following code is based on the VTK sample for MIP
   * http://www.vtk.org/Wiki/VTK/Examples/Cxx/VolumeRendering/MinIntensityRendering
   **/

  // Create a transfer function mapping scalar value to opacity
  double range[2];
  image->GetScalarRange(range);

  vtkSmartPointer<vtkPiecewiseFunction> opacityTransfer = 
    vtkSmartPointer<vtkPiecewiseFunction>::New();
  opacityTransfer->AddSegment(range[0], 0.0, range[1], 1.0);
 
  vtkSmartPointer<vtkColorTransferFunction> colorTransfer = 
    vtkSmartPointer<vtkColorTransferFunction>::New();
  colorTransfer->AddRGBPoint(0, 1.0, 1.0, 1.0);
  colorTransfer->AddRGBPoint(range[1], 1.0, 1.0, 1.0);
 
  vtkSmartPointer<vtkVolumeProperty> property = 
    vtkSmartPointer<vtkVolumeProperty>::New();
  property->SetScalarOpacity(opacityTransfer);
  property->SetColor(colorTransfer);
  property->SetInterpolationTypeToLinear();

  // Create a Maximum Intensity Projection rendering
  vtkSmartPointer<vtkFixedPointVolumeRayCastMapper> mapper = 
    vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
  mapper->SetBlendModeToMaximumIntensity();
  mapper->SetInput(image);

  vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(mapper);
  volume->SetProperty(property);
  
  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkOpenGLRenderer>::New();
  renderer->AddViewProp(volume);
  renderer->SetBackground(0.1, 0.2, 0.3); // Background color dark blue

  vtkSmartPointer<vtkInteractorStyleTrackballCamera> style = 
    vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
 
  vtkSmartPointer<vtkRenderWindow> window = vtkSmartPointer<vtkRenderWindow>::New();
  window->AddRenderer(renderer); 

  vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
  interactor->SetRenderWindow(window);
  interactor->SetInteractorStyle(style);
  interactor->Start();
}


int main()
{
  // Use the commented code below if you know the identifier of a
  // series that corresponds to a 3D image.

  /*
     {
     OrthancClient::OrthancConnection orthanc("http://localhost:8042");
     OrthancClient::Series series(orthanc, "c1c4cb95-05e3bd11-8da9f5bb-87278f71-0b2b43f5");
     Display(series);
     return 0;
     }
  */


  // Try and find a 3D image inside the local store
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

        if (series.Is3DImage())
        {
          Display(series);
          return 0;
        }
        else
        {
          std::cout << "      => Not a 3D image..." << std::endl;
        }
      }
    }
  }

  std::cout << "Unable to find a 3D image in the local Orthanc store" << std::endl;

  return 0;
}
