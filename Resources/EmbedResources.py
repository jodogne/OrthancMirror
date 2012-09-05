import sys
import os
import os.path
import pprint

if len(sys.argv) < 2 or len(sys.argv) % 2 != 0:
    print ('Usage:')
    print ('python %s <TargetBaseFilename> [ <Name> <Source> ]*' % sys.argv[0])
    exit(-1)


try:
    # Make sure the destination directory exists
    os.makedirs(os.path.normpath(os.path.join(sys.argv[1], '..')))
except:
    pass


#####################################################################
## Read each resource file
#####################################################################

resources = {}

counter = 0
i = 2
while i < len(sys.argv):
    resourceName = sys.argv[i].upper()
    pathName = sys.argv[i + 1]

    if not os.path.exists(pathName):
        raise Exception("Non existing path: %s" % pathName)

    if resourceName in resources:
        raise Exception("Twice the same resource: " + resourceName)
    
    if os.path.isdir(pathName):
        # The resource is a directory: Recursively explore its files
        content = {}
        for root, dirs, files in os.walk(pathName):
            base = os.path.relpath(root, pathName)
            for f in files:
                if f.find('~') == -1:  # Ignore Emacs backup files
                    if base == '.':
                        r = f.lower()
                    else:
                        r = os.path.join(base, f).lower()

                    r = '/' + r.replace('\\', '/')
                    if r in content:
                        raise Exception("Twice the same filename (check case): " + r)

                    content[r] = {
                        'Filename' : os.path.join(root, f),
                        'Index' : counter
                        }
                    counter += 1

        resources[resourceName] = {
            'Type' : 'Directory',
            'Files' : content
            }

    elif os.path.isfile(pathName):
        resources[resourceName] = {
            'Type' : 'File',
            'Index' : counter,
            'Filename' : pathName
            }
        counter += 1

    else:
        raise Exception("Not a regular file, nor a directory: " + pathName)

    i += 2

#pprint.pprint(resources)


#####################################################################
## Write .h header
#####################################################################

header = open(sys.argv[1] + '.h', 'w')

header.write("""
#pragma once

#include <string>

namespace Palanthir
{
  namespace EmbeddedResources
  {
    enum FileResourceId
    {
""")

isFirst = True
for name in resources:
    if resources[name]['Type'] == 'File':
        if isFirst:
            isFirst = False
        else:    
            header.write(',\n')
        header.write('      %s' % name)

header.write("""
    };

    enum DirectoryResourceId
    {
""")

isFirst = True
for name in resources:
    if resources[name]['Type'] == 'Directory':
        if isFirst:
            isFirst = False
        else:    
            header.write(',\n')
        header.write('      %s' % name)

header.write("""
    };

    const void* GetFileResourceBuffer(FileResourceId id);
    size_t GetFileResourceSize(FileResourceId id);
    void GetFileResource(std::string& result, FileResourceId id);

    const void* GetDirectoryResourceBuffer(DirectoryResourceId id, const char* path);
    size_t GetDirectoryResourceSize(DirectoryResourceId id, const char* path);
    void GetDirectoryResource(std::string& result, DirectoryResourceId id, const char* path);
  }
}
""")
header.close()



#####################################################################
## Write the resource content in the .cpp source
#####################################################################

def WriteResource(cpp, item):
    cpp.write('    static const uint8_t resource%dBuffer[] = {' % item['Index'])

    f = open(item['Filename'], "rb")
    content = f.read()
    f.close()

    # http://stackoverflow.com/a/1035360
    pos = 0
    for b in content:
        if pos > 0:
            cpp.write(', ')

        if (pos % 16) == 0:
            cpp.write('\n    ')

        if ord(b[0]) < 0:
            raise Exception("Internal error")

        cpp.write("0x%02x" % ord(b[0]))
        pos += 1

    cpp.write('  };\n')
    cpp.write('    static const size_t resource%dSize = %d;\n' % (item['Index'], pos))


cpp = open(sys.argv[1] + '.cpp', 'w')

cpp.write("""
#include "%s.h"
#include "%s/Core/PalanthirException.h"

#include <stdint.h>
#include <string.h>

namespace Palanthir
{
  namespace EmbeddedResources
  {
""" % (os.path.basename(sys.argv[1]),
       os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))))


for name in resources:
    if resources[name]['Type'] == 'File':
        WriteResource(cpp, resources[name])
    else:
        for f in resources[name]['Files']:
            WriteResource(cpp, resources[name]['Files'][f])



#####################################################################
## Write the accessors to the file resources in .cpp
#####################################################################

cpp.write("""
    const void* GetFileResourceBuffer(FileResourceId id)
    {
      switch (id)
      {
""")
for name in resources:
    if resources[name]['Type'] == 'File':
        cpp.write('      case %s:\n' % name)
        cpp.write('        return resource%dBuffer;\n' % resources[name]['Index'])

cpp.write("""
      default:
        throw PalanthirException(ErrorCode_ParameterOutOfRange);
      }
    }

    size_t GetFileResourceSize(FileResourceId id)
    {
      switch (id)
      {
""")

for name in resources:
    if resources[name]['Type'] == 'File':
        cpp.write('      case %s:\n' % name)
        cpp.write('        return resource%dSize;\n' % resources[name]['Index'])

cpp.write("""
      default:
        throw PalanthirException(ErrorCode_ParameterOutOfRange);
      }
    }
""")



#####################################################################
## Write the accessors to the directory resources in .cpp
#####################################################################

cpp.write("""
    const void* GetDirectoryResourceBuffer(DirectoryResourceId id, const char* path)
    {
      switch (id)
      {
""")

for name in resources:
    if resources[name]['Type'] == 'Directory':
        cpp.write('      case %s:\n' % name)
        isFirst = True
        for path in resources[name]['Files']:
            cpp.write('        if (!strcmp(path, "%s"))\n' % path)
            cpp.write('          return resource%dBuffer;\n' % resources[name]['Files'][path]['Index'])
        cpp.write('        throw PalanthirException("Unknown path in a directory resource");\n\n')

cpp.write("""      default:
        throw PalanthirException(ErrorCode_ParameterOutOfRange);
      }
    }

    size_t GetDirectoryResourceSize(DirectoryResourceId id, const char* path)
    {
      switch (id)
      {
""")

for name in resources:
    if resources[name]['Type'] == 'Directory':
        cpp.write('      case %s:\n' % name)
        isFirst = True
        for path in resources[name]['Files']:
            cpp.write('        if (!strcmp(path, "%s"))\n' % path)
            cpp.write('          return resource%dSize;\n' % resources[name]['Files'][path]['Index'])
        cpp.write('        throw PalanthirException("Unknown path in a directory resource");\n\n')

cpp.write("""      default:
        throw PalanthirException(ErrorCode_ParameterOutOfRange);
      }
    }
""")




#####################################################################
## Write the convenience wrappers in .cpp
#####################################################################

cpp.write("""
    void GetFileResource(std::string& result, FileResourceId id)
    {
      size_t size = GetFileResourceSize(id);
      result.resize(size);
      if (size > 0)
        memcpy(&result[0], GetFileResourceBuffer(id), size);
    }

    void GetDirectoryResource(std::string& result, DirectoryResourceId id, const char* path)
    {
      size_t size = GetDirectoryResourceSize(id, path);
      result.resize(size);
      if (size > 0)
        memcpy(&result[0], GetDirectoryResourceBuffer(id, path), size);
    }
  }
}
""")
cpp.close()
