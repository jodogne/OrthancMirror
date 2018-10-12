#!/usr/bin/python

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

START_PLUGINS = 1000000
BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))



## 
## Read all the available error codes and HTTP status
##

with open(os.path.join(BASE, 'Resources', 'ErrorCodes.json'), 'r') as f:
    ERRORS = json.loads(re.sub('/\*.*?\*/', '', f.read()))

for error in ERRORS:
    if error['Code'] >= START_PLUGINS:
        print('ERROR: Error code must be below %d, but "%s" is set to %d' % (START_PLUGINS, error['Name'], error['Code']))
        sys.exit(-1)

with open(os.path.join(BASE, 'Core', 'Enumerations.h'), 'r') as f:
    a = f.read()

HTTP = {}
for i in re.findall('(HttpStatus_([0-9]+)_\w+)', a):
    HTTP[int(i[1])] = i[0]



##
## Generate the "ErrorCode" enumeration in "Enumerations.h"
##

path = os.path.join(BASE, 'Core', 'Enumerations.h')
with open(path, 'r') as f:
    a = f.read()

s = ',\n'.join(map(lambda x: '    ErrorCode_%s = %d    /*!< %s */' % (x['Name'], int(x['Code']), x['Description']), ERRORS))

s += ',\n    ErrorCode_START_PLUGINS = %d' % START_PLUGINS
a = re.sub('(enum ErrorCode\s*{)[^}]*?(\s*};)', r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)



##
## Generate the "OrthancPluginErrorCode" enumeration in "OrthancCPlugin.h"
##

path = os.path.join(BASE, 'Plugins', 'Include', 'orthanc', 'OrthancCPlugin.h')
with open(path, 'r') as f:
    a = f.read()

s = ',\n'.join(map(lambda x: '    OrthancPluginErrorCode_%s = %d    /*!< %s */' % (x['Name'], int(x['Code']), x['Description']), ERRORS))
s += ',\n\n    _OrthancPluginErrorCode_INTERNAL = 0x7fffffff\n  '
a = re.sub('(typedef enum\s*{)[^}]*?(} OrthancPluginErrorCode;)', r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)



##
## Generate the "EnumerationToString(ErrorCode)" and
## "ConvertErrorCodeToHttpStatus(ErrorCode)" functions in
## "Enumerations.cpp"
##

path = os.path.join(BASE, 'Core', 'Enumerations.cpp')
with open(path, 'r') as f:
    a = f.read()

s = '\n\n'.join(map(lambda x: '      case ErrorCode_%s:\n        return "%s";' % (x['Name'], x['Description']), ERRORS))
a = re.sub('(EnumerationToString\(ErrorCode.*?\)\s*{\s*switch \([^)]*?\)\s*{)[^}]*?(\s*default:)',
           r'\1\n%s\2' % s, a, re.DOTALL)

def GetHttpStatus(x):
    s = HTTP[x['HttpStatus']]
    return '      case ErrorCode_%s:\n        return %s;' % (x['Name'], s)

s = '\n\n'.join(map(GetHttpStatus, filter(lambda x: 'HttpStatus' in x, ERRORS)))
a = re.sub('(ConvertErrorCodeToHttpStatus\(ErrorCode.*?\)\s*{\s*switch \([^)]*?\)\s*{)[^}]*?(\s*default:)',
           r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)



##
## Generate the "ErrorCode" enumeration in "OrthancSQLiteException.h"
##

path = os.path.join(BASE, 'Core', 'SQLite', 'OrthancSQLiteException.h')
with open(path, 'r') as f:
    a = f.read()

e = filter(lambda x: 'SQLite' in x and x['SQLite'], ERRORS)
s = ',\n'.join(map(lambda x: '      ErrorCode_%s' % x['Name'], e))
a = re.sub('(enum ErrorCode\s*{)[^}]*?(\s*};)', r'\1\n%s\2' % s, a, re.DOTALL)

s = '\n\n'.join(map(lambda x: '          case ErrorCode_%s:\n            return "%s";' % (x['Name'], x['Description']), e))
a = re.sub('(EnumerationToString\(ErrorCode.*?\)\s*{\s*switch \([^)]*?\)\s*{)[^}]*?(\s*default:)',
           r'\1\n%s\2' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)



##
## Generate the "PrintErrors" function in "main.cpp"
##

path = os.path.join(BASE, 'OrthancServer', 'main.cpp')
with open(path, 'r') as f:
    a = f.read()

s = '\n'.join(map(lambda x: '    PrintErrorCode(ErrorCode_%s, "%s");' % (x['Name'], x['Description']), ERRORS))
a = re.sub('(static void PrintErrors[^{}]*?{[^{}]*?{)([^}]*?)}', r'\1\n%s\n  }' % s, a, re.DOTALL)

with open(path, 'w') as f:
    f.write(a)
