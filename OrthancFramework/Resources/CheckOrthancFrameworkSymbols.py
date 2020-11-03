#!/usr/bin/env python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2020 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


##
## This maintenance script detects all the public methods in the
## Orthanc framework that come with an inlined implementation in the
## header file. Such methods can break the ABI of the shared library,
## as the actual implementation might change over versions.
##


# Ubuntu 20.04:
# sudo apt-get install python-clang-6.0
# ./ParseWebAssemblyExports.py --libclang=libclang-6.0.so.1 ./Test.cpp

# Ubuntu 18.04:
# sudo apt-get install python-clang-4.0
# ./ParseWebAssemblyExports.py --libclang=libclang-4.0.so.1 ./Test.cpp

# Ubuntu 14.04:
# ./ParseWebAssemblyExports.py --libclang=libclang-3.6.so.1 ./Test.cpp


import os
import sys
import clang.cindex
import argparse

##
## Parse the command-line arguments
##

parser = argparse.ArgumentParser(description = 'Parse WebAssembly C++ source file, and create a basic JavaScript wrapper.')
parser.add_argument('--libclang',
                    default = '',
                    help = 'manually provides the path to the libclang shared library')

args = parser.parse_args()


if len(args.libclang) != 0:
    clang.cindex.Config.set_library_file(args.libclang)

index = clang.cindex.Index.create()


ROOT = os.path.abspath(os.path.dirname(sys.argv[0]))
SOURCES = []

for root, dirs, files in os.walk(os.path.join(ROOT, '..', 'Sources')):
    for name in files:
        if os.path.splitext(name)[1] == '.h':
            SOURCES.append(os.path.join(root, name))

AMALGAMATION = '/tmp/CheckOrthancFrameworkSymbols.cpp'
            
with open(AMALGAMATION, 'w') as f:
    f.write('#include "%s"\n' % os.path.join(ROOT, '..', 'Sources', 'OrthancFramework.h'))
    for source in SOURCES:
        f.write('#include "%s"\n' % source)
            

tu = index.parse(AMALGAMATION,
                 [ '-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=1' ])



def ExploreNamespace(node, namespace):
    for child in node.get_children():
        fqn = namespace + [ child.spelling ]
        
        if child.kind == clang.cindex.CursorKind.NAMESPACE:
            ExploreNamespace(child, fqn)

        elif (child.kind == clang.cindex.CursorKind.CLASS_DECL or
              child.kind == clang.cindex.CursorKind.STRUCT_DECL):
            visible = False
            
            for i in child.get_children():
                if (i.kind == clang.cindex.CursorKind.VISIBILITY_ATTR and
                    i.spelling == 'default'):
                    visible = True

            if visible:
                isPublic = (child.kind == clang.cindex.CursorKind.STRUCT_DECL)
                
                for i in child.get_children():
                    if i.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                        isPublic = (i.access_specifier == clang.cindex.AccessSpecifier.PUBLIC)
                        
                    elif (i.kind == clang.cindex.CursorKind.CXX_METHOD or
                          i.kind == clang.cindex.CursorKind.CONSTRUCTOR):
                        if isPublic:
                            hasImplementation = False
                            for j in i.get_children():
                                if j.kind == clang.cindex.CursorKind.COMPOUND_STMT:
                                    hasImplementation = True

                            if hasImplementation:
                                print('Exported public method with an implementation: %s::%s()' %
                                      ('::'.join(fqn), i.spelling))


for node in tu.cursor.get_children():
    if (node.kind == clang.cindex.CursorKind.NAMESPACE and
        node.spelling == 'Orthanc'):
        ExploreNamespace(node, [ 'Orthanc' ])
