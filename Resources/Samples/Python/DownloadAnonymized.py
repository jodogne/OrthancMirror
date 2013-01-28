#!/usr/bin/python
# -*- coding: utf-8 -*-


URL = 'http://localhost:8042'

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
        print 'Downloading %s' % name
        zipContent = RestToolbox.DoGet('%s/patients/%s/archive' % (URL, patient))
        f = open(os.path.join('/tmp', name + '.zip'), 'wb')
        f.write(zipContent)
        f.close()
