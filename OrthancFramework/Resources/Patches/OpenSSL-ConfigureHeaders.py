#!/usr/bin/env python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


import json
import os
import re
import sys

if len(sys.argv) != 2:
    raise Exception('Bad number of arguments')


# This emulates "util/perl/OpenSSL/stackhash.pm"

GENERATE_STACK_MACROS = '''
SKM_DEFINE_STACK_OF_INTERNAL(${nametype}, ${realtype}, ${plaintype})
#define sk_${nametype}_num(sk) OPENSSL_sk_num(ossl_check_const_${nametype}_sk_type(sk))
#define sk_${nametype}_value(sk, idx) ((${realtype} *)OPENSSL_sk_value(ossl_check_const_${nametype}_sk_type(sk), (idx)))
#define sk_${nametype}_new(cmp) ((STACK_OF(${nametype}) *)OPENSSL_sk_new(ossl_check_${nametype}_compfunc_type(cmp)))
#define sk_${nametype}_new_null() ((STACK_OF(${nametype}) *)OPENSSL_sk_new_null())
#define sk_${nametype}_new_reserve(cmp, n) ((STACK_OF(${nametype}) *)OPENSSL_sk_new_reserve(ossl_check_${nametype}_compfunc_type(cmp), (n)))
#define sk_${nametype}_reserve(sk, n) OPENSSL_sk_reserve(ossl_check_${nametype}_sk_type(sk), (n))
#define sk_${nametype}_free(sk) OPENSSL_sk_free(ossl_check_${nametype}_sk_type(sk))
#define sk_${nametype}_zero(sk) OPENSSL_sk_zero(ossl_check_${nametype}_sk_type(sk))
#define sk_${nametype}_delete(sk, i) ((${realtype} *)OPENSSL_sk_delete(ossl_check_${nametype}_sk_type(sk), (i)))
#define sk_${nametype}_delete_ptr(sk, ptr) ((${realtype} *)OPENSSL_sk_delete_ptr(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr)))
#define sk_${nametype}_push(sk, ptr) OPENSSL_sk_push(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr))
#define sk_${nametype}_unshift(sk, ptr) OPENSSL_sk_unshift(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr))
#define sk_${nametype}_pop(sk) ((${realtype} *)OPENSSL_sk_pop(ossl_check_${nametype}_sk_type(sk)))
#define sk_${nametype}_shift(sk) ((${realtype} *)OPENSSL_sk_shift(ossl_check_${nametype}_sk_type(sk)))
#define sk_${nametype}_pop_free(sk, freefunc) OPENSSL_sk_pop_free(ossl_check_${nametype}_sk_type(sk),ossl_check_${nametype}_freefunc_type(freefunc))
#define sk_${nametype}_insert(sk, ptr, idx) OPENSSL_sk_insert(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr), (idx))
#define sk_${nametype}_set(sk, idx, ptr) ((${realtype} *)OPENSSL_sk_set(ossl_check_${nametype}_sk_type(sk), (idx), ossl_check_${nametype}_type(ptr)))
#define sk_${nametype}_find(sk, ptr) OPENSSL_sk_find(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr))
#define sk_${nametype}_find_ex(sk, ptr) OPENSSL_sk_find_ex(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr))
#define sk_${nametype}_find_all(sk, ptr, pnum) OPENSSL_sk_find_all(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_type(ptr), pnum)
#define sk_${nametype}_sort(sk) OPENSSL_sk_sort(ossl_check_${nametype}_sk_type(sk))
#define sk_${nametype}_is_sorted(sk) OPENSSL_sk_is_sorted(ossl_check_const_${nametype}_sk_type(sk))
#define sk_${nametype}_dup(sk) ((STACK_OF(${nametype}) *)OPENSSL_sk_dup(ossl_check_const_${nametype}_sk_type(sk)))
#define sk_${nametype}_deep_copy(sk, copyfunc, freefunc) ((STACK_OF(${nametype}) *)OPENSSL_sk_deep_copy(ossl_check_const_${nametype}_sk_type(sk), ossl_check_${nametype}_copyfunc_type(copyfunc), ossl_check_${nametype}_freefunc_type(freefunc)))
#define sk_${nametype}_set_cmp_func(sk, cmp) ((sk_${nametype}_compfunc)OPENSSL_sk_set_cmp_func(ossl_check_${nametype}_sk_type(sk), ossl_check_${nametype}_compfunc_type(cmp)))
'''


