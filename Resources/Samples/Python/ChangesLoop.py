#!/usr/bin/python

import time
import sys
import RestToolbox

if len(sys.argv) != 3:
    print("""
Sample script that continuously monitors the arrival of new DICOM
images into Orthanc (through the Changes API).

Usage: %s [hostname] [HTTP port]
For instance: %s localhost 8042
""" % (sys.argv[0], sys.argv[0]))
    exit(-1)

URL = 'http://%s:%d' % (sys.argv[1], int(sys.argv[2]))


def NewInstanceReceived(path):
    print 'New instance received: "%s"' % path


URL = 'http://localhost:8042'
current = 0
while True:
    r = RestToolbox.DoGet(URL + '/changes', {
            'since' : current,
            'limit' : 4 
            })

    for change in r['Changes']:
        if change['ChangeType'] == 'NewInstance':
            path = change['Path']
            NewInstanceReceived(path)
            RestToolbox.DoDelete(URL + path)

    current = r['Last']

    if r['Done']:
        print "Everything has been processed: Waiting..."
        time.sleep(1)
