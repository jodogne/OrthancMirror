#!/usr/bin/python

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
