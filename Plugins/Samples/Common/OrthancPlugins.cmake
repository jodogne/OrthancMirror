include(CheckLibraryExists)


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  link_libraries(uuid)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pthread")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  link_libraries(rpcrt4 ws2_32 secur32)
  if (CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
  endif()

  CHECK_LIBRARY_EXISTS(winpthread pthread_create "" HAVE_WIN_PTHREAD)
  if (HAVE_WIN_PTHREAD)
    # This line is necessary to compile with recent versions of MinGW,
    # otherwise "libwinpthread-1.dll" is not statically linked.
    SET(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic")
  endif()
endif ()


if (CMAKE_COMPILER_IS_GNUCXX)
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${ORTHANC_ROOT}/Plugins/Samples/Common/VersionScript.map -Wl,--no-undefined")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  link_libraries(dl rt)
endif()


include_directories(${ORTHANC_ROOT}/Plugins/Include/)

if (MSVC)
  include_directories(${ORTHANC_ROOT}/Resources/ThirdParty/VisualStudio/)
endif()
