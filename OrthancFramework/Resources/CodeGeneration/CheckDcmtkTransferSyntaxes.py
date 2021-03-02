#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


import json
import os
import re
import sys

if len(sys.argv) != 2:
    print('Usage: %s [Path to DCMTK source code]' % sys.argv[0])
    exit(-1)


orthancSyntaxes = []
    

with open(os.path.join(os.path.dirname(__file__), 'DicomTransferSyntaxes.json'), 'r') as f:
    for syntax in json.loads(f.read()):
        orthancSyntaxes.append(syntax['UID'])


with open(os.path.join(sys.argv[1], 'dcmdata/include/dcmtk/dcmdata/dcuid.h'), 'r') as f:
    r = re.compile(r'^#define UID_([A-Za-z0-9_]+)TransferSyntax\s+"([0-9.]+)"$')
    
    for line in f.readlines():
        m = r.match(line)
        if m != None:
            syntax = m.group(2)
            if not syntax in orthancSyntaxes:
                print('Missing syntax: %s => %s' % (syntax, m.group(1)))
