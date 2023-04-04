# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
# Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


set(OPENSSL_VERSION_MAJOR 3)
set(OPENSSL_VERSION_MINOR 1)
set(OPENSSL_VERSION_PATCH 0)
set(OPENSSL_VERSION_PRE_RELEASE "")
set(OPENSSL_VERSION_FULL "${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}.${OPENSSL_VERSION_PATCH}${OPENSSL_VERSION_PRE_RELEASE}")
SET(OPENSSL_SOURCES_DIR ${CMAKE_BINARY_DIR}/openssl-${OPENSSL_VERSION_FULL})
SET(OPENSSL_URL "http://orthanc.osimis.io/ThirdPartyDownloads/openssl-${OPENSSL_VERSION_FULL}.tar.gz")
SET(OPENSSL_MD5 "f6c520aa2206d4d1fa71ea30b5e9a56d")

if (IS_DIRECTORY "${OPENSSL_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${OPENSSL_MD5} ${OPENSSL_URL} "${OPENSSL_SOURCES_DIR}")


if (FirstRun)
  # Apply the patches
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/openssl-${OPENSSL_VERSION_FULL}.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  execute_process(
    COMMAND ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/OpenSSL-ConfigureHeaders.py
    "${OPENSSL_SOURCES_DIR}"
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while configuring the OpenSSL headers")
  endif()

  file(WRITE ${OPENSSL_SOURCES_DIR}/include/openssl/opensslv.h "")
  file(WRITE ${OPENSSL_SOURCES_DIR}/include/crypto/bn_conf.h "")
  file(WRITE ${OPENSSL_SOURCES_DIR}/include/crypto/dso_conf.h "")

  file(WRITE ${OPENSSL_SOURCES_DIR}/crypto/buildinf.h "
#define DATE \"\"
#define PLATFORM \"\"
#define compiler_flags \"\"
")

else()
  message("The patches for OpenSSL have already been applied")
endif()


if (OPENSSL_VERSION_PRE_RELEASE STREQUAL "")
  set(VERSION_VERSION_OFFSET 0)
else()
  set(VERSION_VERSION_OFFSET 15)
endif()

math(EXPR OPENSSL_CONFIGURED_API "${OPENSSL_VERSION_MAJOR} * 10000 + ${OPENSSL_VERSION_MINOR} * 100")

# This macro is normally defined in "opensslv.h.in"
math(EXPR OPENSSL_VERSION_NUMBER "(${OPENSSL_VERSION_MAJOR} << 28) + (${OPENSSL_VERSION_MINOR} << 20) + (${OPENSSL_VERSION_PATCH} << 4) + ${VERSION_VERSION_OFFSET}")

list(GET CMAKE_FIND_LIBRARY_SUFFIXES 0 OPENSSL_DSO_EXTENSION)

add_definitions(
  -DOPENSSL_VERSION_MAJOR=${OPENSSL_VERSION_MAJOR}
  -DOPENSSL_VERSION_MINOR=${OPENSSL_VERSION_MINOR}
  -DOPENSSL_VERSION_PATCH=${OPENSSL_VERSION_PATCH}
  -DOPENSSL_CONFIGURED_API=${OPENSSL_CONFIGURED_API}
  -DOPENSSL_VERSION_NUMBER=${OPENSSL_VERSION_NUMBER}
  -DOPENSSL_VERSION_PRE_RELEASE="${OPENSSL_VERSION_PRE_RELEASE}"
  -DOPENSSL_VERSION_BUILD_METADATA=""
  -DOPENSSL_VERSION_TEXT="OpenSSL ${OPENSSL_VERSION_FULL}"
  -DOPENSSL_VERSION_STR="${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}.${OPENSSL_VERSION_PATCH}"
  -DOPENSSL_FULL_VERSION_STR="${OPENSSL_VERSION_FULL}"
  -DDSO_EXTENSION="${OPENSSL_DSO_EXTENSION}"

  -DOPENSSLDIR="/usr/local/ssl"
  -DMODULESDIR=""  # TODO
  
  -DOPENSSL_BUILDING_OPENSSL
  -DOPENSSL_THREADS
  -DOPENSSL_IA32_SSE2
  
  -DOPENSSL_NO_AFALGENG
  -DOPENSSL_NO_ASM
  -DOPENSSL_NO_CHACHA  # Necessary for VC2015-64 since openssl-3.0.1
  -DOPENSSL_NO_DEVCRYPTOENG
  -DOPENSSL_NO_DYNAMIC_ENGINE
  -DOPENSSL_NO_EC_NISTP_64_GCC_128
  -DOPENSSL_NO_GOST
  -DOPENSSL_NO_RFC3779
  -DOPENSSL_NO_SCTP

  -DOPENSSL_NO_KTLS  # TODO ?
  )


include_directories(
  BEFORE
  ${OPENSSL_SOURCES_DIR}
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_32
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/include
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/include
  ${OPENSSL_SOURCES_DIR}/providers/common/include
  ${OPENSSL_SOURCES_DIR}/providers/implementations/include
  )


set(OPENSSL_SOURCES_SUBDIRS
  ## Assembly is disabled
  # ${OPENSSL_SOURCES_DIR}/crypto/aes/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/bf/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/bn/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/camellia/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/cast/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/chacha/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/des/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/ec/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/md5/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/modes/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/poly1305/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/rc4/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/rc5/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/ripemd/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/sha/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/whrlpool/asm

  ${OPENSSL_SOURCES_DIR}/crypto
  ${OPENSSL_SOURCES_DIR}/crypto/aes
  ${OPENSSL_SOURCES_DIR}/crypto/aria
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/async
  ${OPENSSL_SOURCES_DIR}/crypto/async/arch
  ${OPENSSL_SOURCES_DIR}/crypto/bf
  ${OPENSSL_SOURCES_DIR}/crypto/bio
  ${OPENSSL_SOURCES_DIR}/crypto/bn
  ${OPENSSL_SOURCES_DIR}/crypto/buffer
  ${OPENSSL_SOURCES_DIR}/crypto/camellia
  ${OPENSSL_SOURCES_DIR}/crypto/cast
  ${OPENSSL_SOURCES_DIR}/crypto/chacha
  ${OPENSSL_SOURCES_DIR}/crypto/cmac
  ${OPENSSL_SOURCES_DIR}/crypto/cmp
  ${OPENSSL_SOURCES_DIR}/crypto/cms
  ${OPENSSL_SOURCES_DIR}/crypto/comp
  ${OPENSSL_SOURCES_DIR}/crypto/conf
  ${OPENSSL_SOURCES_DIR}/crypto/crmf
  ${OPENSSL_SOURCES_DIR}/crypto/ct
  ${OPENSSL_SOURCES_DIR}/crypto/des
  ${OPENSSL_SOURCES_DIR}/crypto/dh
  ${OPENSSL_SOURCES_DIR}/crypto/dsa
  ${OPENSSL_SOURCES_DIR}/crypto/dso
  ${OPENSSL_SOURCES_DIR}/crypto/ec
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_32
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_64
  ${OPENSSL_SOURCES_DIR}/crypto/encode_decode
  ${OPENSSL_SOURCES_DIR}/crypto/engine
  ${OPENSSL_SOURCES_DIR}/crypto/err
  ${OPENSSL_SOURCES_DIR}/crypto/ess
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/ffc
  ${OPENSSL_SOURCES_DIR}/crypto/hmac
  ${OPENSSL_SOURCES_DIR}/crypto/http
  ${OPENSSL_SOURCES_DIR}/crypto/idea
  ${OPENSSL_SOURCES_DIR}/crypto/kdf
  ${OPENSSL_SOURCES_DIR}/crypto/lhash
  ${OPENSSL_SOURCES_DIR}/crypto/md2
  ${OPENSSL_SOURCES_DIR}/crypto/md4
  ${OPENSSL_SOURCES_DIR}/crypto/md5
  ${OPENSSL_SOURCES_DIR}/crypto/mdc2
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/crypto/objects
  ${OPENSSL_SOURCES_DIR}/crypto/ocsp
  ${OPENSSL_SOURCES_DIR}/crypto/pem
  ${OPENSSL_SOURCES_DIR}/crypto/perlasm
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs12
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305
  ${OPENSSL_SOURCES_DIR}/crypto/property
  ${OPENSSL_SOURCES_DIR}/crypto/rand
  ${OPENSSL_SOURCES_DIR}/crypto/rc2
  ${OPENSSL_SOURCES_DIR}/crypto/rc4
  ${OPENSSL_SOURCES_DIR}/crypto/rc5
  ${OPENSSL_SOURCES_DIR}/crypto/ripemd
  ${OPENSSL_SOURCES_DIR}/crypto/rsa
  ${OPENSSL_SOURCES_DIR}/crypto/seed
  ${OPENSSL_SOURCES_DIR}/crypto/sha
  ${OPENSSL_SOURCES_DIR}/crypto/siphash
  ${OPENSSL_SOURCES_DIR}/crypto/sm2
  ${OPENSSL_SOURCES_DIR}/crypto/sm3
  ${OPENSSL_SOURCES_DIR}/crypto/sm4
  ${OPENSSL_SOURCES_DIR}/crypto/srp
  ${OPENSSL_SOURCES_DIR}/crypto/stack
  ${OPENSSL_SOURCES_DIR}/crypto/store
  ${OPENSSL_SOURCES_DIR}/crypto/ts
  ${OPENSSL_SOURCES_DIR}/crypto/txt_db
  ${OPENSSL_SOURCES_DIR}/crypto/ui
  ${OPENSSL_SOURCES_DIR}/crypto/whrlpool
  ${OPENSSL_SOURCES_DIR}/crypto/x509

  # ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding  # OS-specific
  ${OPENSSL_SOURCES_DIR}/providers
  ${OPENSSL_SOURCES_DIR}/providers/common
  ${OPENSSL_SOURCES_DIR}/providers/common/der
  ${OPENSSL_SOURCES_DIR}/providers/implementations/asymciphers
  ${OPENSSL_SOURCES_DIR}/providers/implementations/ciphers
  ${OPENSSL_SOURCES_DIR}/providers/implementations/digests
  ${OPENSSL_SOURCES_DIR}/providers/implementations/encode_decode
  ${OPENSSL_SOURCES_DIR}/providers/implementations/exchange
  ${OPENSSL_SOURCES_DIR}/providers/implementations/kdfs
  ${OPENSSL_SOURCES_DIR}/providers/implementations/kem
  ${OPENSSL_SOURCES_DIR}/providers/implementations/keymgmt
  ${OPENSSL_SOURCES_DIR}/providers/implementations/macs
  ${OPENSSL_SOURCES_DIR}/providers/implementations/rands
  ${OPENSSL_SOURCES_DIR}/providers/implementations/signature
  ${OPENSSL_SOURCES_DIR}/providers/implementations/storemgmt

  ${OPENSSL_SOURCES_DIR}/ssl
  ${OPENSSL_SOURCES_DIR}/ssl/record
  ${OPENSSL_SOURCES_DIR}/ssl/statem
  )

if (ENABLE_OPENSSL_ENGINES)
  add_definitions(
    #-DENGINESDIR="/usr/local/lib/engines-1.1"  # On GNU/Linux
    -DENGINESDIR="."
    )

  list(APPEND OPENSSL_SOURCES_SUBDIRS
    ${OPENSSL_SOURCES_DIR}/engines
    ${OPENSSL_SOURCES_DIR}/crypto/engine
    )
else()
  add_definitions(-DOPENSSL_NO_ENGINE)
endif()

list(APPEND OPENSSL_SOURCES_SUBDIRS
  # EC, ECDH and ECDSA are necessary for PKCS11, and for contacting
  # HTTPS servers that use TLS certificate encrypted with ECDSA
  # (check the output of a recent version of the "sslscan"
  # command). Until Orthanc <= 1.4.1, these features were only
  # enabled if ENABLE_PKCS11 support was set to "ON".
  # https://groups.google.com/d/msg/orthanc-users/2l-bhYIMEWg/oMmK33bYBgAJ
  ${OPENSSL_SOURCES_DIR}/crypto/ec
  ${OPENSSL_SOURCES_DIR}/crypto/ecdh
  ${OPENSSL_SOURCES_DIR}/crypto/ecdsa
  )

foreach(d ${OPENSSL_SOURCES_SUBDIRS})
  AUX_SOURCE_DIRECTORY(${d} OPENSSL_SOURCES)
endforeach()


list(REMOVE_ITEM OPENSSL_SOURCES
  # Files below are not part of the "libcrypto.a" and "libssl.a" that
  # are created by compiling OpenSSL from sources
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_nyi.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_unix.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_vms.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win32.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_wince.c
  ${OPENSSL_SOURCES_DIR}/crypto/aes/aes_x86core.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/ncbc_enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp224.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp521.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256_table.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_s390x_nistp.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecx_s390x.c
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_base2_44.c
  ${OPENSSL_SOURCES_DIR}/crypto/rsa/rsa_acvp_test_params.c
  ${OPENSSL_SOURCES_DIR}/engines/e_devcrypto.c
  ${OPENSSL_SOURCES_DIR}/engines/e_loader_attic.c
  ${OPENSSL_SOURCES_DIR}/providers/common/securitycheck_fips.c
  ${OPENSSL_SOURCES_DIR}/providers/implementations/macs/blake2_mac_impl.c
  
  ${OPENSSL_SOURCES_DIR}/engines/e_afalg.c  # Fails on OS X and Visual Studio
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_ieee754.c  # Fails on Visual Studio

  ${OPENSSL_SOURCES_DIR}/ssl/ktls.c   # TODO ?

  # Disable PowerPC sources
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bn_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/chacha/chacha_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha_ppc.c

  # Disable SPARC sources
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bn_sparc.c

  # Disable CPUID for non-x86 platforms
  ${OPENSSL_SOURCES_DIR}/crypto/armcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/loongarchcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/ppccap.c
  ${OPENSSL_SOURCES_DIR}/crypto/riscvcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/s390xcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/sparcv9cap.c
  )


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD" OR
    APPLE)
  list(APPEND OPENSSL_SOURCES
    ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding/rand_unix.c
    )
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  list(APPEND OPENSSL_SOURCES
    ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding/rand_win.c
    )  
endif()
  

# Check out "${OPENSSL_SOURCES_DIR}/Configurations/README.md": "This
# is default if no option is specified, it works on any supported
# system." It is mandatory to define it as a macro, as it is used by
# all the source files that include OpenSSL (e.g. "Core/Toolbox.cpp"
# or curl)
add_definitions(-DTHIRTY_TWO_BIT)


if (NOT CMAKE_COMPILER_IS_GNUCXX OR
    "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows" OR
    "${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
  # Disable the use of a gcc extension, that is neither available on
  # MinGW, nor on LSB
  add_definitions(
    -DOPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE
    )
endif()


if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(OPENSSL_DEFINITIONS
    "${OPENSSL_DEFINITIONS};OPENSSL_SYSNAME_WIN32;SO_WIN32;WIN32_LEAN_AND_MEAN;L_ENDIAN;NO_WINDOWS_BRAINDEATH")
  
  if (ENABLE_OPENSSL_ENGINES)
    link_libraries(crypt32)
  endif()

  add_definitions(
    -DOPENSSL_RAND_SEED_OS  # ${OPENSSL_SOURCES_DIR}/crypto/rand/rand_win.c
    )
 
elseif ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
  add_definitions(
    # In order for "crypto/mem_sec.c" to compile on LSB
    -DOPENSSL_NO_SECURE_MEMORY

    # The "OPENSSL_RAND_SEED_OS" value implies a syscall() to
    # "__NR_getrandom" (i.e. system call "getentropy(2)") in
    # "rand_unix.c", which is not available in LSB.
    -DOPENSSL_RAND_SEED_DEVRANDOM

    # If "OPENSSL_NO_ERR" is not defined, the PostgreSQL plugin
    # crashes with segmentation fault in function
    # "build_SYS_str_reasons()", that is called from
    # "OPENSSL_init_ssl()"
    # https://bugs.orthanc-server.com/show_bug.cgi?id=193
    -DOPENSSL_NO_ERR
    )

else()
  # Fixes error "OpenSSL error: error:2406C06E:random number
  # generator:RAND_DRBG_instantiate:error retrieving entropy" that was
  # present in Orthanc 1.6.0, if statically linking on Ubuntu 18.04
  add_definitions(
    -DOPENSSL_RAND_SEED_OS
    )
endif()


set_source_files_properties(
  ${OPENSSL_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS
    "${OPENSSL_DEFINITIONS};DSO_NONE"
    )
