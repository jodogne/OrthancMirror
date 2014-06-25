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



/**
 * Parameters of the HTTP server.
 **/

var orthanc = { 
  host: 'localhost',
  port: 8042 
};

var port = 8000;



/**
 * The Web application.
 **/

var http = require('http');
var querystring = require('querystring');
var toolbox = require('./NodeToolbox.js');

var server = http.createServer(function(req, response) {
  switch (req.method)
  {
  case 'GET':
    {
      if (req.url == '/') {
        toolbox.Redirect('/index.html', response);
      }
      else if (req.url == '/index.html') {
        toolbox.ServeFile('DrawingDicomizer/index.html', response);
      }
      else if (req.url == '/drawing.js') {
        toolbox.ServeFile('DrawingDicomizer/drawing.js', response);
      }
      else if (req.url == '/orthanc.js') {
        toolbox.ServeFile('DrawingDicomizer/orthanc.js', response);
      }
      else if (req.url == '/jquery.js') {
        toolbox.ServeFile('../../../OrthancExplorer/libs/jquery-1.7.2.min.js', response);
      }
      else if (req.url.startsWith('/orthanc')) {
        toolbox.ForwardGetRequest(orthanc, req.url.substr(8), response);
      }
      else {
        toolbox.NotFound(response);
      }

      break;
    }

  case 'POST':
    {
      var body = '';

      req.on('data', function (data) {
        body += data;
      });

      req.on('end', function () {
        if (req.url == '/orthanc/tools/create-dicom') {
          body = JSON.stringify(querystring.parse(body));
          toolbox.ForwardPostRequest(orthanc, '/tools/create-dicom', body, response);
        }
        else {
          toolbox.NotFound(response);
        }
      });

      break;
    }

  default:
    toolbox.NotFound(response);
  }
});

server.listen(port);
