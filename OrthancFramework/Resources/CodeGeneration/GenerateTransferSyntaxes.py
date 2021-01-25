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
import pystache

BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))



## https://www.dicomlibrary.com/dicom/transfer-syntax/
## https://cedocs.intersystems.com/latest/csp/docbook/DocBook.UI.Page.cls?KEY=EDICOM_transfer_syntax


with open(os.path.join(BASE, 'Resources', 'CodeGeneration', 'DicomTransferSyntaxes.json'), 'r') as f:
    SYNTAXES = json.loads(f.read())



##
## Generate the "DicomTransferSyntax" enumeration in "Enumerations.h"
##

path = os.path.join(BASE, 'Sources', 'Enumerations.h')
with open(path, 'r') as f:
    a = f.read()

s = ',\n'.join(map(lambda x: '    DicomTransferSyntax_%s    /*!< %s */' % (x['Value'], x['Name']), SYNTAXES))

a = re.sub('(enum DicomTransferSyntax\s*{)[^}]*?(\s*};)', r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)



##
## Generate the implementations
##

with open(os.path.join(BASE, 'Sources', 'Enumerations_TransferSyntaxes.impl.h'), 'w') as b:
    with open(os.path.join(BASE, 'Resources', 'CodeGeneration', 'GenerateTransferSyntaxesEnumerations.mustache'), 'r') as a:
        b.write(pystache.render(a.read(), {
            'Syntaxes' : SYNTAXES
        }))

with open(os.path.join(BASE, 'Sources', 'DicomParsing', 'FromDcmtkBridge_TransferSyntaxes.impl.h'), 'w') as b:
    with open(os.path.join(BASE, 'Resources', 'CodeGeneration', 'GenerateTransferSyntaxesDcmtk.mustache'), 'r') as a:
        b.write(pystache.render(a.read(), {
            'Syntaxes' : SYNTAXES
        }))
