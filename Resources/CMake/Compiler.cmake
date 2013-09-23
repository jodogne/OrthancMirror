# This file sets all the compiler-related flags

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-long-long -Wno-implicit-function-declaration")  
  # --std=c99 makes libcurl not to compile
  # -pedantic gives a lot of warnings on OpenSSL 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wno-long-long -Wno-variadic-macros")
elseif (${MSVC})
  # http://stackoverflow.com/a/6510446
  foreach(flag_var
    CMAKE_C_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE 
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL 
    CMAKE_CXX_FLAGS_MINSIZEREL 
    CMAKE_C_FLAGS_RELWITHDEBINFO 
    CMAKE_CXX_FLAGS_RELWITHDEBINFO) 
    string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
  endforeach(flag_var)
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS=1
    -D_CRT_SECURE_NO_DEPRECATE=1
    )
  include_directories(${CMAKE_SOURCE_DIR}/Resources/VisualStudio)
  link_libraries(netapi32)
endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -I${LSB_PATH}/include -I${LSB_PATH}/include/c++ -I${LSB_PATH}/include/c++/backward -fpermissive")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${LSB_LIBPATH}")
  endif()

  add_definitions(
    -D_LARGEFILE64_SOURCE=1 
    -D_FILE_OFFSET_BITS=64
    )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

  # Remove the "-rdynamic" option
  # http://www.mail-archive.com/cmake@cmake.org/msg08837.html
  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
  link_libraries(uuid pthread rt)

elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  add_definitions(
    -DWINVER=0x0501
    -D_CRT_SECURE_NO_WARNINGS=1
    )
  link_libraries(rpcrt4 ws2_32)

  if (${CMAKE_COMPILER_IS_GNUCXX})
    # This is a patch for MinGW64
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-multiple-definition -static-libgcc -static-libstdc++")
  endif()

endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  CHECK_INCLUDE_FILES(rpc.h HAVE_UUID_H)
else()
  CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_H)
endif()

if (NOT HAVE_UUID_H)
  message(FATAL_ERROR "Please install the uuid-dev package")
endif()

if (${STATIC_BUILD})
  add_definitions(-DORTHANC_STATIC=1)
else()
  add_definitions(-DORTHANC_STATIC=0)
endif()
