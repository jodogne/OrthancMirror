#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2024 Osimis S.A., Belgium
# Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


import os
import sys
import datetime

if len(sys.argv) != 5:
    sys.stderr.write('Usage: %s <Version> <ProductName> <Filename> <Description>\n\n' % sys.argv[0])
    sys.stderr.write('Example: %s 0.9.1 Orthanc Orthanc.exe "Lightweight, RESTful DICOM server for medical imaging"\n' % sys.argv[0])
    sys.exit(-1)

SOURCE = os.path.join(os.path.dirname(__file__), 'WindowsResources.rc')

VERSION = sys.argv[1]
PRODUCT = sys.argv[2]
FILENAME = sys.argv[3]
DESCRIPTION = sys.argv[4]

if VERSION == 'mainline':
    VERSION = '999.999.999'
    RELEASE = 'This is a mainline build, not an official release'
else:
    RELEASE = 'Release %s' % VERSION

v = VERSION.split('.')
if len(v) != 2 and len(v) != 3:
    sys.stderr.write('Bad version number: %s\n' % VERSION)
    sys.exit(-1)

if len(v) == 2:
    v.append('0')

extension = os.path.splitext(FILENAME)[1]
if extension.lower() == '.dll':
    BLOCK = '040904E4'
    TYPE = 'VFT_DLL'
elif extension.lower() == '.exe':
    #BLOCK = '040904B0'   # LANG_ENGLISH/SUBLANG_ENGLISH_US,
    BLOCK = '040904E4'   # Lang=US English, CharSet=Windows Multilingual
    TYPE = 'VFT_APP'
else:
    sys.stderr.write('Unsupported extension (.EXE or .DLL only): %s\n' % extension)
    sys.exit(-1)


with open(SOURCE, 'r') as source:
    content = source.read()
    content = content.replace('${VERSION_MAJOR}', v[0])
    content = content.replace('${VERSION_MINOR}', v[1])
    content = content.replace('${VERSION_PATCH}', v[2])
    content = content.replace('${RELEASE}', RELEASE)
    content = content.replace('${DESCRIPTION}', DESCRIPTION)
    content = content.replace('${PRODUCT}', PRODUCT)   
    content = content.replace('${FILENAME}', FILENAME)   
    content = content.replace('${YEAR}', str(datetime.datetime.now().year))
    content = content.replace('${BLOCK}', BLOCK)
    content = content.replace('${TYPE}', TYPE)

    sys.stdout.write(content)
