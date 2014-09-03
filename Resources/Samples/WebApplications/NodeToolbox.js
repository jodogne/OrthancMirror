/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


var fs = require('fs');
var http = require('http');


function ForwardGetRequest(orthanc, path, res) {
  var opts = orthanc;
  opts.path = path;
  opts.method = 'GET';

  http.get(opts, function(response) {
    if (response.statusCode == 200) {
      response.setEncoding('utf-8');
      response.on('data', function(chunk) {
        res.write(chunk);
      });
      response.on('end', function() {
        res.end();
      });
    } else {
      console.log('Got error on GET forwarding: ' + 
                  response.statusCode + ' (' + path + ')');
      res.writeHead(response.statusCode);
      res.end();
    }
  }).on('error', function(e) {
    console.log('Unable to contact Orthanc: ' + e.message);
    res.writeHead(503);  // Service Unavailable
    res.end();
  });
}


function ForwardPostRequest(orthanc, path, body, res) {
  var opts = orthanc;
  opts.path = path;
  opts.method = 'POST';
  opts.headers = {
    'Content-Length': body.length
  }

  var req = http.request(opts, function(response) {
    if (response.statusCode == 200) {
      response.setEncoding('utf-8');
      response.on('data', function(chunk) {
        res.write(chunk);
      });
      response.on('end', function() {
        res.end();
      });
    } else {
      console.log('Got error on POST forwarding: ' + 
                  response.statusCode + ' (' + path + ')');
      res.writeHead(response.statusCode);
      res.end();
    }
  }).on('error', function(e) {
    console.log('Unable to contact Orthanc: ' + e.message);
    res.writeHead(503);  // Service Unavailable
    res.end();
  });

  req.write(body);
  req.end();
}


function ServeFile(filename, res) {
  fs.readFile(filename, function(r, c) {
    res.end(c.toString());
  });
}


function NotFound(res) {
  res.writeHead(404, {'Content-Type': 'text/plain'});
  res.end();
}


function Redirect(path, res) {
  res.writeHead(301, {
    'Content-Type': 'text/plain',
    'Location': path
  });
  res.end();
}


String.prototype.startsWith = function(prefix) {
  return this.indexOf(prefix) === 0;
}


module.exports.ForwardGetRequest = ForwardGetRequest;
module.exports.ForwardPostRequest = ForwardPostRequest;
module.exports.NotFound = NotFound;
module.exports.Redirect = Redirect;
module.exports.ServeFile = ServeFile;
