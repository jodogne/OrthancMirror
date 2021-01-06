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


if (STATIC_BUILD OR NOT USE_SYSTEM_PUGIXML)
  set(PUGIXML_SOURCES_DIR ${CMAKE_BINARY_DIR}/pugixml-1.9)
  set(PUGIXML_MD5 "7286ee2ed11376b6b780ced19fae0b64")
  set(PUGIXML_URL "http://orthanc.osimis.io/ThirdPartyDownloads/pugixml-1.9.tar.gz")

  DownloadPackage(${PUGIXML_MD5} ${PUGIXML_URL} "${PUGIXML_SOURCES_DIR}")

  include_directories(
    ${PUGIXML_SOURCES_DIR}/src
    )

  set(PUGIXML_SOURCES
    #${PUGIXML_SOURCES_DIR}/src/vlog_is_on.cc
    ${PUGIXML_SOURCES_DIR}/src/pugixml.cpp
    )

  source_group(ThirdParty\\pugixml REGULAR_EXPRESSION ${PUGIXML_SOURCES_DIR}/.*)

else()
  CHECK_INCLUDE_FILE_CXX(pugixml.hpp HAVE_PUGIXML_H)
  if (NOT HAVE_PUGIXML_H)
    message(FATAL_ERROR "Please install the libpugixml-dev package")
  endif()

  link_libraries(pugixml)
endif()
