#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

import re

# Read the conformance statement of Orthanc
with open('DicomConformanceStatement.txt', 'r') as f:
    statements = f.readlines()

# Create an index of all the DICOM UIDs that are known to DCMTK
uids = {}
with open('/usr/include/dcmtk/dcmdata/dcuid.h', 'r') as dcmtk:
    for l in dcmtk.readlines():
        m = re.match(r'#define UID_(.+?)\s*"(.+?)"', l)
        if m != None:
            uids[m.group(1)] = m.group(2)

# Loop over the lines of the statement, looking for the "|" separator
with open('/tmp/DicomConformanceStatement.txt', 'w') as f:
    for l in statements:
        m = re.match(r'(\s*)(.*?)(\s*)\|.*$', l)
        if m != None:
            name = m.group(2)
            uid = uids[name]
            f.write('%s%s%s| %s\n' % (m.group(1), name, m.group(3), uid))

        else:
            # No "|" in this line, just output it
            f.write(l)
