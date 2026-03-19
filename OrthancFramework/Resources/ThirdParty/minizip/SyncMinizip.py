#!/usr/bin/env python3

import os
import requests

ZLIB_VERSION = '1.3.2'
URL = 'https://raw.githubusercontent.com/madler/zlib/refs/tags/v%s/contrib/minizip' % ZLIB_VERSION
TARGET = os.path.join(os.path.dirname(__file__))

for f in [
        'crypt.h',
        'ioapi.c',
        'ioapi.h',
        'skipset.h',
        'unzip.c',
        'unzip.h',
        'zip.c',
        'zip.h',
]:
    r = requests.get('%s/%s' % (URL, f))
    r.raise_for_status()
    with open(os.path.join(TARGET, f), 'wb') as f:
        f.write(r.content)
