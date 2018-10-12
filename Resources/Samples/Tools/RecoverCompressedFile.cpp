/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../../../Core/Compression/ZlibCompressor.h"
#include "../../../Core/SystemToolbox.h"
#include "../../../Core/OrthancException.h"

#include <stdio.h>

int main(int argc, const char* argv[])
{
  if (argc != 2 && argc != 3)
  {
    fprintf(stderr, "Maintenance tool to recover a DICOM file that was compressed by Orthanc.\n\n");
    fprintf(stderr, "Usage: %s <input> [output]\n", argv[0]);
    fprintf(stderr, "If \"output\" is not given, the data will be output to stdout\n");
    return -1;
  }

  try
  {
    fprintf(stderr, "Reading the file into memory...\n");
    fflush(stderr);

    std::string content;
    Orthanc::SystemToolbox::ReadFile(content, argv[1]);

    fprintf(stderr, "Decompressing the content of the file...\n");
    fflush(stderr);

    Orthanc::ZlibCompressor compressor;
    std::string uncompressed;
    compressor.Uncompress(uncompressed, 
                          content.empty() ? NULL : content.c_str(), 
                          content.size());

    fprintf(stderr, "Writing the uncompressed data...\n");
    fflush(stderr);

    if (argc == 3)
    {
      Orthanc::SystemToolbox::WriteFile(uncompressed, argv[2]);
    }
    else
    {
      if (uncompressed.size() > 0)
      {
        fwrite(&uncompressed[0], uncompressed.size(), 1, stdout);
      }
    }

    fprintf(stderr, "Done!\n");
  }
  catch (Orthanc::OrthancException& e)
  {
    fprintf(stderr, "Error: %s\n", e.What());
    return -1;
  }

  return 0;
}
