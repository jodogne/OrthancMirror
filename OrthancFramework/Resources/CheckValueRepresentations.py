#!/usr/bin/env python3

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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



import lxml.etree
import os
import re
import requests


##
## Download and parse the part of the DICOM spec containing the value
## representations as XML
##

SOURCE = 'https://dicom.nema.org/medical/dicom/current/source/docbook/part05/part05.xml'
TMP = '/tmp/dicom.xml'

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

if not os.path.isfile(TMP):
    r = requests.get(SOURCE)
    r.raise_for_status()
    with open(TMP, 'wb') as f:
        f.write(r.content)


with open(TMP, 'rb') as f:
    doc = lxml.etree.fromstring(f.read())


# Grab the default namespace from the document root
default_ns = doc.nsmap.get(None)  # e.g. "http://docbook.org/ns/docbook"

ns = {
    'xml': 'http://www.w3.org/XML/1998/namespace',
    'db': default_ns,
}



##
## Locate the list of all the VRs
##

table = doc.xpath('//*[@xml:id="table_6.2-1"]', namespaces=ns)

all_vrs = table[0].xpath('.//db:tbody/db:tr/db:td[1]/db:para[1]/text()', namespaces=ns)


##
## Check for the enumerations
##

print('\nChecking Enumerations.h')

vrs = []
names = []

with open(os.path.join(BASE, 'OrthancFramework', 'Sources', 'Enumerations.h'), 'r') as f:
    for line in f.read().splitlines():
        if 'ValueRepresentation_' in line:
            m = re.search(r'(ValueRepresentation_.+?)\s*=.*// ([A-Z][A-Z])', line)
            if m != None:
                names.append(m.group(1))
                vrs.append(m.group(2))

for vr in all_vrs:
    if not vr in vrs:
        print('  - Value representation not defined:', vr)



with open(os.path.join(BASE, 'OrthancServer', 'Plugins', 'Include', 'orthanc', 'OrthancCPlugin.h'), 'r') as f:
    content = f.read()

    print('\nChecking OrthancCPlugin.h')

    func = re.search(r'OrthancPluginValueRepresentation.*?_OrthancPluginValueRepresentation_INTERNAL', content, re.DOTALL).group(0)

    for vr in vrs:
        if not ('OrthancPluginValueRepresentation_%s' % vr) in func:
            print('  - Value representation not handled:', vr)



with open(os.path.join(BASE, 'OrthancFramework', 'Sources', 'Enumerations.cpp'), 'r') as f:
    content = f.read()

    print('\nChecking EnumerationToString()')

    func = re.search(r'EnumerationToString\(ValueRepresentation.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('case %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('return "%s"' % vr) in func:
            print('  - Value representation not handled:', vr)


    print('\nChecking StringToValueRepresentation()')

    func = re.search(r'StringToValueRepresentation\(.*?throw OrthancException', content, re.DOTALL).group(0)
    for name in names:
        if not ('return %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('== "%s"' % vr) in func:
            print('  - Value representation not handled:', vr)


    print('\nChecking IsBinaryValueRepresentation()')

    func = re.search(r'IsBinaryValueRepresentation\(.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('case %s' % name) in func:
            print('  - Value representation not handled:', name)



with open(os.path.join(BASE, 'OrthancFramework', 'Sources', 'DicomFormat', 'DicomMap.cpp'), 'r') as f:
    content = f.read()

    print('\nChecking DicomMap::ValidateTag()')

    func = re.search(r'ValidateTag\(.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('case %s' % name) in func:
            print('  - Value representation not handled:', name)



with open(os.path.join(BASE, 'OrthancFramework', 'Sources', 'DicomParsing', 'FromDcmtkBridge.cpp'), 'r') as f:
    content = f.read()

    print('\nChecking FromDcmtkBridge::Convert()')

    func = re.search(r'ValueRepresentation FromDcmtkBridge::Convert.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('return %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('case EVR_%s' % vr) in func:
            print('  - Value representation not handled:', vr)



with open(os.path.join(BASE, 'OrthancFramework', 'Sources', 'DicomParsing', 'ToDcmtkBridge.cpp'), 'r') as f:
    content = f.read()

    print('\nChecking ToDcmtkBridge::Convert()')

    func = re.search(r'ToDcmtkBridge::Convert\(ValueRepresentation.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('case %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('EVR_%s' % vr) in func:
            print('  - Value representation not handled:', vr)



with open(os.path.join(BASE, 'OrthancServer', 'Plugins', 'Engine', 'PluginsEnumerations.cpp'), 'r') as f:
    content = f.read()

    print('\nChecking ConvertToDcmtkBridge::Convert(ValueRepresentation)')

    func = re.search(r'OrthancPluginValueRepresentation Convert\(ValueRepresentation.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('case %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('return OrthancPluginValueRepresentation_%s' % vr) in func:
            print('  - Value representation not handled:', vr)


    print('\nChecking ConvertToDcmtkBridge::Convert(OrthancPluginValueRepresentation)')

    func = re.search(r'ValueRepresentation Convert\(OrthancPluginValueRepresentation.*?default', content, re.DOTALL).group(0)
    for name in names:
        if not ('return %s' % name) in func:
            print('  - Value representation not handled:', name)

    for vr in vrs:
        if not ('case OrthancPluginValueRepresentation_%s' % vr) in func:
            print('  - Value representation not handled:', vr)


print('\nDone\n')
