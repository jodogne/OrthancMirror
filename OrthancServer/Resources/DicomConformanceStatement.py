#!/usr/bin/env python3

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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




# This file injects the UID information into the DICOM conformance
# statement of Orthanc

import io
import json
import os
import pystache
import re
import requests
import tarfile

BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


# Read the current version of DCMTK
with open(os.path.join(BASE, 'OrthancFramework', 'Resources', 'CMake', 'OrthancFrameworkParameters.cmake'), 'r') as f:
    r = re.search(r'set\(DCMTK_STATIC_VERSION "([0-9.]+)" CACHE STRING', f.read())
    assert(r != None)
    version = r.group(1)
    url = 'https://orthanc.uclouvain.be/downloads/third-party-downloads/dcmtk-%s.tar.gz' % version
    r = requests.get(url)
    r.raise_for_status()

    dcuid = None
    tar = tarfile.open(fileobj = io.BytesIO(r.content), mode = 'r:gz')
    for i in tar.getmembers():
        if i.name.endswith('dcmdata/include/dcmtk/dcmdata/dcuid.h'):
            dcuid = tar.extractfile(i)

    assert(dcuid != None)
    dcmtk = dcuid.readlines()


# Create an index of all the DICOM Store SCP UIDs that are known to DCMTK
all_store_scp = []
longest = 0
for l in dcmtk:
    m = re.match(r'#define UID_(.+?Storage)\s*"(.+?)"', l.decode('ascii'))
    if m != None:
        longest = max(longest, len(m.group(1)))
        all_store_scp.append({
            'name' : m.group(1),
            'uid' : m.group(2),
        })

all_store_scp = sorted(all_store_scp, key = lambda x: x['name'])

for i in range(len(all_store_scp)):
    all_store_scp[i]['name'] = all_store_scp[i]['name'].ljust(longest)

store_scp = list(filter(lambda x: not x['name'].startswith('DRAFT') and not x['name'].startswith('RETIRED'), all_store_scp))
retired_store_scp = list(filter(lambda x: x['name'].startswith('RETIRED'), all_store_scp))
draft_store_scp = list(filter(lambda x: x['name'].startswith('DRAFT'), all_store_scp))


# Read all the transfer syntaxes
with open(os.path.join(BASE, 'OrthancFramework', 'Resources', 'CodeGeneration', 'DicomTransferSyntaxes.json')) as f:
    a = json.loads(f.read())

transfer_syntaxes = []
longest = 0
for i in range(len(a)):
    item = {
        'name' : '%sTransferSyntax' % a[i]['Value'],
        'uid' : a[i]['UID'],
    }
    transfer_syntaxes.append(item)
    longest = max(longest, len(item['name']))

transfer_syntaxes = sorted(transfer_syntaxes, key = lambda x: x['uid'])

for i in range(len(transfer_syntaxes)):
    transfer_syntaxes[i]['name'] = transfer_syntaxes[i]['name'].ljust(longest)


# Inject into the template conformance statement of Orthanc
with open(os.path.join(os.path.dirname(__file__), 'DicomConformanceStatement.mustache'), 'r') as f:
    with open(os.path.join(os.path.dirname(__file__), 'DicomConformanceStatement.txt'), 'w') as g:
        g.write(pystache.render(f.read(), {
            'store_scp' : store_scp,
            'draft_store_scp' : draft_store_scp,
            'retired_store_scp' : retired_store_scp,
            'transfer_syntaxes' : transfer_syntaxes,
        }))
