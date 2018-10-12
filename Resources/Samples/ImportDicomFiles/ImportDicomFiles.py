#!/usr/bin/env python

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
import sys
import os.path
import httplib2
import base64

if len(sys.argv) != 4 and len(sys.argv) != 6:
    print("""
Sample script to recursively import in Orthanc all the DICOM files
that are stored in some path. Please make sure that Orthanc is running
before starting this script. The files are uploaded through the REST
API.

Usage: %s [hostname] [HTTP port] [path]
Usage: %s [hostname] [HTTP port] [path] [username] [password]
For instance: %s 127.0.0.1 8042 .
""" % (sys.argv[0], sys.argv[0], sys.argv[0]))
    exit(-1)

URL = 'http://%s:%d/instances' % (sys.argv[1], int(sys.argv[2]))

success_count = 0
total_file_count = 0


# This function will upload a single file to Orthanc through the REST API
def UploadFile(path):
    global success_count
    global total_file_count

    f = open(path, "rb")
    content = f.read()
    f.close()
    total_file_count += 1

    try:
        sys.stdout.write("Importing %s" % path)

        h = httplib2.Http()

        headers = { 'content-type' : 'application/dicom' }

        if len(sys.argv) == 6:
            username = sys.argv[4]
            password = sys.argv[5]

            # h.add_credentials(username, password)

            # This is a custom reimplementation of the
            # "Http.add_credentials()" method for Basic HTTP Access
            # Authentication (for some weird reason, this method does
            # not always work)
            # http://en.wikipedia.org/wiki/Basic_access_authentication
            headers['authorization'] = 'Basic ' + base64.b64encode(username + ':' + password)       

        resp, content = h.request(URL, 'POST', 
                                  body = content,
                                  headers = headers)

        if resp.status == 200:
            sys.stdout.write(" => success\n")
            success_count += 1
        else:
            sys.stdout.write(" => failure (Is it a DICOM file? Is there a password?)\n")

    except:
        sys.stdout.write(" => unable to connect (Is Orthanc running? Is there a password?)\n")


if os.path.isfile(sys.argv[3]):
    # Upload a single file
    UploadFile(sys.argv[3])
else:
    # Recursively upload a directory
    for root, dirs, files in os.walk(sys.argv[3]):
        for f in files:
            UploadFile(os.path.join(root, f))

if success_count == total_file_count:
    print("\nSummary: all %d DICOM file(s) have been imported successfully" % success_count)
else:
    print("\nSummary: %d out of %d files have been imported successfully as DICOM instances" % (success_count, total_file_count))
