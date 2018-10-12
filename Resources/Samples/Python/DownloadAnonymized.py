#!/usr/bin/python
# -*- coding: utf-8 -*-

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2018 Osimis S.A., Belgium
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



URL = 'http://127.0.0.1:8042'

#
# This sample code will download a ZIP file for each patient that has
# been anonymized in Orthanc.
#

import os
import os.path
import sys
import RestToolbox

# Loop over the patients
for patient in RestToolbox.DoGet('%s/patients' % URL):

    # Ignore patients whose name starts with "Anonymized", as it is
    # the result of a previous anonymization
    infos = RestToolbox.DoGet('%s/patients/%s' % (URL, patient))
    name = infos['MainDicomTags']['PatientName'].lower()
    if name.startswith('anonymized'):

        # Trigger the download
        print('Downloading %s' % name)
        zipContent = RestToolbox.DoGet('%s/patients/%s/archive' % (URL, patient))
        with open(os.path.join('/tmp', name + '.zip'), 'wb') as f:
            f.write(zipContent)
