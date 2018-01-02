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
        toolbox.ServeFile('../../../OrthancExplorer/libs/jquery.min.js', response);
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


console.log('The demo is running at http://localhost:' + port + '/');
server.listen(port);
