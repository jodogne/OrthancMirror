# http://sourceforge.net/apps/trac/mingw-w64/wiki/GeneralUsageInstructions

# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

# Detect the prefix of the mingw-w64 compiler
execute_process(
  COMMAND uname -p
  OUTPUT_VARIABLE MINGW64_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

if (${MINGW64_ARCHITECTURE} STREQUAL "x86_64")
  set(MINGW64_PREFIX "x86_64")
else()
  set(MINGW64_PREFIX "i686")
endif()
  
# which compilers to use for C and C++
set(CMAKE_C_COMPILER ${MINGW64_PREFIX}-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER ${MINGW64_PREFIX}-w64-mingw32-g++)
set(CMAKE_RC_COMPILER ${MINGW64_PREFIX}-w64-mingw32-windres)

# here is the target environment located
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
