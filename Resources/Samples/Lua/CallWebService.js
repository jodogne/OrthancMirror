/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
