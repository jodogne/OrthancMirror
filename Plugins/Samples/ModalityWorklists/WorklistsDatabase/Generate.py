#!/usr/bin/python

import os
import subprocess

SOURCE = '/home/jodogne/Downloads/dcmtk-3.6.0/dcmwlm/data/wlistdb/OFFIS/'
TARGET = os.path.abspath(os.path.dirname(__file__))

for f in os.listdir(SOURCE):
    ext = os.path.splitext(f)

    if ext[1].lower() == '.dump':
        subprocess.check_call([
            'dump2dcm',
            '-g',
            '-q',
            os.path.join(SOURCE, f),
            os.path.join(TARGET, ext[0].lower() + '.wl'),
        ])
