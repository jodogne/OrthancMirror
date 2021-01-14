#!/usr/bin/env python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


import argparse
import bz2
import gzip
import os
import requests
import sys
import tarfile
import zipfile

from requests.auth import HTTPBasicAuth



parser = argparse.ArgumentParser(description = 'Command-line tool to import files or archives into Orthanc.')
parser.add_argument('--url', 
                    default = 'http://localhost:8042',
                    help = 'URL to the REST API of the Orthanc server')
parser.add_argument('--username',
                    default = 'orthanc',
                    help = 'Username to the REST API')
parser.add_argument('--password',
                    default = 'orthanc',
                    help = 'Password to the REST API')
parser.add_argument('--force', help = 'Do not warn the user about deletion',
                    action = 'store_true')
parser.add_argument('--clear', help = 'Remove the content of the Orthanc database',
                    action = 'store_true')
parser.add_argument('--verbose', help = 'Be verbose',
                    action = 'store_true')
parser.add_argument('--ignore-errors', help = 'Do not stop if encountering non-DICOM files',
                    action = 'store_true')
parser.add_argument('files', metavar = 'N', nargs = '*',
                    help = 'Files to import')


args = parser.parse_args()

if args.clear and not args.force:
    print("""
WARNING: This script will remove all the content of your
Orthanc instance running on %s!

Are you sure ["yes" to go on]?""" % args.server)

    if sys.stdin.readline().strip() != 'yes':
        print('Aborting...')
        exit(0)



IMPORTED_STUDIES = set()
COUNT_ERROR = 0
COUNT_SUCCESS = 0
        
def UploadBuffer(dicom):
    global IMPORTED_STUDIES
    global COUNT_ERROR
    global COUNT_SUCCESS
    
    auth = HTTPBasicAuth(args.username, args.password)
    r = requests.post('%s/instances' % args.url, auth = auth, data = dicom)

    try:
        r.raise_for_status()
    except:
        COUNT_ERROR += 1
        if args.ignore_errors:
            if args.verbose:
                print('  not a valid DICOM file, ignoring it')
            return
        else:
            raise
        
    info = r.json()
    COUNT_SUCCESS += 1

    if not info['ParentStudy'] in IMPORTED_STUDIES:
        IMPORTED_STUDIES.add(info['ParentStudy'])
        
        r2 = requests.get('%s/instances/%s/tags?short' % (args.url, info['ID']),
                          auth = auth)
        r2.raise_for_status()
        tags = r2.json()

        print('')
        print('New imported study:')
        print('  Orthanc ID of the patient: %s' % info['ParentPatient'])
        print('  Orthanc ID of the study: %s' % info['ParentStudy'])
        print('  DICOM Patient ID: %s' % (
            tags['0010,0020'] if '0010,0020' in tags else '(empty)'))
        print('  DICOM Study Instance UID: %s' % (
            tags['0020,000d'] if '0020,000d' in tags else '(empty)'))
        print('')


def UploadFile(path):
    with open(path, 'rb') as f:
        dicom = f.read()
        if args.verbose:
            print('Uploading: %s (%dMB)' % (path, len(dicom) / (1024 * 1024)))

        UploadBuffer(dicom)

        
def UploadBzip2(path):
    with bz2.BZ2File(path, 'rb') as f:
        dicom = f.read()
        if args.verbose:
            print('Uploading: %s (%dMB)' % (path, len(dicom) / (1024 * 1024)))

        UploadBuffer(dicom)

        
def UploadGzip(path):
    with gzip.open(path, 'rb') as f:
        dicom = f.read()
        if args.verbose:
            print('Uploading: %s (%dMB)' % (path, len(dicom) / (1024 * 1024)))

        UploadBuffer(dicom)

        
def UploadTar(path, decoder):
    if args.verbose:
        print('Uncompressing tar archive: %s' % path)
    with tarfile.open(path, decoder) as tar:
        for item in tar:
            if item.isreg():
                f = tar.extractfile(item)
                dicom = f.read()
                f.close()

                if args.verbose:
                    print('Uploading: %s (%dMB)' % (item.name, len(dicom) / (1024 * 1024)))

                UploadBuffer(dicom)

        
def UploadZip(path):
    if args.verbose:
        print('Uncompressing ZIP archive: %s' % path)
    with zipfile.ZipFile(path, 'r') as zip:
        for item in zip.infolist():
            # WARNING - "item.is_dir()" would be better, but is not available in Python 2.7
            if item.file_size > 0:
                dicom = zip.read(item.filename)

                if args.verbose:
                    print('Uploading: %s (%dMB)' % (item.filename, len(dicom) / (1024 * 1024)))

                UploadBuffer(dicom)


def DecodeFile(path):
    extension = os.path.splitext(path) [1]
    
    if path.endswith('.tar.bz2'):
        UploadTar(path, 'r:bz2')
    elif path.endswith('.tar.gz'):
        UploadTar(path, 'r:gz')        
    elif extension == '.zip':
        UploadZip(path)
    elif extension == '.tar':
        UploadTar(path, 'r')
    elif extension == '.bz2':
        UploadBzip2(path)
    elif extension == '.gz':
        UploadGzip(path)
    else:
        UploadFile(path)
                

if args.clear:
    print('Removing the content of Orthanc')

    auth = HTTPBasicAuth(args.username, args.password)
    r = requests.get('%s/studies' % args.url, auth = auth)
    r.raise_for_status()

    print('  %d studies are being removed...' % len(r.json()))
    
    for study in r.json():
        requests.delete('%s/studies/%s' % (args.url, study), auth = auth).raise_for_status()

    print('Orthanc is now empty')
    print('')
        
    
for path in args.files:
    if os.path.isfile(path):
        DecodeFile(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            for name in files:
                DecodeFile(os.path.join(root, name))
    else:
        raise Exception('Missing file or directory: %s' % path)
        

print('')
print('Status:')
print('  %d DICOM instances properly imported' % COUNT_SUCCESS)
print('  %d DICOM studies properly imported' % len(IMPORTED_STUDIES))
print('  Error in %d files' % COUNT_ERROR)
print('')
