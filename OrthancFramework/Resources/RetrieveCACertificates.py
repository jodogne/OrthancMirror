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


import re
import sys
import subprocess
import urllib2


if len(sys.argv) <= 2:
    print('Download a set of CA certificates, convert them to PEM, then format them as a C macro')
    print('Usage: %s [Macro] [Certificate1] <Certificate2>...' % sys.argv[0])
    print('')
    print('Example: %s BITBUCKET_CERTIFICATES https://www.digicert.com/CACerts/DigiCertHighAssuranceEVRootCA.crt' % sys.argv[0])
    print('')
    sys.exit(-1)

MACRO = sys.argv[1]

sys.stdout.write('#define %s ' % MACRO)

for url in sys.argv[2:]:
    # Download the certificate from the CA authority, in the DES format
    des = urllib2.urlopen(url).read()

    # Convert DES to PEM
    p = subprocess.Popen([ 'openssl', 'x509', '-inform', 'DES', '-outform', 'PEM' ],
                         stdin = subprocess.PIPE,
                         stdout = subprocess.PIPE)
    pem = p.communicate(input = des)[0]
    pem = re.sub(r'\r', '', pem)       # Remove any carriage return
    pem = re.sub(r'\\', r'\\\\', pem)  # Escape any backslash
    pem = re.sub(r'"', r'\\"', pem)    # Escape any quote

    # Write the PEM data into the macro
    for line in pem.split('\n'):
        sys.stdout.write(' \\\n')
        sys.stdout.write('"%s\\n" ' % line)

sys.stdout.write('\n')
sys.stderr.write('Done!\n')
