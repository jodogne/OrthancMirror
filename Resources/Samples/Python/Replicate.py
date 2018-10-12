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


import base64
import httplib2
import json
import re
import sys

URL_REGEX = re.compile('(http|https)://((.+?):(.+?)@|)(.*)')


if len(sys.argv) != 3:
    print("""
Script to copy the content of one Orthanc server to another Orthanc
server through their REST API.

Usage: %s [SourceURI] [TargetURI]
For instance: %s http://orthanc:password@127.0.0.1:8042/ http://127.0.0.1:8043/
""" % (sys.argv[0], sys.argv[0]))
    exit(-1)



def CreateHeaders(parsedUrl):
    headers = { }
    username = parsedUrl.group(3)
    password = parsedUrl.group(4)

    if username != None and password != None:
        # This is a custom reimplementation of the
        # "Http.add_credentials()" method for Basic HTTP Access
        # Authentication (for some weird reason, this method does not
        # always work)
        # http://en.wikipedia.org/wiki/Basic_access_authentication
        headers['authorization'] = 'Basic ' + base64.b64encode(username + ':' + password)

    return headers


def GetBaseUrl(parsedUrl):
    return '%s://%s' % (parsedUrl.group(1), parsedUrl.group(5))


def DoGetString(url):
    global URL_REGEX
    parsedUrl = URL_REGEX.match(url)
    headers = CreateHeaders(parsedUrl)

    h = httplib2.Http()
    resp, content = h.request(GetBaseUrl(parsedUrl), 'GET', headers = headers)

    if resp.status == 200:
        return content
    else:
        raise Exception('Unable to contact Orthanc at: ' + url)
    

def DoPostDicom(url, body):
    global URL_REGEX
    parsedUrl = URL_REGEX.match(url)
    headers = CreateHeaders(parsedUrl)
    headers['content-type'] = 'application/dicom'

    h = httplib2.Http()
    resp, content = h.request(GetBaseUrl(parsedUrl), 'POST',
                              body = body,
                              headers = headers)

    if resp.status != 200:
        raise Exception('Unable to contact Orthanc at: ' + url)
    

def _DecodeJson(s):
    if (sys.version_info >= (3, 0)):
        return json.loads(s.decode())
    else:
        return json.loads(s)


def DoGetJson(url):
    return _DecodeJson(DoGetString(url))


SOURCE = sys.argv[1]
TARGET = sys.argv[2]

for study in DoGetJson('%s/studies' % SOURCE):
    print('Sending study %s...' % study)
    for instance in DoGetJson('%s/studies/%s/instances' % (SOURCE, study)):
        dicom = DoGetString('%s/instances/%s/file' % (SOURCE, instance['ID']))
        DoPostDicom('%s/instances' % TARGET, dicom)
