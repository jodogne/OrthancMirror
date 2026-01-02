#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
# This sample Python script illustrates how to encapsulate a JPEG
# image into a DICOM enveloppe, without any transcoding.
#

import base64
import json
import requests


##################
##  Parameters  ##
##################

JPEG = '/tmp/sample.jpg'
URL = 'http://localhost:8042/'
USERNAME = 'orthanc'
PASSWORD = 'orthanc'

TAGS = {
    'PatientID' : 'Test',
    'PatientName' : 'Hello^World',
    'SOPClassUID' : '1.2.840.10008.5.1.4.1.1.7',  # Secondary capture
    }



########################################
##  Application of the DICOM-ization  ##
########################################

with open(JPEG, 'rb') as f:
    jpeg = f.read()

b = base64.b64encode(jpeg)
content = 'data:image/jpeg;base64,%s' % b.decode()

command = {
    'Content' : content,
    'Tags' : TAGS,
    'Encapsulate' : True,
}

r = requests.post('%s/tools/create-dicom' % URL, json.dumps(command),
                  auth = requests.auth.HTTPBasicAuth(USERNAME, PASSWORD))
r.raise_for_status()

print('URL of the newly created instance: %s/instances/%s/file' % (URL, r.json() ['ID']))
