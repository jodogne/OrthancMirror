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


import httplib2
import json
import sys

if (sys.version_info >= (3, 0)):
    from urllib.parse import urlencode
else:
    from urllib import urlencode


_credentials = None


def _DecodeJson(s):
    try:
        if (sys.version_info >= (3, 0)):
            return json.loads(s.decode())
        else:
            return json.loads(s)
    except:
        return s


def SetCredentials(username, password):
    global _credentials
    _credentials = (username, password)

def _SetupCredentials(h):
    global _credentials
    if _credentials != None:
        h.add_credentials(_credentials[0], _credentials[1])

def DoGet(uri, data = {}, interpretAsJson = True):
    d = ''
    if len(data.keys()) > 0:
        d = '?' + urlencode(data)

    h = httplib2.Http()
    _SetupCredentials(h)
    resp, content = h.request(uri + d, 'GET')
    if not (resp.status in [ 200 ]):
        raise Exception(resp.status)
    elif not interpretAsJson:
        return content.decode()
    else:
        return _DecodeJson(content)


def _DoPutOrPost(uri, method, data, contentType):
    h = httplib2.Http()
    _SetupCredentials(h)

    if isinstance(data, str):
        body = data
        if len(contentType) != 0:
            headers = { 'content-type' : contentType }
        else:
            headers = { 'content-type' : 'text/plain' }
    else:
        body = json.dumps(data)
        headers = { 'content-type' : 'application/json' }
    
    resp, content = h.request(
        uri, method,
        body = body,
        headers = headers)

    if not (resp.status in [ 200, 302 ]):
        raise Exception(resp.status)
    else:
        return _DecodeJson(content)


def DoDelete(uri):
    h = httplib2.Http()
    _SetupCredentials(h)
    resp, content = h.request(uri, 'DELETE')

    if not (resp.status in [ 200 ]):
        raise Exception(resp.status)
    else:
        return _DecodeJson(content)


def DoPut(uri, data = {}, contentType = ''):
    return _DoPutOrPost(uri, 'PUT', data, contentType)


def DoPost(uri, data = {}, contentType = ''):
    return _DoPutOrPost(uri, 'POST', data, contentType)
