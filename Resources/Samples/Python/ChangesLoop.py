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


##
## Print help message
##

if len(sys.argv) != 3:
    print("""
Sample script that continuously monitors the arrival of new DICOM
images into Orthanc (through the Changes API).

Usage: %s [hostname] [HTTP port]
For instance: %s 127.0.0.1 8042
""" % (sys.argv[0], sys.argv[0]))
    exit(-1)

URL = 'http://%s:%d' % (sys.argv[1], int(sys.argv[2]))



##
## The following function is called each time a new instance is
## received.
##

def NewInstanceReceived(path):
    global URL
    patientName = RestToolbox.DoGet(URL + path + '/content/PatientName')
    
    # Remove the possible trailing characters due to DICOM padding
    patientName = patientName.strip()

    print('New instance received for patient "%s": "%s"' % (patientName, path))



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
        # We are only interested interested in the arrival of new instances
        if change['ChangeType'] == 'NewInstance':
            # Call the callback function
            path = change['Path']
            NewInstanceReceived(path)

            # Delete the instance once it has been discovered
            RestToolbox.DoDelete(URL + path)

    current = r['Last']

    if r['Done']:
        print('Everything has been processed: Waiting...')
        time.sleep(1)
