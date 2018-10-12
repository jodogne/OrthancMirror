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



import time
import sys
import RestToolbox
import md5


##
## Print help message
##

if len(sys.argv) != 3:
    print("""
Sample script that anonymizes patients in real-time. A patient gets
anonymized as soon as she gets stable (i.e. when no DICOM instance has
been received for this patient for a sufficient amount of time - cf.
the configuration option "StableAge").

Usage: %s [hostname] [HTTP port]
For instance: %s 127.0.0.1 8042
""" % (sys.argv[0], sys.argv[0]))
    exit(-1)

URL = 'http://%s:%d' % (sys.argv[1], int(sys.argv[2]))



##
## The following function is called whenever a patient gets stable
##

COUNT = 1

def AnonymizePatient(path):
    global URL
    global COUNT

    patient = RestToolbox.DoGet(URL + path)
    patientID = patient['MainDicomTags']['PatientID']

    # Ignore anonymized patients
    if not 'AnonymizedFrom' in patient:
        print('Patient with ID "%s" is stabilized: anonymizing it...' % (patientID))
        
        # The PatientID after anonymization is taken as the 8 first
        # characters from the MD5 hash of the original PatientID
        anonymizedID = md5.new(patientID).hexdigest()[:8]
        anonymizedName = 'Anonymized patient %d' % COUNT
        COUNT += 1

        RestToolbox.DoPost(URL + path + '/anonymize',
                           { 'Replace' : { 'PatientID' : anonymizedID,
                                           'PatientName' : anonymizedName } })

        # Delete the source patient after the anonymization
        RestToolbox.DoDelete(URL + change['Path'])



##
## Main loop that listens to the changes API.
## 

current = 0
while True:
    r = RestToolbox.DoGet(URL + '/changes', {
            'since' : current,
            'limit' : 4   # Retrieve at most 4 changes at once
            })

    for change in r['Changes']:
        if change['ChangeType'] == 'StablePatient':
            AnonymizePatient(change['Path'])

    current = r['Last']

    if r['Done']:
        print('Everything has been processed: Waiting...')
        time.sleep(1)
