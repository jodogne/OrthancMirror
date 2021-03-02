===================
GENERAL INFORMATION
===================

This folder contains sample Web applications.

These Web applications make use of NodeJs (http://nodejs.org/). To run
the applications, you therefore need to install NodeJs on your
computer. NodeJs acts here as a lightweight, cross-platform Web server
that statically serves the HTML/JavaScript files and that dynamically
serves the Orthanc REST API as a reverse proxy (to avoid cross-domain
problems with AJAX).

Once NodeJs is installed, start Orthanc with default parameters
(i.e. HTTP port set to 8042), start NodeJs with the sample application
you are interested in (e.g. "node DrawingDicomizer.js"). Then, open
http://localhost:8000/ with a standard Web browser to try the sample
application.



=======================================
DRAWING DICOMIZER (DrawingDicomizer.js)
=======================================

This sample shows how to convert the content of a HTML5 canvas as a
DICOM file, using a single AJAX request to Orthanc.

Internally, the content of the HTML5 canvas is serialized through the
standard "toDataURL()" method of the canvas object. This returns a
string containing the PNG image encoded using the Data URI Scheme
(http://en.wikipedia.org/wiki/Data_URI_scheme). Such a string is then
sent to Orthanc using the '/tools/create-dicom' REST call, that
transparently decompresses the PNG image into a DICOM image.
