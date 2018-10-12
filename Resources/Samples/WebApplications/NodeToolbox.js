/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
