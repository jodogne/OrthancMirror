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


import argparse
import time
import os
import os.path
import sys
import RestToolbox

parser = argparse.ArgumentParser(
    description = 'Automated classification of DICOM files from Orthanc.',
    formatter_class = argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument('--host', default = '127.0.0.1',
                    help = 'The host address that runs Orthanc')
parser.add_argument('--port', type = int, default = '8042',
                    help = 'The port number to which Orthanc is listening for the REST API')
parser.add_argument('--target', default = 'OrthancFiles',
                    help = 'The target directory where to store the DICOM files')
parser.add_argument('--all', action = 'store_true',
                    help = 'Replay the entire history on startup (disabled by default)')
parser.set_defaults(all = False)
parser.add_argument('--remove', action = 'store_true',
                    help = 'Remove DICOM files from Orthanc once classified (disabled by default)')
parser.set_defaults(remove = False)


def FixPath(p):
    return p.encode('ascii', errors = 'replace').translate(None, r"'\/:*?\"<>|!=").strip()

def GetTag(resource, tag):
    if ('MainDicomTags' in resource and
        tag in resource['MainDicomTags']):
        return resource['MainDicomTags'][tag]
    else:
        return 'No' + tag

def ClassifyInstance(instanceId):
    # Extract the patient, study, series and instance information
    instance = RestToolbox.DoGet('%s/instances/%s' % (URL, instanceId))
    series = RestToolbox.DoGet('%s/series/%s' % (URL, instance['ParentSeries']))
    study = RestToolbox.DoGet('%s/studies/%s' % (URL, series['ParentStudy']))
    patient = RestToolbox.DoGet('%s/patients/%s' % (URL, study['ParentPatient']))

    # Construct a target path
    a = '%s - %s' % (GetTag(patient, 'PatientID'),
                     GetTag(patient, 'PatientName'))
    b = GetTag(study, 'StudyDescription')
    c = '%s - %s' % (GetTag(series, 'Modality'),
                     GetTag(series, 'SeriesDescription'))
    d = '%s.dcm' % GetTag(instance, 'SOPInstanceUID')
    
    p = os.path.join(args.target, FixPath(a), FixPath(b), FixPath(c))
    f = os.path.join(p, FixPath(d))

    # Copy the DICOM file to the target path
    print('Writing new DICOM file: %s' % f)
    
    try:
        os.makedirs(p)
    except:
        # Already existing directory, ignore the error
        pass
    
    dcm = RestToolbox.DoGet('%s/instances/%s/file' % (URL, instanceId))
    with open(f, 'wb') as g:
        g.write(dcm)


# Parse the arguments
args = parser.parse_args()
URL = 'http://%s:%d' % (args.host, args.port)
print('Connecting to Orthanc on address: %s' % URL)

# Compute the starting point for the changes loop
if args.all:
    current = 0
else:
    current = RestToolbox.DoGet(URL + '/changes?last')['Last']

# Polling loop using the 'changes' API of Orthanc, waiting for the
# incoming of new DICOM files
while True:
    r = RestToolbox.DoGet(URL + '/changes', {
            'since' : current,
            'limit' : 4   # Retrieve at most 4 changes at once
            })

    for change in r['Changes']:
        # We are only interested interested in the arrival of new instances
        if change['ChangeType'] == 'NewInstance':
            try:
                ClassifyInstance(change['ID'])

                # If requested, remove the instance once it has been
                # properly handled by "ClassifyInstance()". Thanks to
                # the "try/except" block, the instance is not removed
                # if the "ClassifyInstance()" function fails.
                if args.remove:
                    RestToolbox.DoDelete('%s/instances/%s' % (URL, change['ID']))

            except:
                print('Unable to write instance %s to the disk' % change['ID'])

    current = r['Last']

    if r['Done']:
        print('Everything has been processed: Waiting...')
        time.sleep(1)
