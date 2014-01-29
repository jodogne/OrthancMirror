#include "../../../Core/Compression/ZlibCompressor.h"
#include "../../../Core/Toolbox.h"
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
    Orthanc::Toolbox::ReadFile(content, argv[1]);

    fprintf(stderr, "Decompressing the content of the file...\n");
    fflush(stderr);

    Orthanc::ZlibCompressor compressor;
    std::string uncompressed;
    compressor.Uncompress(uncompressed, content);

    fprintf(stderr, "Writing the uncompressed data...\n");
    fflush(stderr);

    if (argc == 3)
    {
      Orthanc::Toolbox::WriteFile(uncompressed, argv[2]);
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
