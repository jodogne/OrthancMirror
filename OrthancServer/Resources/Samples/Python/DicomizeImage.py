#!/usr/bin/python3
# -*- coding: utf-8 -*-

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


#
# This sample Python script illustrates how to DICOM-ize a JPEG image,
# a PNG image, or a PDF file using the route "/tools/create-dicom" of
# the REST API of Orthanc. Make sure to adapt the parameters of the
# DICOM-ization below.
#
# The following command-line will install the required library:
#
#  $ sudo pip3 install requests
#

import base64
import imghdr
import json
import os
import requests


########################################
##  Parameters for the DICOM-ization  ##
########################################

PATH = os.path.join(os.getenv('HOME'), 'Downloads', 'Spy 11B.jpg')

URL = 'http://localhost:8042/'
USERNAME = 'orthanc'
PASSWORD = 'orthanc'

TAGS = {
    'ConversionType' : 'SI',  # Scanned Image
    'InstanceNumber' : '1',
    'Laterality' : '',
    'Modality' : 'OT',
    'PatientOrientation' : '',
    'SOPClassUID' : '1.2.840.10008.5.1.4.1.1.7',  # Secondary Capture Image Storage
    'SeriesNumber' : '1',
    }

if True:
    # Case 1: Attach the new DICOM image as a new series in an
    # existing study. In this case, "PARENT_STUDY" indicates the
    # Orthanc identifier of the parent study:
    # https://book.orthanc-server.com/faq/orthanc-ids.html
    PARENT_STUDY = '66c8e41e-ac3a9029-0b85e42a-8195ee0a-92c2e62e'

else:
    # Case 2: Create a new study
    PARENT_STUDY = None
    STUDY_TAGS = {
        'PatientID' : 'Test',
        'PatientName' : 'Hello^World',
        'PatientSex' : 'O',
        
        'PatientBirthDate' : None,
        'StudyID' : 'Test',
        'ReferringPhysicianName' : None,
        'AccessionNumber' : None,
    }

    TAGS.update(STUDY_TAGS)



########################################
##  Application of the DICOM-ization  ##
########################################

if imghdr.what(PATH) == 'jpeg':
    mime = 'image/jpeg'
elif imghdr.what(PATH) == 'png':
    mime = 'image/png'
elif os.path.splitext(PATH) [1] == '.pdf':
    mime = 'application/pdf'
else:
    raise Exception('The input image is neither JPEG, nor PNG, nor PDF')

with open(PATH, 'rb') as f:
    content = f.read()

data = 'data:%s;base64,%s' % (mime, base64.b64encode(content).decode('ascii'))

arguments = {
    'Content': data,
    'Tags': TAGS,
}

if PARENT_STUDY != None:
    arguments['Parent'] = PARENT_STUDY

r = requests.post('%s/tools/create-dicom' % URL,
                  json.dumps(arguments),
                  auth = requests.auth.HTTPBasicAuth(USERNAME, PASSWORD))
r.raise_for_status()
