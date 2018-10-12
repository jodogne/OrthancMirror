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


/**
 * This file is a simple echo Web service implemented using
 * "node.js". Whenever it receives a POST HTTP query, it echoes its
 * body both to stdout and to the client. Credentials are checked.
 **/


// Parameters of the ECHO server 
var port = 8000;
var username = 'alice';
var password = 'alicePassword';


var http = require('http');
var authorization = 'Basic ' + new Buffer(username + ':' + password).toString('base64')

var server = http.createServer(function(req, response) {
  // Check the credentials
  if (req.headers.authorization != authorization)
  {
    console.log('Bad credentials, access not allowed');
    response.writeHead(401);
    response.end();
    return;
  }

  switch (req.method)
  {
  case 'POST':
    {
      var body = '';

      req.on('data', function (data) {
        response.write(data);
        body += data;
      });

      req.on('end', function () {
        console.log('Message received: ' + body);
        response.end();
      });

      break;
    }

  default:
    console.log('Method ' + req.method + ' is not supported by this ECHO Web service');
    response.writeHead(405, {'Allow': 'POST'});
    response.end();
  }
});

server.listen(port);
