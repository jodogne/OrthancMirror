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



import os
import os.path
import sys
import RestToolbox

def PrintHelp():
    print('Download ZIP archives for all the studies generated '
          'during a given time range (according to the StudyDate tag)\n')
    print('Usage: %s <URL> <StartDate> <EndDate> <TargetFolder>\n' % sys.argv[0])
    print('Example: %s http://127.0.0.1:8042/ 20150101 20151231 /tmp/\n' % sys.argv[0])
    exit(-1)

def CheckIsDate(date):
    if len(date) != 8 or not date.isdigit():
        print '"%s" is not a valid date!\n' % date
        exit(-1)


if len(sys.argv) != 5:
    PrintHelp()

URL = sys.argv[1]
START = sys.argv[2]
END = sys.argv[3]
TARGET = sys.argv[4]

CheckIsDate(START)
CheckIsDate(END)

def GetTag(tags, key):
    if key in tags:
        return tags[key]
    else:
        return 'No%s' % key

# Loop over the studies
for studyId in RestToolbox.DoGet('%s/studies' % URL):
    # Retrieve the DICOM tags of the current study
    study = RestToolbox.DoGet('%s/studies/%s' % (URL, studyId))['MainDicomTags']

    # Retrieve the DICOM tags of the parent patient of this study
    patient = RestToolbox.DoGet('%s/studies/%s/patient' % (URL, studyId))['MainDicomTags']

    # Check that the StudyDate tag lies within the given range
    studyDate = study['StudyDate'][:8]
    if studyDate >= START and studyDate <= END:
        # Create a filename
        filename = '%s - %s %s - %s.zip' % (GetTag(study, 'StudyDate'),
                                            GetTag(patient, 'PatientID'),
                                            GetTag(patient, 'PatientName'),
                                            GetTag(study, 'StudyDescription'))

        # Remove any non-ASCII character in the filename
        filename = filename.encode('ascii', errors = 'replace').translate(None, r"'\/:*?\"<>|!=").strip()

        # Download the ZIP archive of the study
        print('Downloading %s' % filename)
        zipContent = RestToolbox.DoGet('%s/studies/%s/archive' % (URL, studyId))

        # Write the ZIP archive at the proper location
        with open(os.path.join(TARGET, filename), 'wb') as f:
            f.write(zipContent)
