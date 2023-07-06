#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


import sys

if len(sys.argv) != 2:
    raise Exception('Bad number of arguments in %s' % sys.argv[0])

with open(sys.argv[1], 'r') as f:
    s = f.read()

s = s.replace('__FILE__', '__ORTHANC_FILE__')

s = """
#if !defined(__ORTHANC_FILE__)
#  define __ORTHANC_FILE__ __FILE__
#endif
""" + s

with open(sys.argv[1], 'w') as f:
    f.write(s)
