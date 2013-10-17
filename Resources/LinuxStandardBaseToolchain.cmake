INCLUDE(CMakeForceCompiler)

SET(LSB_PATH $ENV{LSB_PATH})
SET(LSB_TARGET_VERSION "4.0")

IF ("${LSB_PATH}" STREQUAL "")
  SET(LSB_PATH "/opt/lsb")
ENDIF()

message("Using the following Linux Standard Base: ${LSB_PATH}")

IF (EXISTS ${LSB_PATH}/lib64)
  SET(LSB_TARGET_PROCESSOR "x86_64")
  SET(LSB_LIBPATH ${LSB_PATH}/lib64-${LSB_TARGET_VERSION})
ELSEIF (EXISTS ${LSB_PATH}/lib)
  SET(LSB_TARGET_PROCESSOR "x86")
  SET(LSB_LIBPATH ${LSB_PATH}/lib-${LSB_TARGET_VERSION})
ELSE()
  MESSAGE(FATAL_ERROR "Unable to detect the target processor architecture. Check the LSB_PATH environment variable.")
ENDIF()

SET(LSB_CPPPATH ${LSB_PATH}/include)
SET(PKG_CONFIG_PATH ${LSB_LIBPATH}/pkgconfig/)

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION LinuxStandardBase)
SET(CMAKE_SYSTEM_PROCESSOR ${LSB_TARGET_PROCESSOR})

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER ${LSB_PATH}/bin/lsbcc)
CMAKE_FORCE_CXX_COMPILER(${LSB_PATH}/bin/lsbc++ GNU)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH ${LSB_PATH})

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