GENERATE_LHASH_MACROS = '''
DEFINE_LHASH_OF_INTERNAL(${type});
#define lh_${type}_new(hfn, cmp) ((LHASH_OF(${type}) *)OPENSSL_LH_new(ossl_check_${type}_lh_hashfunc_type(hfn), ossl_check_${type}_lh_compfunc_type(cmp)))
#define lh_${type}_free(lh) OPENSSL_LH_free(ossl_check_${type}_lh_type(lh))
#define lh_${type}_flush(lh) OPENSSL_LH_flush(ossl_check_${type}_lh_type(lh))
#define lh_${type}_insert(lh, ptr) ((${type} *)OPENSSL_LH_insert(ossl_check_${type}_lh_type(lh), ossl_check_${type}_lh_plain_type(ptr)))
#define lh_${type}_delete(lh, ptr) ((${type} *)OPENSSL_LH_delete(ossl_check_${type}_lh_type(lh), ossl_check_const_${type}_lh_plain_type(ptr)))
#define lh_${type}_retrieve(lh, ptr) ((${type} *)OPENSSL_LH_retrieve(ossl_check_${type}_lh_type(lh), ossl_check_const_${type}_lh_plain_type(ptr)))
#define lh_${type}_error(lh) OPENSSL_LH_error(ossl_check_${type}_lh_type(lh))
#define lh_${type}_num_items(lh) OPENSSL_LH_num_items(ossl_check_${type}_lh_type(lh))
#define lh_${type}_node_stats_bio(lh, out) OPENSSL_LH_node_stats_bio(ossl_check_const_${type}_lh_type(lh), out)
#define lh_${type}_node_usage_stats_bio(lh, out) OPENSSL_LH_node_usage_stats_bio(ossl_check_const_${type}_lh_type(lh), out)
#define lh_${type}_stats_bio(lh, out) OPENSSL_LH_stats_bio(ossl_check_const_${type}_lh_type(lh), out)
#define lh_${type}_get_down_load(lh) OPENSSL_LH_get_down_load(ossl_check_${type}_lh_type(lh))
#define lh_${type}_set_down_load(lh, dl) OPENSSL_LH_set_down_load(ossl_check_${type}_lh_type(lh), dl)
#define lh_${type}_doall(lh, dfn) OPENSSL_LH_doall(ossl_check_${type}_lh_type(lh), ossl_check_${type}_lh_doallfunc_type(dfn))
'''


with open(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                       'OpenSSL-ExtractProvidersOIDs.json'), 'r') as f:
    OIDS = json.loads(f.read())


CURRENT_HEADER = ''
    
def Parse(match):
    s = ''
    
    for t in re.findall('generate_stack_macros\("(.+?)"\)', match.group(1)):
        s += (GENERATE_STACK_MACROS
              .replace('${nametype}', t)
              .replace('${realtype}', t)
              .replace('${plaintype}', t))
        
    for t in re.findall('generate_const_stack_macros\("(.+?)"\)', match.group(1)):
        s += (GENERATE_STACK_MACROS
              .replace('${nametype}', t)
              .replace('${realtype}', 'const %s' % t)
              .replace('${plaintype}', t))

    for t in re.findall('generate_stack_string_macros\(\)', match.group(1)):
        s += (GENERATE_STACK_MACROS
              .replace('${nametype}', 'OPENSSL_STRING')
              .replace('${realtype}', 'char')
              .replace('${plaintype}', 'char'))

    for t in re.findall('generate_stack_const_string_macros\(\)', match.group(1)):
        s += (GENERATE_STACK_MACROS
              .replace('${nametype}', 'OPENSSL_CSTRING')
              .replace('${realtype}', 'const char')
              .replace('${plaintype}', 'char'))

    for t in re.findall('generate_stack_block_macros\(\)', match.group(1)):
        s += (GENERATE_STACK_MACROS
              .replace('${nametype}', 'OPENSSL_BLOCK')
              .replace('${realtype}', 'void')
              .replace('${plaintype}', 'void'))
        
    for t in re.findall('generate_lhash_macros\("(.+?)"\)', match.group(1)):
        s += GENERATE_LHASH_MACROS.replace('${type}', t)

    for t in re.findall('\$config{rc4_int}', match.group(1)):
        s += 'unsigned int'

    for t in re.findall('oids_to_c::process_leaves\(.+?\)', match.group(1), re.MULTILINE | re.DOTALL):
        if not CURRENT_HEADER in OIDS:
            raise Exception('Unknown header: %s' % CURRENT_HEADER)

        for (name, definition) in OIDS[CURRENT_HEADER].items():
            s += '#define DER_OID_V_%s %s\n' % (name, ', '.join(definition))
            s += '#define DER_OID_SZ_%s %d\n' % (name, len(definition))
            s += 'extern const unsigned char ossl_der_oid_%s[DER_OID_SZ_%s];\n\n' % (name, name)
        
    return s


for base in [ 'include/openssl',
              'providers/common/include/prov' ]:
    directory = os.path.join(sys.argv[1], base)
    for source in os.listdir(directory):
        if source.endswith('.h.in'):
            target = re.sub('\.h\.in$', '.h', source)
                            
            with open(os.path.join(directory, source), 'r') as f:
                with open(os.path.join(directory, target), 'w') as g:
                    CURRENT_HEADER = source
                    g.write(re.sub('{-(.*?)-}.*?$', Parse, f.read(),
                                   flags = re.MULTILINE | re.DOTALL))


with open(os.path.join(sys.argv[1], 'providers/common/der/orthanc_oids_gen.c'), 'w') as f:
    for (header, content) in OIDS.items():
        f.write('#include "prov/%s"\n' % re.sub('\.h\.in$', '.h', header))

    f.write('\n')
        
    for (header, content) in OIDS.items():
        for (name, definition) in content.items():
            f.write('const unsigned char ossl_der_oid_%s[DER_OID_SZ_%s] = { DER_OID_V_%s };\n' % (
                name, name, name))
