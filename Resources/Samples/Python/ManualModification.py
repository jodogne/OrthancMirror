#!/usr/bin/python

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



# This sample shows how to carry on a manual modification of DICOM
# tags spread accross various levels (Patient/Study/Series/Instance)
# that would normally forbidden as such by the REST API of Orthanc to
# avoid breaking the DICOM hierarchy. This sample can be useful for
# more complex anonymization/modification scenarios, or for optimizing
# the disk usage (the original and the modified instances never
# coexist).

from RestToolbox import *

URL = 'http://127.0.0.1:8042'
STUDY = '27f7126f-4f66fb14-03f4081b-f9341db2-53925988'

identifiers = {}

for instance in DoGet('%s/studies/%s/instances' % (URL, STUDY)):
    # Setup the parameters of the modification
    replace = { 
        "PatientID" : "Hello",
        "PatientName" : "Modified",
        "StationName" : "TEST",
    }

    # Get the original UIDs of the instance
    seriesUID = DoGet('%s/instances/%s/content/SeriesInstanceUID' % (URL, instance['ID']))
    if seriesUID in identifiers:
        replace['SeriesInstanceUID'] = identifiers[seriesUID]

    studyUID = DoGet('%s/instances/%s/content/StudyInstanceUID' % (URL, instance['ID']))
    if studyUID in identifiers:
        replace['StudyInstanceUID'] = identifiers[studyUID]

    # Manually modify the instance
    print('Modifying instance %s' % instance['ID'])
    modified = DoPost('%s/instances/%s/modify' % (URL, instance['ID']),
                      { "Replace" : replace })

    # Remove the original instance
    DoDelete('%s/instances/%s' % (URL, instance['ID']))

    # Add the modified instance
    modifiedId = DoPost('%s/instances' % URL, modified)['ID']

    # Register the modified UIDs
    identifiers[seriesUID] = DoGet('%s/instances/%s/content/SeriesInstanceUID' % (URL, modifiedId))
    identifiers[studyUID] = DoGet('%s/instances/%s/content/StudyInstanceUID' % (URL, modifiedId))
