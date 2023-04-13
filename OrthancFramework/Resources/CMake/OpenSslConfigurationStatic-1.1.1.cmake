# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


SET(OPENSSL_SOURCES_DIR ${CMAKE_BINARY_DIR}/openssl-1.1.1k)
SET(OPENSSL_URL "https://orthanc.uclouvain.be/third-party-downloads/openssl-1.1.1k.tar.gz")
SET(OPENSSL_MD5 "c4e7d95f782b08116afa27b30393dd27")

if (IS_DIRECTORY "${OPENSSL_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${OPENSSL_MD5} ${OPENSSL_URL} "${OPENSSL_SOURCES_DIR}")

if (FirstRun)
  file(WRITE ${OPENSSL_SOURCES_DIR}/crypto/buildinf.h "
#define DATE \"\"
#define PLATFORM \"\"
#define compiler_flags \"\"
")
  file(WRITE ${OPENSSL_SOURCES_DIR}/crypto/bn_conf.h "")
  file(WRITE ${OPENSSL_SOURCES_DIR}/crypto/dso_conf.h "")

  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/openssl-1.1.1-conf.h.in
    ${OPENSSL_SOURCES_DIR}/include/openssl/opensslconf.h
    )

  # Apply the patches
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/openssl-1.1.1k.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  file(RENAME
    ${OPENSSL_SOURCES_DIR}/include/openssl/e_os2.h
    ${OPENSSL_SOURCES_DIR}/include/openssl/e_os2_source.h)

  # The following patch of "e_os2.h" prevents from building OpenSSL
  # as a DLL under Windows. Otherwise, symbols have inconsistent
  # linkage if ${OPENSSL_SOURCES} is used to create a DLL (notably
  # if building an Orthanc plugin such as PostgreSQL or MySQL).
  file(WRITE ${OPENSSL_SOURCES_DIR}/include/openssl/e_os2.h "
#include \"e_os2_source.h\"
#if defined(_WIN32)
#  undef OPENSSL_EXPORT
#  undef OPENSSL_IMPORT
#  undef OPENSSL_EXTERN
#  undef OPENSSL_GLOBAL
#  define OPENSSL_EXPORT
#  define OPENSSL_IMPORT
#  define OPENSSL_EXTERN extern
#  define OPENSSL_GLOBAL
#endif
")

else()
  message("The patches for OpenSSL have already been applied")
endif()

add_definitions(
  -DOPENSSL_THREADS
  -DOPENSSL_IA32_SSE2
  -DOPENSSL_NO_ASM
  -DOPENSSL_NO_DYNAMIC_ENGINE
  -DOPENSSL_NO_DEVCRYPTOENG

  -DOPENSSL_NO_BF 
  -DOPENSSL_NO_CAMELLIA
  -DOPENSSL_NO_CAST 
  -DOPENSSL_NO_EC_NISTP_64_GCC_128
  -DOPENSSL_NO_GMP
  -DOPENSSL_NO_GOST
  -DOPENSSL_NO_HW
  -DOPENSSL_NO_JPAKE
  -DOPENSSL_NO_IDEA
  -DOPENSSL_NO_KRB5 
  -DOPENSSL_NO_MD2 
  -DOPENSSL_NO_MDC2 
  #-DOPENSSL_NO_MD4   # MD4 is necessary for MariaDB/MySQL client
  -DOPENSSL_NO_RC2 
  -DOPENSSL_NO_RC4 
  -DOPENSSL_NO_RC5 
  -DOPENSSL_NO_RFC3779
  -DOPENSSL_NO_SCTP
  -DOPENSSL_NO_STORE
  -DOPENSSL_NO_SEED
  -DOPENSSL_NO_WHIRLPOOL
  -DOPENSSL_NO_RIPEMD
  -DOPENSSL_NO_AFALGENG

  -DOPENSSLDIR="/usr/local/ssl"
  )


include_directories(
  ${OPENSSL_SOURCES_DIR}
  ${OPENSSL_SOURCES_DIR}/crypto
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_32
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/include
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/include
  )


set(OPENSSL_SOURCES_SUBDIRS
  ${OPENSSL_SOURCES_DIR}/crypto
  ${OPENSSL_SOURCES_DIR}/crypto/aes
  ${OPENSSL_SOURCES_DIR}/crypto/aria
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/async
  ${OPENSSL_SOURCES_DIR}/crypto/async/arch
  ${OPENSSL_SOURCES_DIR}/crypto/bio
  ${OPENSSL_SOURCES_DIR}/crypto/blake2
  ${OPENSSL_SOURCES_DIR}/crypto/bn
  ${OPENSSL_SOURCES_DIR}/crypto/buffer
  ${OPENSSL_SOURCES_DIR}/crypto/chacha
  ${OPENSSL_SOURCES_DIR}/crypto/cmac
  ${OPENSSL_SOURCES_DIR}/crypto/cms
  ${OPENSSL_SOURCES_DIR}/crypto/comp
  ${OPENSSL_SOURCES_DIR}/crypto/conf
  ${OPENSSL_SOURCES_DIR}/crypto/ct
  ${OPENSSL_SOURCES_DIR}/crypto/des
  ${OPENSSL_SOURCES_DIR}/crypto/dh
  ${OPENSSL_SOURCES_DIR}/crypto/dsa
  ${OPENSSL_SOURCES_DIR}/crypto/dso
  ${OPENSSL_SOURCES_DIR}/crypto/ec
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_32
  ${OPENSSL_SOURCES_DIR}/crypto/err
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/hmac
  ${OPENSSL_SOURCES_DIR}/crypto/kdf
  ${OPENSSL_SOURCES_DIR}/crypto/lhash
  ${OPENSSL_SOURCES_DIR}/crypto/md4
  ${OPENSSL_SOURCES_DIR}/crypto/md5
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/crypto/objects
  ${OPENSSL_SOURCES_DIR}/crypto/ocsp
  ${OPENSSL_SOURCES_DIR}/crypto/pem
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs12
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305
  ${OPENSSL_SOURCES_DIR}/crypto/pqueue
  ${OPENSSL_SOURCES_DIR}/crypto/rand
  ${OPENSSL_SOURCES_DIR}/crypto/ripemd
  ${OPENSSL_SOURCES_DIR}/crypto/rsa
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
  ${OPENSSL_SOURCES_DIR}/crypto/x509
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3
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
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_nyi.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_unix.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_vms.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win32.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_wince.c
  ${OPENSSL_SOURCES_DIR}/crypto/aes/aes_x86core.c
  ${OPENSSL_SOURCES_DIR}/crypto/armcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/bio/bss_dgram.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/ncbc_enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256_table.c
  ${OPENSSL_SOURCES_DIR}/crypto/engine/eng_devcrypto.c
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_base2_44.c  # Cannot be compiled with MinGW
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_ieee754.c  # Cannot be compiled with MinGW
  ${OPENSSL_SOURCES_DIR}/crypto/ppccap.c
  ${OPENSSL_SOURCES_DIR}/crypto/s390xcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/sparcv9cap.c
  ${OPENSSL_SOURCES_DIR}/engines/e_afalg.c  # Cannot be compiled with MinGW
  )

# Check out "${OPENSSL_SOURCES_DIR}/Configurations/README": "This is
# default if no option is specified, it works on any supported
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
