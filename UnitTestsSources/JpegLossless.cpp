/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1

#include <dcmtk/dcmjpls/djlsutil.h>
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <dcmtk/dcmjpls/djcodecd.h>

#include "../OrthancServer/ParsedDicomFile.h"
#include "../OrthancServer/FromDcmtkBridge.h"

using namespace Orthanc;

TEST(JpegLossless, Basic)
{
  DJLSDecoderRegistration::registerCodecs( EJLSUC_default, EJLSPC_restore,OFFalse );

#if 0
  std::string s;
  Toolbox::ReadFile(s, "IM-0001-1001-0001.dcm");

  ParsedDicomFile parsed(s);
  DcmFileFormat& dicom = *reinterpret_cast<DcmFileFormat*>(parsed.GetDcmtkObject());

  DcmDataset* dataset = dicom.getDataset();

  dataset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

  if (dataset->canWriteXfer(EXS_LittleEndianExplicit))
  {
    printf("ICI\n");

    parsed.SaveToFile("tutu.dcm");

    FromDcmtkBridge::ExtractPngImage(s, *dataset, 1, ImageExtractionMode_Preview);
    //fileformat.saveFile("test_decompressed.dcm", EXS_LittleEndianExplicit);
  }
#else
  DcmFileFormat fileformat;
  if (fileformat.loadFile("IM-0001-1001-0001.dcm").good())
  {
    DcmDataset *dataset = fileformat.getDataset();

    // decompress data set if compressed
    dataset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

    DcmXfer original_xfer(dataset->getOriginalXfer());
    std::cout << original_xfer.getXferName() << std::endl;

    printf("OK1\n");

    // check if everything went well
    if (1) //dataset->canWriteXfer(EXS_LittleEndianExplicit))
    {
      printf("OK2\n");

      fileformat.saveFile("tutu.dcm", EXS_LittleEndianExplicit);
    }
  }


#endif


  // http://support.dcmtk.org/docs/classDJLSLosslessDecoder.html
  //DJLSDecoderBase b;
  DJLSLosslessDecoder bb;

  DJLSDecoderRegistration::cleanup();
}


#endif
