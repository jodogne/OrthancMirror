#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2018 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# In addition, as a special exception, the copyright holders of this
# program give permission to link the code of its release with the
# OpenSSL project's "OpenSSL" library (or with modified versions of it
# that use the same license as the "OpenSSL" library), and distribute
# the linked executables. You must obey the GNU General Public License
# in all respects for all of the code used other than "OpenSSL". If you
# modify file(s) with this exception, you may extend this exception to
# your version of the file(s), but you are not obligated to do so. If
# you do not wish to do so, delete this exception statement from your
# version. If you delete this exception statement from all source files
# in the program, then also delete it here.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.




# sudo pip install freetype-py



import freetype
import json
import os
import sys
import unicodedata


if len(sys.argv) != 3:
    print('Usage: %s <Font> <Size>\n' % sys.argv[0])
    print('Example: %s /usr/share/fonts/truetype/ubuntu-font-family/UbuntuMono-B.ttf 16\n' % sys.argv[0])
    sys.exit(-1)



FONT = sys.argv[1]
PIXEL_SIZE = int(sys.argv[2])
CHARSET = 'latin-1'


# Load the font
face = freetype.Face(FONT)
face.set_char_size(PIXEL_SIZE * 64)

# Generate all the characters between 0 and 255
characters = ''.join(map(chr, range(0, 256)))

# Interpret the string using the required charset
characters = characters.decode(CHARSET, 'ignore')

# Keep only non-control characters
characters = filter(lambda c: unicodedata.category(c)[0] != 'C', characters)

font = {
    'Name' : os.path.basename(FONT),
    'Size' : PIXEL_SIZE,
    'Characters' : {}
}


def PrintCharacter(c):
    pos = 0
    for i in range(c['Height']):
        s = ''
        for j in range(c['Width']):
            if c['Bitmap'][pos] > 127:
                s += '*'
            else:
                s += ' '
            pos += 1
        print s


for c in characters:
    face.load_char(c)

    info = {
        'Width' : face.glyph.bitmap.width,
        'Height' : face.glyph.bitmap.rows,
        'Advance' : face.glyph.metrics.horiAdvance / 64,
        'Top' : -face.glyph.metrics.horiBearingY / 64,
        'Bitmap' : face.glyph.bitmap.buffer,
    }

    font['Characters'][ord(c)] = info

    #PrintCharacter(info)

minTop = min(map(lambda (k, v): v['Top'], font['Characters'].iteritems()))
for c in font['Characters']:
    font['Characters'][c]['Top'] -= minTop

font['MaxAdvance'] = max(map(lambda (k, v): v['Advance'], font['Characters'].iteritems()))
font['MaxHeight'] = max(map(lambda (k, v): v['Height'], font['Characters'].iteritems()))

print json.dumps(font)
