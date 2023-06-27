#!/usr/bin/env python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


import re
import requests
import sys
import xml.etree.ElementTree as ET

# Usage:
# ./GenerateAnonymizationProfile.py https://raw.githubusercontent.com/jodogne/dicom-specification/master/2023b/part15.xml > ../../OrthancFramework/Sources/DicomParsing/DicomModification_Anonymization2023b.impl.h

if len(sys.argv) != 2:
    raise Exception('Please provide the path or the URL to the part15.xml file from the DICOM standard')

source = sys.argv[1]

if (source.startswith('http://') or
    source.startswith('https://')):
    part15 = requests.get(source).content
else:
    with open(source, 'r') as f:
        part15 = f.read()

root = ET.fromstring(part15)

br = '{http://docbook.org/ns/docbook}' # Shorthand variable


LINES = []

def FormatLine(command, name):
    indentation = 65
    
    if len(command) > indentation:
        indentation = len(command) + 2
        
    line = command + (' ' * (indentation - len(command))) + '// ' + name
    LINES.append(line)

def FormatUnknown(rawTag, name, profile):
    FormatLine('// TODO: %s with rule %s' % (rawTag, profile), name)

    
RAW_TAG_RE = re.compile(r'^\(\s*([0-9A-Fx]{4})\s*,\s*([0-9A-Fx]{4})\s*\)$')


for table in root.iter('%stable' % br):
    if table.attrib['label'] == 'E.1-1':
        for row in table.find('%stbody' % br).iter('%str' % br):
            rawTag = row.find('%std[2]/%spara' % (br, br)).text
            name = row.find('%std[1]/%spara' % (br, br)).text
            profile = row.find('%std[5]/%spara' % (br, br)).text

            if len(name.strip()) == 0:
                continue

            match = RAW_TAG_RE.match(rawTag)
            if match == None:
                raise Exception('Unsupported rule: %s, %s, %s' % (rawTag, profile, name))
            else:
                group = match.group(1).lower()
                element = match.group(2).lower()
                tag = '0x%s, 0x%s' % (group, element)

                if 'x' in group or 'x' in element:
                    if profile == 'X':
                        groupFrom = group.replace('x', '0')
                        groupTo = group.replace('x', 'f')
                        elementFrom = element.replace('x', '0')
                        elementTo = element.replace('x', 'f')
                        FormatLine('removedRanges_.push_back(DicomTagRange(0x%s, 0x%s, 0x%s, 0x%s));' % (
                            groupFrom, groupTo, elementFrom, elementTo), name)
                    else:
                        raise Exception('Unsupported rule: %s, %s, %s' % (rawTag, profile, name))
                elif tag in [
                        '0x0008, 0x0018',   # SOP Instance UID
                        '0x0020, 0x000e',   # Series Instance UID
                        '0x0020, 0x000d',   # Study Instance UID
                ]:
                    FormatLine('// Tag (%s) is set in Apply()         /* %s */' % (tag, profile), name)
                elif tag in [
                        '0x0010, 0x0010',   # Patient's Name
                        '0x0010, 0x0020',   # Patient ID
                ]:
                    FormatLine('// Tag (%s) is set below (*)          /* %s */' % (tag, profile), name)
                elif profile == 'U':
                    FormatLine('uids_.insert(DicomTag(%s));' % (tag), name)
                elif profile == 'X/Z/U*':
                    FormatLine('// RelationshipsVisitor handles (%s)  /* %s */' % (tag, profile), name)
                elif profile == 'X':
                    FormatLine('removals_.insert(DicomTag(%s));' % tag, name)
                elif profile.startswith('X/'):
                    FormatLine('removals_.insert(DicomTag(%s));   /* %s */' % (tag, profile), name)
                elif profile == 'Z':
                    FormatLine('clearings_.insert(DicomTag(%s));' % tag, name)
                elif profile == 'D' or profile.startswith('Z/'):
                    FormatLine('clearings_.insert(DicomTag(%s));  /* %s */' % (tag, profile), name)
                else:
                    # FormatUnknown(rawTag, name, profile)
                    raise Exception('Unsupported rule: %s, %s, %s' % (rawTag, profile, name))

for line in sorted(LINES):
    print(line.encode('ascii', 'ignore').decode('ascii'))
    

# D - replace with a non-zero length value that may be a dummy value and consistent with the VR
# Z - replace with a zero length value, or a non-zero length value that may be a dummy value and consistent with the VR
# X - remove
# K - keep (unchanged for non-sequence attributes, cleaned for sequences)
# C - clean, that is replace with values of similar meaning known not to contain identifying information and consistent with the VR
# U - replace with a non-zero length UID that is internally consistent within a set of Instances
# Z/D - Z unless D is required to maintain IOD conformance (Type 2 versus Type 1)
# X/Z - X unless Z is required to maintain IOD conformance (Type 3 versus Type 2)
# X/D - X unless D is required to maintain IOD conformance (Type 3 versus Type 1)
# X/Z/D - X unless Z or D is required to maintain IOD conformance (Type 3 versus Type 2 versus Type 1)
# X/Z/U* - X unless Z or replacement of contained instance UIDs (U) is required to maintain IOD conformance (Type 3 versus Type 2 versus Type 1 sequences containing UID references)
