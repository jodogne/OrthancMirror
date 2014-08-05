#!/usr/bin/python
# -*- coding: utf-8 -*-

source = u'éäöòДΘĝדصķћ๛ﾈİ'

encodings = {
    'UTF-8' : 'Utf8',
    'ASCII' : 'Ascii',
    'ISO-8859-1' : 'Latin1',
    'ISO-8859-2' : 'Latin2',
    'ISO-8859-3' : 'Latin3',
    'ISO-8859-4' : 'Latin4',
    'ISO-8859-9' : 'Latin5',
    'ISO-8859-5' : 'Cyrillic',
    'ISO-8859-6' : 'Arabic',
    'ISO-8859-7' : 'Greek',
    'ISO-8859-8' : 'Hebrew',
    'TIS-620' : 'Thai',
    'SHIFT-JIS' : 'Japanese',
    #'GB18030' : 'Chinese',
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


# https://en.wikipedia.org/wiki/GB_18030#Technical_details
l.append('::Orthanc::Encoding_Chinese')
expected.append(ToArray('Þßàáâã'))
encoded.append('"\\x81\\x30\\x89\\x37\\x81\\x30\\x89\\x38\\xA8\\xA4\\xA8\\xA2\\x81\\x30\\x89\\x39\\x81\\x30\\x8A\\x30"')


if True:
    print 'static const unsigned int testEncodingsCount = %d;' % len(l)
    print 'static const ::Orthanc::Encoding testEncodings[] = {\n  %s\n};' % (',\n  '.join(l))
    print 'static const char *testEncodingsEncoded[%d] = {\n  %s\n};' % (len(l), ',\n  '.join(encoded))
    print 'static const char *testEncodingsExpected[%d] = {\n  %s\n};' % (len(l), ',\n  '.join(expected))
else:
    for i in range(len(expected)):
        print expected[i]
        #print '%s: %s' % (expected[i], l[i])
