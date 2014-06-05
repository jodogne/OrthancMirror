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
#include <dcmtk/dcmjpls/djcparam.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include <dcmtk/dcmdata/dcstack.h>
#include <dcmtk/dcmdata/dcpixseq.h>

#include "../OrthancServer/ParsedDicomFile.h"
#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/ToDcmtkBridge.h"
#include "../Core/OrthancException.h"
#include "../Core/ImageFormats/ImageBuffer.h"
#include "../Core/ImageFormats/PngWriter.h"

#include <boost/lexical_cast.hpp>

using namespace Orthanc;

TEST(JpegLossless, Basic)
{
  //DJLSDecoderRegistration::registerCodecs( EJLSUC_default, EJLSPC_restore,OFFalse );

#if 0
  // Fallback

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

    // decompress data set if compressed
    dataset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

    DcmXfer original_xfer(dataset->getOriginalXfer());
    std::cout << original_xfer.getXferName() << std::endl;

    FromDcmtkBridge::ExtractPngImage(s, *dataset, 1, ImageExtractionMode_Preview);
    //fileformat.saveFile("test_decompressed.dcm", EXS_LittleEndianExplicit);
  }
#else
  DcmFileFormat fileformat;
  //if (fileformat.loadFile("IM-0001-1001-0001.dcm").good())
  if (fileformat.loadFile("tata.dcm").good())
  {
    DcmDataset *dataset = fileformat.getDataset();

    // <data-set xfer="1.2.840.10008.1.2.4.80" name="JPEG-LS Lossless">

    DcmTag k(DICOM_TAG_PIXEL_DATA.GetGroup(),
             DICOM_TAG_PIXEL_DATA.GetElement());

    DcmElement *element = NULL;
    if (dataset->findAndGetElement(k, element).good())
    {
      DcmPixelData& pixelData = dynamic_cast<DcmPixelData&>(*element);
      DcmPixelSequence* pixelSequence = NULL;
      if (pixelData.getEncapsulatedRepresentation
          (dataset->getOriginalXfer(), NULL, pixelSequence).good())
      {
        OFString value;

        if (!dataset->findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_COLUMNS), value).good())
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }

        unsigned int width = boost::lexical_cast<unsigned int>(value.c_str());

        if (!dataset->findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_ROWS), value).good())
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }

        unsigned int height = boost::lexical_cast<unsigned int>(value.c_str());

        if (!dataset->findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_BITS_STORED), value).good())
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }

        unsigned int bitsStored = boost::lexical_cast<unsigned int>(value.c_str());

        if (!dataset->findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_PIXEL_REPRESENTATION), value).good())
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }

        bool isSigned = (boost::lexical_cast<unsigned int>(value.c_str()) != 0);

        unsigned int samplesPerPixel = 1; // By default
        if (dataset->findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_SAMPLES_PER_PIXEL), value).good())
        {
          samplesPerPixel = boost::lexical_cast<unsigned int>(value.c_str());
        }

        ImageBuffer buffer;
        buffer.SetHeight(height);
        buffer.SetWidth(width);

        if (bitsStored == 8 && samplesPerPixel == 1 && !isSigned)
        {
          buffer.SetFormat(PixelFormat_Grayscale8);
        }
        else if (bitsStored == 8 && samplesPerPixel == 3 && !isSigned)
        {
          buffer.SetFormat(PixelFormat_RGB24);
        }
        else if (bitsStored == 16 && samplesPerPixel == 1 && !isSigned)
        {
          buffer.SetFormat(PixelFormat_Grayscale16);
        }
        else if (bitsStored == 16 && samplesPerPixel == 1 && isSigned)
        {
          buffer.SetFormat(PixelFormat_SignedGrayscale16);
        }
        else
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }

        ImageAccessor accessor(buffer.GetAccessor());

        // http://support.dcmtk.org/docs/classDJLSLosslessDecoder.html
        DJLSLosslessDecoder bb; DJLSCodecParameter cp;
        //DJLSNearLosslessDecoder bb; DJLSCodecParameter cp;

        Uint32 startFragment = 0;  // Default 
        OFString decompressedColorModel;  // Out
        DJ_RPLossless rp;
        OFCondition c = bb.decodeFrame(&rp, pixelSequence, &cp, dataset, 0, startFragment, 
                                       accessor.GetBuffer(), accessor.GetSize(), decompressedColorModel);



        for (unsigned int y = 0; y < accessor.GetHeight(); y++)
        {
          int16_t *p = reinterpret_cast<int16_t*>(accessor.GetRow(y));
          for (unsigned int x = 0; x < accessor.GetWidth(); x++, p ++)
          {
            if (*p < 0)
              *p = 0;
          }
        }

        PngWriter w;
        w.WriteToFile("tata.png", accessor);
      }
    }
  }

#endif


  //DJLSDecoderRegistration::cleanup();
}


#endif
