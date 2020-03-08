SET(OPENSSL_SOURCES_DIR ${CMAKE_BINARY_DIR}/openssl-1.0.2p)
SET(OPENSSL_URL "http://orthanc.osimis.io/ThirdPartyDownloads/openssl-1.0.2p.tar.gz")
SET(OPENSSL_MD5 "ac5eb30bf5798aa14b1ae6d0e7da58df")

if (IS_DIRECTORY "${OPENSSL_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${OPENSSL_MD5} ${OPENSSL_URL} "${OPENSSL_SOURCES_DIR}")

if (FirstRun)
  file(MAKE_DIRECTORY ${OPENSSL_SOURCES_DIR}/include/openssl)

  foreach(header
      ${OPENSSL_SOURCES_DIR}/crypto/aes/aes.h
      ${OPENSSL_SOURCES_DIR}/crypto/asn1/asn1.h
      ${OPENSSL_SOURCES_DIR}/crypto/asn1/asn1_mac.h
      ${OPENSSL_SOURCES_DIR}/crypto/asn1/asn1t.h
      ${OPENSSL_SOURCES_DIR}/crypto/bf/blowfish.h
      ${OPENSSL_SOURCES_DIR}/crypto/bio/bio.h
      ${OPENSSL_SOURCES_DIR}/crypto/bn/bn.h
      ${OPENSSL_SOURCES_DIR}/crypto/buffer/buffer.h
      ${OPENSSL_SOURCES_DIR}/crypto/camellia/camellia.h
      ${OPENSSL_SOURCES_DIR}/crypto/cast/cast.h
      ${OPENSSL_SOURCES_DIR}/crypto/cmac/cmac.h
      ${OPENSSL_SOURCES_DIR}/crypto/cms/cms.h
      ${OPENSSL_SOURCES_DIR}/crypto/comp/comp.h
      ${OPENSSL_SOURCES_DIR}/crypto/conf/conf.h
      ${OPENSSL_SOURCES_DIR}/crypto/conf/conf_api.h
      ${OPENSSL_SOURCES_DIR}/crypto/crypto.h
      ${OPENSSL_SOURCES_DIR}/crypto/des/des.h
      ${OPENSSL_SOURCES_DIR}/crypto/des/des_old.h
      ${OPENSSL_SOURCES_DIR}/crypto/dh/dh.h
      ${OPENSSL_SOURCES_DIR}/crypto/dsa/dsa.h
      ${OPENSSL_SOURCES_DIR}/crypto/dso/dso.h
      ${OPENSSL_SOURCES_DIR}/crypto/ebcdic.h
      ${OPENSSL_SOURCES_DIR}/crypto/ec/ec.h
      ${OPENSSL_SOURCES_DIR}/crypto/ecdh/ecdh.h
      ${OPENSSL_SOURCES_DIR}/crypto/ecdsa/ecdsa.h
      ${OPENSSL_SOURCES_DIR}/crypto/engine/engine.h
      ${OPENSSL_SOURCES_DIR}/crypto/err/err.h
      ${OPENSSL_SOURCES_DIR}/crypto/evp/evp.h
      ${OPENSSL_SOURCES_DIR}/crypto/hmac/hmac.h
      ${OPENSSL_SOURCES_DIR}/crypto/idea/idea.h
      ${OPENSSL_SOURCES_DIR}/crypto/jpake/jpake.h
      ${OPENSSL_SOURCES_DIR}/crypto/krb5/krb5_asn.h
      ${OPENSSL_SOURCES_DIR}/crypto/lhash/lhash.h
      ${OPENSSL_SOURCES_DIR}/crypto/md2/md2.h
      ${OPENSSL_SOURCES_DIR}/crypto/md4/md4.h
      ${OPENSSL_SOURCES_DIR}/crypto/md5/md5.h
      ${OPENSSL_SOURCES_DIR}/crypto/mdc2/mdc2.h
      ${OPENSSL_SOURCES_DIR}/crypto/modes/modes.h
      ${OPENSSL_SOURCES_DIR}/crypto/objects/obj_mac.h
      ${OPENSSL_SOURCES_DIR}/crypto/objects/objects.h
      ${OPENSSL_SOURCES_DIR}/crypto/ocsp/ocsp.h
      ${OPENSSL_SOURCES_DIR}/crypto/opensslconf.h
      ${OPENSSL_SOURCES_DIR}/crypto/opensslv.h
      ${OPENSSL_SOURCES_DIR}/crypto/ossl_typ.h
      ${OPENSSL_SOURCES_DIR}/crypto/pem/pem.h
      ${OPENSSL_SOURCES_DIR}/crypto/pem/pem2.h
      ${OPENSSL_SOURCES_DIR}/crypto/pkcs12/pkcs12.h
      ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/pkcs7.h
      ${OPENSSL_SOURCES_DIR}/crypto/pqueue/pqueue.h
      ${OPENSSL_SOURCES_DIR}/crypto/rand/rand.h
      ${OPENSSL_SOURCES_DIR}/crypto/rc2/rc2.h
      ${OPENSSL_SOURCES_DIR}/crypto/rc4/rc4.h
      ${OPENSSL_SOURCES_DIR}/crypto/rc5/rc5.h
      ${OPENSSL_SOURCES_DIR}/crypto/ripemd/ripemd.h
      ${OPENSSL_SOURCES_DIR}/crypto/rsa/rsa.h
      ${OPENSSL_SOURCES_DIR}/crypto/seed/seed.h
      ${OPENSSL_SOURCES_DIR}/crypto/sha/sha.h
      ${OPENSSL_SOURCES_DIR}/crypto/srp/srp.h
      ${OPENSSL_SOURCES_DIR}/crypto/stack/safestack.h
      ${OPENSSL_SOURCES_DIR}/crypto/stack/stack.h
      ${OPENSSL_SOURCES_DIR}/crypto/store/store.h
      ${OPENSSL_SOURCES_DIR}/crypto/symhacks.h
      ${OPENSSL_SOURCES_DIR}/crypto/ts/ts.h
      ${OPENSSL_SOURCES_DIR}/crypto/txt_db/txt_db.h
      ${OPENSSL_SOURCES_DIR}/crypto/ui/ui.h
      ${OPENSSL_SOURCES_DIR}/crypto/ui/ui_compat.h
      ${OPENSSL_SOURCES_DIR}/crypto/whrlpool/whrlpool.h
      ${OPENSSL_SOURCES_DIR}/crypto/x509/x509.h
      ${OPENSSL_SOURCES_DIR}/crypto/x509/x509_vfy.h
      ${OPENSSL_SOURCES_DIR}/crypto/x509v3/x509v3.h
      ${OPENSSL_SOURCES_DIR}/e_os2.h
      ${OPENSSL_SOURCES_DIR}/ssl/dtls1.h
      ${OPENSSL_SOURCES_DIR}/ssl/kssl.h
      ${OPENSSL_SOURCES_DIR}/ssl/srtp.h
      ${OPENSSL_SOURCES_DIR}/ssl/ssl.h
      ${OPENSSL_SOURCES_DIR}/ssl/ssl2.h
      ${OPENSSL_SOURCES_DIR}/ssl/ssl23.h
      ${OPENSSL_SOURCES_DIR}/ssl/ssl3.h
      ${OPENSSL_SOURCES_DIR}/ssl/tls1.h
      )
    file(COPY ${header} DESTINATION ${OPENSSL_SOURCES_DIR}/include/openssl)
  endforeach()

  file(RENAME
    ${OPENSSL_SOURCES_DIR}/include/openssl/e_os2.h
    ${OPENSSL_SOURCES_DIR}/include/openssl/e_os2_source.h)

  # The following patch of "e_os2.h" prevents from building OpenSSL
  # as a DLL under Windows. Otherwise, symbols have inconsistent
  # linkage if ${OPENSSL_SOURCES} is used to create a DLL (notably
  # if building an Orthanc plugin such as MySQL).
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
endif()

add_definitions(
  -DOPENSSL_THREADS
  -DOPENSSL_IA32_SSE2
  -DOPENSSL_NO_ASM
  -DOPENSSL_NO_DYNAMIC_ENGINE
  -DNO_WINDOWS_BRAINDEATH

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
  )

include_directories(
  ${OPENSSL_SOURCES_DIR}
  ${OPENSSL_SOURCES_DIR}/crypto
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/include
  )

set(OPENSSL_SOURCES_SUBDIRS
  ${OPENSSL_SOURCES_DIR}/crypto
  ${OPENSSL_SOURCES_DIR}/crypto/aes
  ${OPENSSL_SOURCES_DIR}/crypto/asn1
  ${OPENSSL_SOURCES_DIR}/crypto/bio
  ${OPENSSL_SOURCES_DIR}/crypto/bn
  ${OPENSSL_SOURCES_DIR}/crypto/buffer
  ${OPENSSL_SOURCES_DIR}/crypto/cmac
  ${OPENSSL_SOURCES_DIR}/crypto/cms
  ${OPENSSL_SOURCES_DIR}/crypto/comp
  ${OPENSSL_SOURCES_DIR}/crypto/conf
  ${OPENSSL_SOURCES_DIR}/crypto/des
  ${OPENSSL_SOURCES_DIR}/crypto/dh
  ${OPENSSL_SOURCES_DIR}/crypto/dsa
  ${OPENSSL_SOURCES_DIR}/crypto/dso
  ${OPENSSL_SOURCES_DIR}/crypto/engine
  ${OPENSSL_SOURCES_DIR}/crypto/err
  ${OPENSSL_SOURCES_DIR}/crypto/evp
  ${OPENSSL_SOURCES_DIR}/crypto/hmac
  ${OPENSSL_SOURCES_DIR}/crypto/lhash
  ${OPENSSL_SOURCES_DIR}/crypto/md4
  ${OPENSSL_SOURCES_DIR}/crypto/md5
  ${OPENSSL_SOURCES_DIR}/crypto/modes
  ${OPENSSL_SOURCES_DIR}/crypto/objects
  ${OPENSSL_SOURCES_DIR}/crypto/ocsp
  ${OPENSSL_SOURCES_DIR}/crypto/pem
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs12
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7
  ${OPENSSL_SOURCES_DIR}/crypto/pqueue
  ${OPENSSL_SOURCES_DIR}/crypto/rand
  ${OPENSSL_SOURCES_DIR}/crypto/rsa
  ${OPENSSL_SOURCES_DIR}/crypto/sha
  ${OPENSSL_SOURCES_DIR}/crypto/srp
  ${OPENSSL_SOURCES_DIR}/crypto/stack
  ${OPENSSL_SOURCES_DIR}/crypto/ts
  ${OPENSSL_SOURCES_DIR}/crypto/txt_db
  ${OPENSSL_SOURCES_DIR}/crypto/ui
  ${OPENSSL_SOURCES_DIR}/crypto/x509
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3
  ${OPENSSL_SOURCES_DIR}/ssl
  )

if (ENABLE_OPENSSL_ENGINES)
  list(APPEND OPENSSL_SOURCES_SUBDIRS
    ${OPENSSL_SOURCES_DIR}/engines
    )
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
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_unix.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_vms.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_win32.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_wince.c
  ${OPENSSL_SOURCES_DIR}/crypto/armcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/bf/bfs.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/bio/bss_rtcp.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/exp.c
  ${OPENSSL_SOURCES_DIR}/crypto/conf/cnf_save.c
  ${OPENSSL_SOURCES_DIR}/crypto/conf/test.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/des.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/des3s.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/des/des_opts.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/dess.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/des/read_pwd.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/speed.c
  ${OPENSSL_SOURCES_DIR}/crypto/evp/e_dsa.c
  ${OPENSSL_SOURCES_DIR}/crypto/evp/m_ripemd.c
  ${OPENSSL_SOURCES_DIR}/crypto/lhash/lh_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/md4/md4.c
  ${OPENSSL_SOURCES_DIR}/crypto/md4/md4s.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/md4/md4test.c
  ${OPENSSL_SOURCES_DIR}/crypto/md5/md5s.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/bio_ber.c
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/pk7_enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/ppccap.c
  ${OPENSSL_SOURCES_DIR}/crypto/rand/randtest.c
  ${OPENSSL_SOURCES_DIR}/crypto/s390xcap.c
  ${OPENSSL_SOURCES_DIR}/crypto/sparcv9cap.c
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3/tabtest.c
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3/v3conf.c
  ${OPENSSL_SOURCES_DIR}/ssl/ssl_task.c
  ${OPENSSL_SOURCES_DIR}/crypto/LPdir_nyi.c
  ${OPENSSL_SOURCES_DIR}/crypto/aes/aes_x86core.c
  ${OPENSSL_SOURCES_DIR}/crypto/bio/bss_dgram.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bntest.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/expspeed.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/exptest.c
  ${OPENSSL_SOURCES_DIR}/crypto/engine/enginetest.c
  ${OPENSSL_SOURCES_DIR}/crypto/evp/evp_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/hmac/hmactest.c
  ${OPENSSL_SOURCES_DIR}/crypto/md5/md5.c
  ${OPENSSL_SOURCES_DIR}/crypto/md5/md5test.c
  ${OPENSSL_SOURCES_DIR}/crypto/o_dir_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/dec.c
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/enc.c
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/sign.c
  ${OPENSSL_SOURCES_DIR}/crypto/pkcs7/verify.c
  ${OPENSSL_SOURCES_DIR}/crypto/rsa/rsa_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha1.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha1t.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha1test.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha256t.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/sha512t.c
  ${OPENSSL_SOURCES_DIR}/crypto/sha/shatest.c
  ${OPENSSL_SOURCES_DIR}/crypto/srp/srptest.c

  ${OPENSSL_SOURCES_DIR}/crypto/bn/divtest.c
  ${OPENSSL_SOURCES_DIR}/crypto/bn/bnspeed.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/destest.c
  ${OPENSSL_SOURCES_DIR}/crypto/dh/p192.c
  ${OPENSSL_SOURCES_DIR}/crypto/dh/p512.c
  ${OPENSSL_SOURCES_DIR}/crypto/dh/p1024.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/rpw.c
  ${OPENSSL_SOURCES_DIR}/ssl/ssltest.c
  ${OPENSSL_SOURCES_DIR}/crypto/dsa/dsagen.c
  ${OPENSSL_SOURCES_DIR}/crypto/dsa/dsatest.c
  ${OPENSSL_SOURCES_DIR}/crypto/dh/dhtest.c
  ${OPENSSL_SOURCES_DIR}/crypto/pqueue/pq_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/des/ncbc_enc.c

  ${OPENSSL_SOURCES_DIR}/crypto/evp/evp_extra_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/evp/verify_extra_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/x509/verify_extra_test.c
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3/v3prin.c
  ${OPENSSL_SOURCES_DIR}/crypto/x509v3/v3nametest.c
  ${OPENSSL_SOURCES_DIR}/crypto/constant_time_test.c

  ${OPENSSL_SOURCES_DIR}/ssl/heartbeat_test.c
  ${OPENSSL_SOURCES_DIR}/ssl/fatalerrtest.c
  ${OPENSSL_SOURCES_DIR}/ssl/dtlstest.c
  ${OPENSSL_SOURCES_DIR}/ssl/bad_dtls_test.c
  ${OPENSSL_SOURCES_DIR}/ssl/clienthellotest.c
  ${OPENSSL_SOURCES_DIR}/ssl/sslv2conftest.c

  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ecp_nistz256_table.c
  ${OPENSSL_SOURCES_DIR}/crypto/ec/ectest.c
  ${OPENSSL_SOURCES_DIR}/crypto/ecdh/ecdhtest.c
  ${OPENSSL_SOURCES_DIR}/crypto/ecdsa/ecdsatest.c
  )


if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set_source_files_properties(
    ${OPENSSL_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS
    "OPENSSL_SYSNAME_WIN32;SO_WIN32;WIN32_LEAN_AND_MEAN;L_ENDIAN")

  if (ENABLE_OPENSSL_ENGINES)
    link_libraries(crypt32)
  endif()
endif()
