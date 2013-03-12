#!/usr/bin/python

import os
import sys
import os.path
import httplib2

if len(sys.argv) != 4 and len(sys.argv) != 6:
    print("""
Sample script to recursively import in Orthanc all the DICOM files
that are stored in some path. Please make sure that Orthanc is running
before starting this script. The files are uploaded through the REST
API.

Usage: %s [hostname] [HTTP port] [path]
Usage: %s [hostname] [HTTP port] [path] [username] [password]
For instance: %s localhost 8042 .
""" % (sys.argv[0], sys.argv[0], sys.argv[0]))
    exit(-1)

URL = 'http://%s:%d/instances' % (sys.argv[1], int(sys.argv[2]))

success = 0


# This function will upload a single file to Orthanc through the REST API
def UploadFile(path):
    global success

    f = open(path, "rb")
    content = f.read()
    f.close()

    try:
        sys.stdout.write("Importing %s" % path)

        h = httplib2.Http()

        if len(sys.argv) == 6:
            h.add_credentials(sys.argv[4], sys.argv[5])
    
        resp, content = h.request(URL, 'POST', 
                                  body = content,
                                  headers = { 'content-type' : 'application/dicom' })

        if resp.status == 200:
            sys.stdout.write(" => success\n")
            success += 1
        else:
            sys.stdout.write(" => failure (is it a DICOM file?)\n")

    except:
        sys.stdout.write(" => unable to connect (is there a password?)\n")


if os.path.isfile(sys.argv[3]):
    # Upload a single file
    UploadFile(sys.argv[3])
else:
    # Recursively upload a directory
    for root, dirs, files in os.walk(sys.argv[3]):
        for f in files:
            UploadFile(os.path.join(root, f))
        

print("\nSummary: %d DICOM file(s) have been imported" % success)
