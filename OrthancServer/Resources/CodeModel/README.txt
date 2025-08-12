
Introduction
============

The "./GenerateCodeModel.py" Python script analyzes the header of the
Orthanc Plugin SDK using clang, and saves this code model as a JSON
file.

The code model can then be used to generate wrappers. It is notably
used by the "orthanc-java" (starting with its release 1.0) and
"orthanc-python" plugins (starting with its release 4.3).

NB: This file was originally part of the "orthanc-java" project.


Usage on Ubuntu 22.04 LTS
=========================

Executing with default parameters:

$ sudo apt install python3-clang-14
$ python3 ./GenerateCodeModel.py


Or, if you want to have more control:

$ python3 ./GenerateCodeModel.py \
          --libclang=libclang-14.so.1 \
          --source ../../Plugins/Include/orthanc/OrthancCPlugin.h \
          --target ../../Plugins/Include/orthanc/OrthancPluginCodeModel.json


Generation using Docker
=======================

$ ./docker-run.sh
