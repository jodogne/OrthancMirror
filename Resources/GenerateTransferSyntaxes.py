#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2020 Osimis S.A., Belgium
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


import json
import os
import re
import sys

BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))



## https://www.dicomlibrary.com/dicom/transfer-syntax/
## https://cedocs.intersystems.com/latest/csp/docbook/DocBook.UI.Page.cls?KEY=EDICOM_transfer_syntax


with open(os.path.join(BASE, 'Resources', 'DicomTransferSyntaxes.json'), 'r') as f:
    SYNTAXES = json.loads(f.read())



##
## Generate the "DicomTransferSyntax" enumeration in "Enumerations.h"
##

path = os.path.join(BASE, 'Core', 'Enumerations.h')
with open(path, 'r') as f:
    a = f.read()

s = ',\n'.join(map(lambda x: '    DicomTransferSyntax_%s    /*!< %s */' % (x['Value'], x['Name']), SYNTAXES))

a = re.sub('(enum DicomTransferSyntax\s*{)[^}]*?(\s*};)', r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)


    
##
## Generate the "GetTransferSyntaxUid()" function in
## "Enumerations.cpp"
##

path = os.path.join(BASE, 'Core', 'Enumerations.cpp')
with open(path, 'r') as f:
    a = f.read()

s = '\n\n'.join(map(lambda x: '      case DicomTransferSyntax_%s:\n        return "%s";' % (x['Value'], x['UID']), SYNTAXES))
a = re.sub('(GetTransferSyntaxUid\(DicomTransferSyntax.*?\)\s*{\s*switch \([^)]*?\)\s*{)[^}]*?(\s*default:)',
           r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)

    
##
## Generate the "GetDcmtkTransferSyntax()" function in
## "FromDcmtkBridge.cpp"
##

path = os.path.join(BASE, 'Core', 'DicomParsing', 'FromDcmtkBridge.cpp')
with open(path, 'r') as f:
    a = f.read()

def Format(x):
    t = '      case DicomTransferSyntax_%s:\n        target = %s;\n        return true;' % (x['Value'], x['DCMTK'])
    if 'SinceDCMTK' in x:
        return '#if DCMTK_VERSION_NUMBER >= %s\n%s\n#endif' % (x['SinceDCMTK'], t)
    else:
        return t
    
s = '\n\n'.join(map(Format, filter(lambda x: 'DCMTK' in x, SYNTAXES)))
a = re.sub('(GetDcmtkTransferSyntax\(E_TransferSyntax.*?\s*DicomTransferSyntax.*?\)\s*{\s*switch \([^)]*?\)\s*{)[^}]*?(\s*default:)',
           r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)
