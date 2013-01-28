#!/usr/bin/python
# -*- coding: utf-8 -*-


URL = 'http://localhost:8042'

#
# This sample code will anonymize all the patients that are stored in
# Orthanc.
#

import sys
import RestToolbox

# Loop over the patients
for patient in RestToolbox.DoGet('%s/patients' % URL):

    # Ignore patients whose name starts with "Anonymized", as it is
    # the result of a previous anonymization
    infos = RestToolbox.DoGet('%s/patients/%s' % (URL, patient))
    name = infos['MainDicomTags']['PatientName'].lower()
    if not name.startswith('anonymized'):

        # Trigger the anonymization
        RestToolbox.DoPost('%s/patients/%s/anonymize' % (URL, patient))
