#!/usr/bin/python
# -*- coding: utf-8 -*-

source = u'TestéäöòДΘĝדصķћ๛ﾈİ'

encodings = {
    'UTF-8' : 'Utf8',
    'ASCII' : 'Ascii',
    'ISO-8859-1' : 'Latin1',
    'ISO-8859-2' : 'Latin2',
    'ISO-8859-3' : 'Latin3',
    'ISO-8859-4' : 'Latin4',
    'ISO-8859-9' : 'Latin5',
    'ISO-8859-5' : 'Cyrillic',
    'WINDOWS-1251' : 'Windows1251',
    'ISO-8859-6' : 'Arabic',
    'ISO-8859-7' : 'Greek',
    'ISO-8859-8' : 'Hebrew',
    'TIS-620' : 'Thai',
    'SHIFT-JIS' : 'Japanese',
    #'GB18030' : 'Chinese',  # Done manually below (*)
}

#from encodings.aliases import aliases
#for a, b in aliases.iteritems():
#    print '%s : %s' % (a, b)


# "63" corresponds to "?"
l = []
encoded = []
expected = []

def ToArray(source):
    result = ''
    for byte in bytearray(source):
        result += '\\x%02x' % byte
    return '"%s"' % result
    

for encoding, orthancEnumeration in encodings.iteritems():
    l.append('::Orthanc::Encoding_%s' % orthancEnumeration)
    s = source.encode(encoding, 'ignore')
    encoded.append(ToArray(s))
    expected.append(ToArray(s.decode(encoding).encode('utf-8')))


# https://en.wikipedia.org/wiki/GB_18030#Technical_details  (*)
l.append('::Orthanc::Encoding_Chinese')
expected.append(ToArray('Þßàáâã'))
encoded.append('"\\x81\\x30\\x89\\x37\\x81\\x30\\x89\\x38\\xA8\\xA4\\xA8\\xA2\\x81\\x30\\x89\\x39\\x81\\x30\\x8A\\x30"')

# Issue 32
# "encoded" is the copy/paste from "dcm2xml +Ca cyrillic Issue32.dcm"
l.append('::Orthanc::Encoding_Windows1251')
encoded.append('"\\xd0\\xe5\\xed\\xf2\\xe3\\xe5\\xed\\xee\\xe3\\xf0\\xe0\\xf4\\xe8\\xff"')
expected.append(ToArray('Рентгенография'))
l.append('::Orthanc::Encoding_Windows1251')
encoded.append('"\\xD2\\xE0\\xE7"')
expected.append(ToArray('Таз'))
l.append('::Orthanc::Encoding_Windows1251')
encoded.append('"\\xcf\\xf0\\xff\\xec\\xe0\\xff"')
expected.append(ToArray('Прямая'))


if True:
    print 'static const unsigned int testEncodingsCount = %d;' % len(l)
    print 'static const ::Orthanc::Encoding testEncodings[] = {\n  %s\n};' % (',\n  '.join(l))
    print 'static const char *testEncodingsEncoded[%d] = {\n  %s\n};' % (len(l), ',\n  '.join(encoded))
    print 'static const char *testEncodingsExpected[%d] = {\n  %s\n};' % (len(l), ',\n  '.join(expected))
else:
    for i in range(len(expected)):
        print expected[i]
        #print '%s: %s' % (expected[i], l[i])



u = (u'grüßEN SébasTIen %s' % source)
print 'static const char *toUpperSource = %s;' % ToArray(u.encode('utf-8'))
print 'static const char *toUpperResult = %s;' % ToArray(u.upper().encode('utf-8'))
