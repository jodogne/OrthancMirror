#!/usr/bin/python
# -*- coding: utf-8 -*-

source = u'éäöòДΘĝדصķћ'

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
}

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
    l.append('Orthanc::Encoding_%s' % orthancEnumeration)
    s = source.encode(encoding, 'replace')
    encoded.append(ToArray(s))
    expected.append(ToArray(s.decode(encoding).encode('utf-8')))

print 'static const unsigned int testEncodingsCount = %d;' % len(encodings)
print 'static const Orthanc::Encoding testEncodings[] = {\n  %s\n};' % (',\n  '.join(l))
print 'static const char *testEncodingsEncoded[%d] = {\n  %s\n};' % (len(encodings), ',\n  '.join(encoded))
print 'static const char *testEncodingsExpected[%d] = {\n  %s\n};' % (len(encodings), ',\n  '.join(expected))
