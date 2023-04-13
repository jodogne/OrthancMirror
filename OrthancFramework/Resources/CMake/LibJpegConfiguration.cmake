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


if (STATIC_BUILD OR NOT USE_SYSTEM_LIBJPEG)
  set(LIBJPEG_SOURCES_DIR ${CMAKE_BINARY_DIR}/jpeg-9c)
  DownloadPackage(
    "93c62597eeef81a84d988bccbda1e990"
    "https://orthanc.uclouvain.be/third-party-downloads/jpegsrc.v9c.tar.gz"
    "${LIBJPEG_SOURCES_DIR}")

  include_directories(
    ${LIBJPEG_SOURCES_DIR}
    )

  list(APPEND LIBJPEG_SOURCES 
    ${LIBJPEG_SOURCES_DIR}/jaricom.c
    ${LIBJPEG_SOURCES_DIR}/jcapimin.c
    ${LIBJPEG_SOURCES_DIR}/jcapistd.c
    ${LIBJPEG_SOURCES_DIR}/jcarith.c
    ${LIBJPEG_SOURCES_DIR}/jccoefct.c
    ${LIBJPEG_SOURCES_DIR}/jccolor.c
    ${LIBJPEG_SOURCES_DIR}/jcdctmgr.c
    ${LIBJPEG_SOURCES_DIR}/jchuff.c
    ${LIBJPEG_SOURCES_DIR}/jcinit.c
    ${LIBJPEG_SOURCES_DIR}/jcmarker.c
    ${LIBJPEG_SOURCES_DIR}/jcmaster.c
    ${LIBJPEG_SOURCES_DIR}/jcomapi.c
    ${LIBJPEG_SOURCES_DIR}/jcparam.c
    ${LIBJPEG_SOURCES_DIR}/jcprepct.c
    ${LIBJPEG_SOURCES_DIR}/jcsample.c
    ${LIBJPEG_SOURCES_DIR}/jctrans.c
    ${LIBJPEG_SOURCES_DIR}/jdapimin.c
    ${LIBJPEG_SOURCES_DIR}/jdapistd.c
    ${LIBJPEG_SOURCES_DIR}/jdarith.c
    ${LIBJPEG_SOURCES_DIR}/jdatadst.c
    ${LIBJPEG_SOURCES_DIR}/jdatasrc.c
    ${LIBJPEG_SOURCES_DIR}/jdcoefct.c
    ${LIBJPEG_SOURCES_DIR}/jdcolor.c
    ${LIBJPEG_SOURCES_DIR}/jddctmgr.c
    ${LIBJPEG_SOURCES_DIR}/jdhuff.c
    ${LIBJPEG_SOURCES_DIR}/jdinput.c
    ${LIBJPEG_SOURCES_DIR}/jcmainct.c
    ${LIBJPEG_SOURCES_DIR}/jdmainct.c
    ${LIBJPEG_SOURCES_DIR}/jdmarker.c
    ${LIBJPEG_SOURCES_DIR}/jdmaster.c
    ${LIBJPEG_SOURCES_DIR}/jdmerge.c
    ${LIBJPEG_SOURCES_DIR}/jdpostct.c
    ${LIBJPEG_SOURCES_DIR}/jdsample.c
    ${LIBJPEG_SOURCES_DIR}/jdtrans.c
    ${LIBJPEG_SOURCES_DIR}/jerror.c
    ${LIBJPEG_SOURCES_DIR}/jfdctflt.c
    ${LIBJPEG_SOURCES_DIR}/jfdctfst.c
    ${LIBJPEG_SOURCES_DIR}/jfdctint.c
    ${LIBJPEG_SOURCES_DIR}/jidctflt.c
    ${LIBJPEG_SOURCES_DIR}/jidctfst.c
    ${LIBJPEG_SOURCES_DIR}/jidctint.c
    #${LIBJPEG_SOURCES_DIR}/jmemansi.c
    #${LIBJPEG_SOURCES_DIR}/jmemdos.c
    #${LIBJPEG_SOURCES_DIR}/jmemmac.c
    ${LIBJPEG_SOURCES_DIR}/jmemmgr.c
    #${LIBJPEG_SOURCES_DIR}/jmemname.c
    ${LIBJPEG_SOURCES_DIR}/jmemnobs.c
    ${LIBJPEG_SOURCES_DIR}/jquant1.c
    ${LIBJPEG_SOURCES_DIR}/jquant2.c
    ${LIBJPEG_SOURCES_DIR}/jutils.c

    # ${LIBJPEG_SOURCES_DIR}/rdbmp.c
    # ${LIBJPEG_SOURCES_DIR}/rdcolmap.c
    # ${LIBJPEG_SOURCES_DIR}/rdgif.c
    # ${LIBJPEG_SOURCES_DIR}/rdppm.c
    # ${LIBJPEG_SOURCES_DIR}/rdrle.c
    # ${LIBJPEG_SOURCES_DIR}/rdswitch.c
    # ${LIBJPEG_SOURCES_DIR}/rdtarga.c
    # ${LIBJPEG_SOURCES_DIR}/transupp.c
    # ${LIBJPEG_SOURCES_DIR}/wrbmp.c
    # ${LIBJPEG_SOURCES_DIR}/wrgif.c
    # ${LIBJPEG_SOURCES_DIR}/wrppm.c
    # ${LIBJPEG_SOURCES_DIR}/wrrle.c
    # ${LIBJPEG_SOURCES_DIR}/wrtarga.c
    )

  configure_file(
    ${LIBJPEG_SOURCES_DIR}/jconfig.txt
    ${LIBJPEG_SOURCES_DIR}/jconfig.h COPYONLY
    )

  source_group(ThirdParty\\libjpeg REGULAR_EXPRESSION ${LIBJPEG_SOURCES_DIR}/.*)

else()
  include(FindJPEG)

  if (NOT JPEG_FOUND)
    message(FATAL_ERROR "Unable to find libjpeg")
  endif()

  include_directories(${JPEG_INCLUDE_DIR})
  link_libraries(${JPEG_LIBRARIES})
endif()
