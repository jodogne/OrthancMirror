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


if (STATIC_BUILD OR NOT USE_SYSTEM_MONGOOSE)
  SET(MONGOOSE_SOURCES_DIR ${CMAKE_BINARY_DIR}/mongoose)

  if (IS_DIRECTORY "${MONGOOSE_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  if (0)
    # Use Mongoose 3.1
    DownloadPackage(
      "e718fc287b4eb1bd523be3fa00942bb0"
      "https://orthanc.uclouvain.be/third-party-downloads/mongoose-3.1.tgz"
      "${MONGOOSE_SOURCES_DIR}")
    
    add_definitions(-DMONGOOSE_USE_CALLBACKS=0)
    set(MONGOOSE_PATCH ${CMAKE_CURRENT_LIST_DIR}/../Patches/mongoose-3.1-patch.diff)

  else() 
    # Use Mongoose 3.8
    DownloadPackage(
      "7e3296295072792cdc3c633f9404e0c3"
      "https://orthanc.uclouvain.be/third-party-downloads/mongoose-3.8.tgz"
      "${MONGOOSE_SOURCES_DIR}")
    
    add_definitions(-DMONGOOSE_USE_CALLBACKS=1)
    set(MONGOOSE_PATCH ${CMAKE_CURRENT_LIST_DIR}/../Patches/mongoose-3.8-patch.diff)
  endif()

  # Patch mongoose
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -N mongoose.c 
    INPUT_FILE ${MONGOOSE_PATCH}
    WORKING_DIRECTORY ${MONGOOSE_SOURCES_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure AND FirstRun)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  include_directories(
    ${MONGOOSE_SOURCES_DIR}
    )

  set(MONGOOSE_SOURCES
    ${MONGOOSE_SOURCES_DIR}/mongoose.c
    )


  if (ENABLE_SSL)
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
      link_libraries(dl)
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from mongoose
      )
  endif()


  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (CMAKE_COMPILER_IS_GNUCXX)
      # This is a patch for MinGW64
      add_definitions(-D_TIMESPEC_DEFINED=1)
    endif()
  endif()

  source_group(ThirdParty\\Mongoose REGULAR_EXPRESSION ${MONGOOSE_SOURCES_DIR}/.*)

else()
  CHECK_INCLUDE_FILE_CXX(mongoose.h HAVE_MONGOOSE_H)
  if (NOT HAVE_MONGOOSE_H)
    message(FATAL_ERROR "Please install the mongoose-devel package")
  endif()

  CHECK_LIBRARY_EXISTS(mongoose mg_start "" HAVE_MONGOOSE_LIB)
  if (NOT HAVE_MONGOOSE_LIB)
    message(FATAL_ERROR "Please install the mongoose-devel package")
  endif()

  if (SYSTEM_MONGOOSE_USE_CALLBACKS)
    add_definitions(-DMONGOOSE_USE_CALLBACKS=1)
  else()
    add_definitions(-DMONGOOSE_USE_CALLBACKS=0)
  endif()

  link_libraries(mongoose)
endif()
