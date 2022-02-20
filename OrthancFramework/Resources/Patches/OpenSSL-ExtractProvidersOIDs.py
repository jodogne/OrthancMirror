#!/usr/bin/env python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
# Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


##
## This is a maintenance script to automatically extract the OIDs
## generated from the ".asn1" files by the OpenSSL configuration
## script "./Configure". This script generates the file
## "OpenSSL-ExtractProvidersOIDs.json". The output JSON is then used
## by "OpenSSL-ConfigureHeaders.py".
##


import json
import os
import re
import sys

if len(sys.argv) != 2:
    raise Exception('Provide the path to your configured OpenSSL 3.x build directory')

BASE = os.path.join(sys.argv[1], 'providers/common/include/prov')
TARGET = 'OpenSSL-ExtractProvidersOIDs.json'
RESULT = {}


for source in os.listdir(BASE):
    if source.endswith('.h.in'):
        path = os.path.join(BASE, re.sub('.in$', '', source))

        content = {}
        
        with open(path, 'r') as f:
            for definition in re.findall('#define (DER_OID_V_.+?)#define (DER_OID_SZ_.+?)extern const(.+?)$', f.read(), re.MULTILINE | re.DOTALL):
                oid = definition[0].strip().split(' ')
                
                name = oid[0].replace('DER_OID_V_', '')
                oid = oid[1:]

                sizes = definition[1].strip().split(' ')
                if (name in content or
                    len(sizes) != 2 or
                    sizes[0] != 'DER_OID_SZ_%s' % name or
                    int(sizes[1]) != len(oid)):
                    raise Exception('Cannot parse %s, for OID %s' % (path, name))

                content[name] = list(map(lambda x: x.replace(',', ''), oid))

        RESULT[source] = content


with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), TARGET), 'w') as f:
    f.write(json.dumps(RESULT, sort_keys = True, indent = 4))
