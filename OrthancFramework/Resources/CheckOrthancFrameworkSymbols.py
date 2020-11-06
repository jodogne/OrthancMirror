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
            

tu = index.parse(AMALGAMATION, [
    '-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=1',
    '-DORTHANC_BUILD_UNIT_TESTS=0',
    '-DORTHANC_SANDBOXED=0',
    '-DORTHANC_ENABLE_BASE64=1',
    '-DORTHANC_ENABLE_CIVETWEB=1',
    '-DORTHANC_ENABLE_CURL=1',
    '-DORTHANC_ENABLE_DCMTK=1',
    '-DORTHANC_ENABLE_DCMTK_JPEG=1',
    '-DORTHANC_ENABLE_DCMTK_NETWORKING=1',
    '-DORTHANC_ENABLE_DCMTK_TRANSCODING=1',
    '-DORTHANC_ENABLE_JPEG=1',
    '-DORTHANC_ENABLE_LOCALE=1',
    '-DORTHANC_ENABLE_LUA=1',
    '-DORTHANC_ENABLE_LOGGING=1',
    '-DORTHANC_ENABLE_MD5=1',
    '-DORTHANC_ENABLE_PKCS11=1',
    '-DORTHANC_ENABLE_PNG=1',
    '-DORTHANC_ENABLE_PUGIXML=1',
    '-DORTHANC_ENABLE_SSL=1',
    '-DORTHANC_SQLITE_STANDALONE=0',
    '-DORTHANC_ENABLE_LOGGING_STDIO=0',
])


FILES = []
COUNT = 0

def ReportProblem(message, fqn, cursor):
    global FILES, COUNT
    FILES.append(os.path.normpath(str(cursor.location.file)))
    COUNT += 1

    print('%s: %s::%s()' % (message, '::'.join(fqn), cursor.spelling))


def ExploreClass(child, fqn):
    # Safety check
    if (child.kind != clang.cindex.CursorKind.CLASS_DECL and
        child.kind != clang.cindex.CursorKind.STRUCT_DECL):
        raise Exception()

    # Ignore forward declaration of classes
    if not child.is_definition():
        return
    

    ##
    ## Verify that the class is publicly exported (its visibility must
    ## be "default")
    ##
    visible = False

    for i in child.get_children():
        if (i.kind == clang.cindex.CursorKind.VISIBILITY_ATTR and
            i.spelling == 'default'):
            visible = True

    if not visible:
        return

    
    ##
    ## Ignore pure abstract interfaces, by checking the following
    ## criteria:
    ##   - It must be a C++ class (not a struct)
    ##   - The class name must start with "I"
    ##   - All its methods must be pure virtual (abstract) and public
    ##   - Its destructor must be public, virtual, and must do nothing
    ##
    
    if (child.kind == clang.cindex.CursorKind.CLASS_DECL and
        fqn[-1].startswith('I')):
        abstract = True
        isPublic = False

        for i in child.get_children():
            if i.kind == clang.cindex.CursorKind.VISIBILITY_ATTR:      # "default"
                pass
            elif i.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                isPublic = (i.access_specifier == clang.cindex.AccessSpecifier.PUBLIC)
            elif i.kind == clang.cindex.CursorKind.CXX_BASE_SPECIFIER:
                if i.spelling != 'boost::noncopyable':
                    abstract = False
            elif isPublic:
                if i.kind == clang.cindex.CursorKind.CXX_METHOD:
                    if i.is_pure_virtual_method():
                        pass  # pure virtual is ok
                    elif i.is_static_method():
                        # static method without an inline implementation is ok
                        for j in i.get_children():
                            if j.kind == clang.cindex.CursorKind.COMPOUND_STMT:
                                abstract = False
                    else:
                        abstract = False
                elif (i.kind == clang.cindex.CursorKind.DESTRUCTOR and
                      i.is_virtual_method()):
                    # The destructor must be virtual, and must do nothing
                    c = list(i.get_children())
                    if (len(c) != 1 or
                        c[0].kind != clang.cindex.CursorKind.COMPOUND_STMT or
                        len(list(c[0].get_children())) != 0):
                        abstract = False
                elif (i.kind == clang.cindex.CursorKind.CLASS_DECL or
                      i.kind == clang.cindex.CursorKind.STRUCT_DECL):
                    ExploreClass(i, fqn + [ i.spelling ])
                else:
                    abstract = False

        if abstract:
            print('Detected a pure interface (this is fine): %s' % ('::'.join(fqn)))
            return


    ##
    ## We are facing a standard C++ class or struct
    ##
    
    isPublic = (child.kind == clang.cindex.CursorKind.STRUCT_DECL)

    for i in child.get_children():
        if (i.kind == clang.cindex.CursorKind.VISIBILITY_ATTR or    # "default"
            i.kind == clang.cindex.CursorKind.CXX_BASE_SPECIFIER):  # base class
            pass
        
        elif i.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
            isPublic = (i.access_specifier == clang.cindex.AccessSpecifier.PUBLIC)

        elif (i.kind == clang.cindex.CursorKind.CLASS_DECL or
              i.kind == clang.cindex.CursorKind.STRUCT_DECL):
            # This is a subclass
            if isPublic:
                ExploreClass(i, fqn + [ i.spelling ])

        elif (i.kind == clang.cindex.CursorKind.CXX_METHOD or
              i.kind == clang.cindex.CursorKind.CONSTRUCTOR or
              i.kind == clang.cindex.CursorKind.DESTRUCTOR):
            if isPublic:
                hasImplementation = False
                for j in i.get_children():
                    if j.kind == clang.cindex.CursorKind.COMPOUND_STMT:
                        hasImplementation = True

                if hasImplementation:
                    ReportProblem('Exported public method with an implementation', fqn, i)

        elif i.kind == clang.cindex.CursorKind.VAR_DECL:
            if isPublic:
                ReportProblem('Exported public member variable', fqn, i)

        elif i.kind == clang.cindex.CursorKind.FUNCTION_TEMPLATE:
            if isPublic:
                ReportProblem('Exported public template method', fqn, i)

        elif i.kind == clang.cindex.CursorKind.FRIEND_DECL:
            if isPublic:
                ReportProblem('Exported public friend method', fqn, i)
            
        elif (i.kind == clang.cindex.CursorKind.TYPEDEF_DECL or  # Allow "typedef"
              i.kind == clang.cindex.CursorKind.ENUM_DECL):      # Allow enums
            pass
            
        else:
            if isPublic:
                raise Exception('Unsupported: %s, %s' % (i.kind, i.location))


def ExploreNamespace(node, namespace):
    for child in node.get_children():
        fqn = namespace + [ child.spelling ]
        
        if child.kind == clang.cindex.CursorKind.NAMESPACE:
            ExploreNamespace(child, fqn)

        elif (child.kind == clang.cindex.CursorKind.CLASS_DECL or
              child.kind == clang.cindex.CursorKind.STRUCT_DECL):
            ExploreClass(child, fqn)



print('')

for node in tu.cursor.get_children():
    if (node.kind == clang.cindex.CursorKind.NAMESPACE and
        node.spelling == 'Orthanc'):
        ExploreNamespace(node, [ 'Orthanc' ])


print('\nTotal of possibly problematic methods: %d' % COUNT)

print('\nProblematic files:\n')
for i in sorted(list(set(FILES))):
    print(i)

print('')
