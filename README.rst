Orthanc - A Lightweight, RESTful DICOM Server
=============================================


General Information
-------------------

General information about this software can be found on its official
Website, and in the Orthanc Book:
http://www.orthanc-server.com/
http://book.orthanc-server.com/

The instructions for building Orthanc can be found in the "./INSTALL"
file.


Supported Platforms
-------------------

Currently, the officially validated platforms are:

* GNU/Linux (32bit and 64bit).
* Windows (32bit and 64bit).
* Apple OS X (32bit and 64bit).

Orthanc is known to work on other UNIX-like platforms (such as FreeBSD
and OpenBSD).


Supported Toolchains
--------------------

Orthanc can currently be built using the following compiling
toolchains:

* Native GNU/Linux compilation, with gcc.
* Native Windows compilation, with Microsoft Visual Studio.
* Cross-compilation for Windows under GNU/Linux, with MinGW.


Licensing
---------

Orthanc is licensed under the GPLv3 license, with the OpenSSL
exception:
http://people.gnome.org/~markmc/openssl-and-the-gpl.html

Full information about the licensing of the Orthanc ecosystem is
available in the Orthanc Book:
https://book.orthanc-server.com/faq/licensing.html

We also kindly ask scientific works and clinical studies that make
use of Orthanc to cite Orthanc in their associated publications.
Similarly, we ask open-source and closed-source products that make
use of Orthanc to warn us about this use. You can cite our work
using the following BibTeX entry:

@Article{Jodogne2018,
  author="Jodogne, S{\'e}bastien",
  title="The {O}rthanc Ecosystem for Medical Imaging",
  journal="Journal of Digital Imaging",
  year="2018",
  month="Jun",
  day="01",
  volume="31",
  number="3",
  pages="341--352",
  issn="1618-727X",
  doi="10.1007/s10278-018-0082-y",
  url="https://doi.org/10.1007/s10278-018-0082-y"
}


Licensing of special directories
--------------------------------

The following directories have separate licensing terms:

* The files of the "./OrthancFramework/Sources/" directory are
  licensed under the LGPLv3 (Lesser GPL) since Orthanc 1.7.2. The
  related COPYING file can be found in "./OrthancFramework".

* The files of the "./OrthancFramework/Sources/SQLite/" directory are
  licensed under the 3-clause BSD license, as they are derived from
  the Chromium project.


Content
-------

This archive contains the following important directories:

* ./OrthancFramework/Sources/          - The sources of the Orthanc framework
* ./OrthancFramework/UnitTestsSources/ - Unit tests of the Orthanc framework
* ./OrthancServer/OrthancExplorer/     - Code of the Orthanc Explorer (HTML5/Javascript)
* ./OrthancServer/Plugins/             - Code of the plugin framework
* ./OrthancServer/Resources/Samples/   - Samples
* ./OrthancServer/Sources/             - Code of the Orthanc server (depends on DCMTK)
* ./OrthancServer/UnitTestsSources/    - Unit tests of the Orthanc server

This archive contains the following important files:

* ./AUTHORS                       - The list of the authors
* ./COPYING                       - The main GPLv3 license
* ./INSTALL                       - How to build Orthanc
* ./NEWS                          - The history of main changes between versions
* ./OrthancServer/CMakeLists.txt  - The main build script of the Orthanc server
* ./README                        - This file

We have decided not to maintain a separate "ChangeLog" file. Each
commit to the official Orthanc Mercurial repository should be
associated with a description of the changes.
