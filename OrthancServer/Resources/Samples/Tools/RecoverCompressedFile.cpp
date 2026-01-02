/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../../../OrthancFramework/Sources/Compression/ZlibCompressor.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"

#include <stdio.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#endif


#if defined(_WIN32) && !defined(__MINGW32__)
// arguments are passed as UTF-16 on Windows
int wmain(int argc, wchar_t *argv[])
{
  // Set Windows console output to UTF-8 (otherwise, strings are considered to be in UTF-16.  For example, Cyrillic UTF-8 strings appear as garbage without that config)
  SetConsoleOutputCP(CP_UTF8);

  // Transform the UTF-16 arguments into UTF-8 arguments
  std::vector<std::string> arguments; // UTF-8 arguments

  for (int i = 0; i < argc; i++)
  {
    std::wstring argument(argv[i]);
    arguments.push_back(Orthanc::SystemToolbox::WStringToUtf8(argument));
  }

#else
int main(int argc, char *argv[])
{
  std::vector<std::string> arguments; // UTF-8 arguments

  // the arguments are assumed to be directly in UTF-8
  for (int i = 0; i < argc; i++)
  {
    arguments.push_back(argv[i]);
  }
#endif

  if (arguments.size() != 2 && arguments.size() != 3)
  {
    fprintf(stderr, "Maintenance tool to recover a DICOM file that was compressed by Orthanc.\n\n");
    fprintf(stderr, "Usage: %s <input> [output]\n", arguments[0].c_str());
    fprintf(stderr, "If \"output\" is not given, the data will be output to stdout\n");
    return -1;
  }

  try
  {
    fprintf(stderr, "Reading the file into memory...\n");
    fflush(stderr);

    std::string content;
    Orthanc::SystemToolbox::ReadFile(content, Orthanc::SystemToolbox::PathFromUtf8(arguments[1]));

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
      Orthanc::SystemToolbox::WriteFile(uncompressed, Orthanc::SystemToolbox::PathFromUtf8(arguments[2]));
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
