#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# In addition, as a special exception, the copyright holders of this
# program give permission to link the code of its release with the
# OpenSSL project's "OpenSSL" library (or with modified versions of it
# that use the same license as the "OpenSSL" library), and distribute
# the linked executables. You must obey the GNU General Public License
# in all respects for all of the code used other than "OpenSSL". If you
# modify file(s) with this exception, you may extend this exception to
# your version of the file(s), but you are not obligated to do so. If
# you do not wish to do so, delete this exception statement from your
# version. If you delete this exception statement from all source files
# in the program, then also delete it here.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


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
if len(v) != 3:
    sys.stderr.write('Bad version number: %s\n' % VERSION)
    sys.exit(-1)

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
