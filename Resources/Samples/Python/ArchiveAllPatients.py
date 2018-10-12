#!/usr/bin/python
# -*- coding: utf-8 -*-

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2018 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.



import os
import os.path
import sys
import RestToolbox

def PrintHelp():
    print('Download one ZIP archive for all the patients stored in Orthanc\n')
    print('Usage: %s <URL> <Target>\n' % sys.argv[0])
    print('Example: %s http://127.0.0.1:8042/ /tmp/Archive.zip\n' % sys.argv[0])
    exit(-1)

if len(sys.argv) != 3:
    PrintHelp()

URL = sys.argv[1]
TARGET = sys.argv[2]

patients = RestToolbox.DoGet('%s/patients' % URL)

print('Downloading ZIP...')
zipContent = RestToolbox.DoPost('%s/tools/create-archive' % URL, patients)

# Write the ZIP archive at the proper location
with open(TARGET, 'wb') as f:
    f.write(zipContent)
