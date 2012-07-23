Introduction
============

The code in this folder is a standalone object-oriented wrapper around
SQLite3. It is derived from the code of Chromium:

http://src.chromium.org/viewvc/chrome/trunk/src/sql/
http://maxradi.us/documents/sqlite/


Main differences with Chromium
==============================

* The reference counting mechanism has been reimplemented to make it 
  simpler.
* The PalantirException class is used for the exception mechanisms.
* A statement is always valid (is_valid() always return true).
* The classes and the methods have been renamed to meet Palantir's
  coding conventions.


Licensing
=========

The code in this folder is licensed under the 3-clause BSD license, in
order to respect the original license of the code.

It is pretty straightforward to extract the code from this folder and
to include it in another project.
