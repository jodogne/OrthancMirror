if (STATIC_BUILD OR NOT USE_SYSTEM_LIBPNG)
  SET(LIBPNG_SOURCES_DIR ${CMAKE_BINARY_DIR}/libpng-1.5.12)
  SET(LIBPNG_URL "http://www.orthanc-server.com/downloads/third-party/libpng-1.5.12.tar.gz")
  SET(LIBPNG_MD5 "8ea7f60347a306c5faf70b977fa80e28")

  DownloadPackage(${LIBPNG_MD5} ${LIBPNG_URL} "${LIBPNG_SOURCES_DIR}")

  include_directories(
    ${LIBPNG_SOURCES_DIR}
    )

  configure_file(
    ${LIBPNG_SOURCES_DIR}/scripts/pnglibconf.h.prebuilt
    ${LIBPNG_SOURCES_DIR}/pnglibconf.h
    )

  set(LIBPNG_SOURCES
    #${LIBPNG_SOURCES_DIR}/example.c
    ${LIBPNG_SOURCES_DIR}/png.c
    ${LIBPNG_SOURCES_DIR}/pngerror.c
    ${LIBPNG_SOURCES_DIR}/pngget.c
    ${LIBPNG_SOURCES_DIR}/pngmem.c
    ${LIBPNG_SOURCES_DIR}/pngpread.c
    ${LIBPNG_SOURCES_DIR}/pngread.c
    ${LIBPNG_SOURCES_DIR}/pngrio.c
    ${LIBPNG_SOURCES_DIR}/pngrtran.c
    ${LIBPNG_SOURCES_DIR}/pngrutil.c
    ${LIBPNG_SOURCES_DIR}/pngset.c
    #${LIBPNG_SOURCES_DIR}/pngtest.c
    ${LIBPNG_SOURCES_DIR}/pngtrans.c
    ${LIBPNG_SOURCES_DIR}/pngwio.c
    ${LIBPNG_SOURCES_DIR}/pngwrite.c
    ${LIBPNG_SOURCES_DIR}/pngwtran.c
    ${LIBPNG_SOURCES_DIR}/pngwutil.c
    )

  #set_property(
  #  SOURCE ${LIBPNG_SOURCES}
  #  PROPERTY COMPILE_FLAGS -UHAVE_CONFIG_H)

  add_definitions(
    -DPNG_NO_CONSOLE_IO=1
    -DPNG_NO_STDIO=1
    # The following declaration avoids "__declspec(dllexport)" in
    # libpng to prevent publicly exposing its symbols by the DLLs
    -DPNG_IMPEXP=
    )

  source_group(ThirdParty\\libpng REGULAR_EXPRESSION ${LIBPNG_SOURCES_DIR}/.*)

else()
  include(FindPNG)

  if (NOT ${PNG_FOUND})
    message(FATAL_ERROR "Unable to find libpng")
  endif()

  include_directories(${PNG_INCLUDE_DIRS})
  link_libraries(${PNG_LIBRARIES})
  add_definitions(${PNG_DEFINITIONS})
endif()
