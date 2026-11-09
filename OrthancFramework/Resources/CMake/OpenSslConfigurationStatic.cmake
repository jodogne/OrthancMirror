# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


find_package(Perl REQUIRED)

set(OPENSSL_VERSION_MAJOR 3)
set(OPENSSL_VERSION_MINOR 5)
set(OPENSSL_VERSION_PATCH 9)
set(OPENSSL_VERSION_PRE_RELEASE "")
set(OPENSSL_VERSION_FULL "${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}.${OPENSSL_VERSION_PATCH}${OPENSSL_VERSION_PRE_RELEASE}")
SET(OPENSSL_SOURCES_DIR ${CMAKE_BINARY_DIR}/openssl-${OPENSSL_VERSION_FULL})
SET(OPENSSL_URL "${THIRD_PARTY_DOWNLOADS_ROOT_URL}/openssl-${OPENSSL_VERSION_FULL}.tar.gz")
SET(OPENSSL_MD5 "382c33039018809e6351539e9bdcd509")

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


  # Execute the "config" script from the OpenSSL source distribution
  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    set(Options --with-rand-seed=devrandom linux-x86_64)
  elseif (MSVC)
    # This is Visual Studio
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(Options VC-WIN32)
    else()
      set(Options VC-WIN64A)
    endif()
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows" AND CMAKE_COMPILER_IS_GNUCXX)
    # This is MinGW
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(Options mingw)
    else()
      set(Options mingw64)
    endif()
  else()
    # Generic system
    set(Options)
  endif()

  execute_process(
    COMMAND ${PERL_EXECUTABLE}
    Configure no-asm ${Options}
    WORKING_DIRECTORY ${OPENSSL_SOURCES_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while configuring OpenSSL using Perl")
  endif()


  # Create ${GENERATED_MANDATORY} targets from the Makefile of OpenSSL
  execute_process(
    COMMAND ${PERL_EXECUTABLE}
    util/mkbuildinf.pl "Orthanc static ${CMAKE_SYSTEM_NAME}" ""
    WORKING_DIRECTORY ${OPENSSL_SOURCES_DIR}
    OUTPUT_FILE crypto/buildinf.h
    RESULT_VARIABLE Failure
    ERROR_VARIABLE FailureOutput
    )

  if (NOT Failure EQUAL 0)
    message(FATAL_ERROR "Perl failed with exit code ${Failure}: ${FailureOutput}")
  endif()


  foreach(config_file
      crypto/params_idx.c
      include/internal/param_names.h
      include/openssl/core_names.h
      )
    execute_process(
      COMMAND ${PERL_EXECUTABLE} -I. -Iutil/perl -Mconfigdata -MOpenSSL::paramnames
      util/dofile.pl ${config_file}.in
      WORKING_DIRECTORY ${OPENSSL_SOURCES_DIR}
      OUTPUT_FILE ${config_file}
      RESULT_VARIABLE Failure
      ERROR_VARIABLE FailureOutput
      )

    if (NOT Failure EQUAL 0)
      message(FATAL_ERROR "Perl failed on ${config_file} with exit code ${Failure}: ${FailureOutput}")
    endif()
  endforeach()


  foreach(config_file
      include/crypto/bn_conf.h
      include/crypto/dso_conf.h
      include/openssl/asn1.h
      include/openssl/asn1t.h
      include/openssl/bio.h
      include/openssl/cmp.h
      include/openssl/cms.h
      include/openssl/comp.h
      include/openssl/conf.h
      include/openssl/crmf.h
      include/openssl/crypto.h
      include/openssl/ct.h
      include/openssl/err.h
      include/openssl/ess.h
      include/openssl/fipskey.h
      include/openssl/lhash.h
      include/openssl/ocsp.h
      include/openssl/opensslv.h
      include/openssl/pkcs12.h
      include/openssl/pkcs7.h
      include/openssl/safestack.h
      include/openssl/srp.h
      include/openssl/ssl.h
      include/openssl/ui.h
      include/openssl/x509.h
      include/openssl/x509_acert.h
      include/openssl/x509_vfy.h
      include/openssl/x509v3.h
      )
    execute_process(
      COMMAND ${PERL_EXECUTABLE} -I. -Mconfigdata
      util/dofile.pl ${config_file}.in
      WORKING_DIRECTORY ${OPENSSL_SOURCES_DIR}
      OUTPUT_FILE ${config_file}
      RESULT_VARIABLE Failure
      ERROR_VARIABLE FailureOutput
      )

    if (NOT Failure EQUAL 0)
      message(FATAL_ERROR "Perl failed on ${config_file} with exit code ${Failure}: ${FailureOutput}")
    endif()
  endforeach()


  foreach(config_file
      providers/common/der/der_digests_gen.c
      providers/common/der/der_dsa_gen.c
      providers/common/der/der_ec_gen.c
      providers/common/der/der_ecx_gen.c
      providers/common/der/der_ml_dsa_gen.c
      providers/common/der/der_rsa_gen.c
      providers/common/der/der_slh_dsa_gen.c
      providers/common/der/der_sm2_gen.c
      providers/common/der/der_wrap_gen.c
      providers/common/include/prov/der_digests.h
      providers/common/include/prov/der_dsa.h
      providers/common/include/prov/der_ec.h
      providers/common/include/prov/der_ecx.h
      providers/common/include/prov/der_ml_dsa.h
      providers/common/include/prov/der_rsa.h
      providers/common/include/prov/der_slh_dsa.h
      providers/common/include/prov/der_sm2.h
      providers/common/include/prov/der_wrap.h
      )
    execute_process(
      COMMAND ${PERL_EXECUTABLE} -I. -Iproviders/common/der -Mconfigdata -Moids_to_c
      util/dofile.pl ${config_file}.in
      WORKING_DIRECTORY ${OPENSSL_SOURCES_DIR}
      OUTPUT_FILE ${config_file}
      RESULT_VARIABLE Failure
      ERROR_VARIABLE FailureOutput
      )

    if (NOT Failure EQUAL 0)
      message(FATAL_ERROR "Perl failed on ${config_file} with exit code ${Failure}: ${FailureOutput}")
    endif()
  endforeach()

else()
  message("The patches for OpenSSL have already been applied")
endif()



add_definitions(
  -DENGINESDIR="/usr/local/lib64/engines-3"
  -DMODULESDIR="/usr/local/lib64/ossl-modules"
  -DOPENSSLDIR="/usr/local/ssl"
  -DOPENSSL_BUILDING_OPENSSL

  #-DL_ENDIAN
  #-DOPENSSL_PIC
  #-DOPENSSL_USE_NODELETE
  )


include_directories(
  BEFORE
  ${OPENSSL_SOURCES_DIR}
  ${OPENSSL_SOURCES_DIR}/include
  ${OPENSSL_SOURCES_DIR}/providers/common/include
  ${OPENSSL_SOURCES_DIR}/providers/fips/include
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
  # ${OPENSSL_SOURCES_DIR}/crypto/perlasm
  # ${OPENSSL_SOURCES_DIR}/crypto/poly1305/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/rc4/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/rc5/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/ripemd/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/sha/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/sm3/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/sm4/asm
  # ${OPENSSL_SOURCES_DIR}/crypto/whrlpool/asm

  # ${OPENSSL_SOURCES_DIR}/crypto/md2
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
  ${OPENSSL_SOURCES_DIR}/crypto/encode_decode
  ${OPENSSL_SOURCES_DIR}/crypto/err
  ${OPENSSL_SOURCES_DIR}/crypto/ess
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/ffc
  ${OPENSSL_SOURCES_DIR}/crypto/hashtable
  ${OPENSSL_SOURCES_DIR}/crypto/hmac
  ${OPENSSL_SOURCES_DIR}/crypto/hpke
  ${OPENSSL_SOURCES_DIR}/crypto/http
  ${OPENSSL_SOURCES_DIR}/crypto/idea
  ${OPENSSL_SOURCES_DIR}/crypto/kdf
  ${OPENSSL_SOURCES_DIR}/crypto/lhash
  ${OPENSSL_SOURCES_DIR}/crypto/md4
  ${OPENSSL_SOURCES_DIR}/crypto/md5
  ${OPENSSL_SOURCES_DIR}/crypto/mdc2
  ${OPENSSL_SOURCES_DIR}/crypto/ml_dsa
  ${OPENSSL_SOURCES_DIR}/crypto/ml_kem
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/crypto/objects
  ${OPENSSL_SOURCES_DIR}/crypto/ocsp
  ${OPENSSL_SOURCES_DIR}/crypto/pem
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs12
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305
  ${OPENSSL_SOURCES_DIR}/crypto/property
  ${OPENSSL_SOURCES_DIR}/crypto/rand
  ${OPENSSL_SOURCES_DIR}/crypto/rbtree
  ${OPENSSL_SOURCES_DIR}/crypto/rc2
  ${OPENSSL_SOURCES_DIR}/crypto/rc4
  ${OPENSSL_SOURCES_DIR}/crypto/rc5
  ${OPENSSL_SOURCES_DIR}/crypto/ripemd
  ${OPENSSL_SOURCES_DIR}/crypto/rsa
  ${OPENSSL_SOURCES_DIR}/crypto/seed
  ${OPENSSL_SOURCES_DIR}/crypto/sha
  ${OPENSSL_SOURCES_DIR}/crypto/siphash
  ${OPENSSL_SOURCES_DIR}/crypto/slh_dsa
  ${OPENSSL_SOURCES_DIR}/crypto/sm2
  ${OPENSSL_SOURCES_DIR}/crypto/sm3
  ${OPENSSL_SOURCES_DIR}/crypto/sm4
  ${OPENSSL_SOURCES_DIR}/crypto/srp
  ${OPENSSL_SOURCES_DIR}/crypto/stack
  ${OPENSSL_SOURCES_DIR}/crypto/store
  ${OPENSSL_SOURCES_DIR}/crypto/thread
  ${OPENSSL_SOURCES_DIR}/crypto/thread/arch
  ${OPENSSL_SOURCES_DIR}/crypto/ts
  ${OPENSSL_SOURCES_DIR}/crypto/txt_db
  ${OPENSSL_SOURCES_DIR}/crypto/ui
  ${OPENSSL_SOURCES_DIR}/crypto/whrlpool
  ${OPENSSL_SOURCES_DIR}/crypto/x509

  # ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding  # OS-specific
  # ${OPENSSL_SOURCES_DIR}/providers/fips
  ${OPENSSL_SOURCES_DIR}/providers
  ${OPENSSL_SOURCES_DIR}/providers/common
  ${OPENSSL_SOURCES_DIR}/providers/common/der
  ${OPENSSL_SOURCES_DIR}/providers/implementations
  ${OPENSSL_SOURCES_DIR}/providers/implementations/asymciphers
  ${OPENSSL_SOURCES_DIR}/providers/implementations/ciphers
  ${OPENSSL_SOURCES_DIR}/providers/implementations/digests
  ${OPENSSL_SOURCES_DIR}/providers/implementations/encode_decode
  ${OPENSSL_SOURCES_DIR}/providers/implementations/exchange
  ${OPENSSL_SOURCES_DIR}/providers/implementations/include
  ${OPENSSL_SOURCES_DIR}/providers/implementations/include/prov
  ${OPENSSL_SOURCES_DIR}/providers/implementations/kdfs
  ${OPENSSL_SOURCES_DIR}/providers/implementations/kem
  ${OPENSSL_SOURCES_DIR}/providers/implementations/keymgmt
  ${OPENSSL_SOURCES_DIR}/providers/implementations/macs
  ${OPENSSL_SOURCES_DIR}/providers/implementations/rands
  ${OPENSSL_SOURCES_DIR}/providers/implementations/signature
  ${OPENSSL_SOURCES_DIR}/providers/implementations/skeymgmt
  ${OPENSSL_SOURCES_DIR}/providers/implementations/storemgmt

  ${OPENSSL_SOURCES_DIR}/ssl
  ${OPENSSL_SOURCES_DIR}/ssl/quic
  ${OPENSSL_SOURCES_DIR}/ssl/record
  ${OPENSSL_SOURCES_DIR}/ssl/record/methods
  ${OPENSSL_SOURCES_DIR}/ssl/rio
  ${OPENSSL_SOURCES_DIR}/ssl/statem
  )

if (ENABLE_OPENSSL_ENGINES)
  list(APPEND OPENSSL_SOURCES_SUBDIRS
    # ${OPENSSL_SOURCES_DIR}/engines   # Removed in Orthanc 1.13.1
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
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_32
  ${OPENSSL_SOURCES_DIR}/crypto/ec/curve448/arch_64
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
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bn_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bn_sparc.c
  ${OPENSSL_SOURCES_DIR}/crypto/chacha/chacha_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/chacha/chacha_riscv.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/ncbc_enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp224.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp384.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistp521.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256_table.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_s390x_nistp.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_sm2p256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecx_s390x.c
  ${OPENSSL_SOURCES_DIR}/crypto/evp/legacy_md2.c
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_base2_44.c
  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5_ecb.c
  ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5_enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5_skey.c
  ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5cfb64.c
  ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5ofb64.c
  ${OPENSSL_SOURCES_DIR}/crypto/rsa/rsa_acvp_test_params.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha_ppc.c
  ${OPENSSL_SOURCES_DIR}/crypto/sm3/sm3_riscv.c
  ${OPENSSL_SOURCES_DIR}/providers/common/securitycheck_fips.c
  ${OPENSSL_SOURCES_DIR}/providers/implementations/ciphers/cipher_rc5.c
  ${OPENSSL_SOURCES_DIR}/providers/implementations/ciphers/cipher_rc5_hw.c
  ${OPENSSL_SOURCES_DIR}/providers/implementations/digests/md2_prov.c
  ${OPENSSL_SOURCES_DIR}/providers/implementations/macs/blake2_mac_impl.c
  ${OPENSSL_SOURCES_DIR}/ssl/record/methods/ktls_meth.c

  ${OPENSSL_SOURCES_DIR}/crypto/poly1305/poly1305_ieee754.c  # Fails on Visual Studio
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha_riscv.c  # Fails on LSB
  ${OPENSSL_SOURCES_DIR}/providers/legacyprov.c  # Fails on LSB

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
    ${OPENSSL_SOURCES_DIR}/crypto/thread/arch/thread_posix.c
    ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding/rand_unix.c
    )
  list(REMOVE_ITEM OPENSSL_SOURCES
    ${OPENSSL_SOURCES_DIR}/providers/implementations/storemgmt/winstore_store.c
    )

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    add_definitions(-DHAVE_MADVISE=0)
  endif()

elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  list(APPEND OPENSSL_SOURCES
    ${OPENSSL_SOURCES_DIR}/crypto/thread/arch/thread_win.c
    ${OPENSSL_SOURCES_DIR}/providers/implementations/rands/seeding/rand_win.c
    )
  link_libraries(crypt32)

endif()
