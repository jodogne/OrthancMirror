/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once


/********************************************************************
 ** LINUX-LIKE ARCHITECTURES
 ********************************************************************/

#if defined(__LSB_VERSION__)
// Linux Standard Base (LSB) does not come with be16toh, be32toh, and
// be64toh
#  define ORTHANC_HAS_BUILTIN_BYTE_SWAP 0
#  include <endian.h>
#elif defined(__linux__) || defined(__EMSCRIPTEN__)
#  define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#  include <endian.h>
#endif


/********************************************************************
 ** WINDOWS ARCHITECTURES
 **
 ** On Windows x86, "host" will always be little-endian ("le").
 ********************************************************************/

#if defined(_WIN32)
#  if defined(_MSC_VER)
//   Visual Studio - http://msdn.microsoft.com/en-us/library/a3140177.aspx
#    define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#    define be16toh(x) _byteswap_ushort(x)
#    define be32toh(x) _byteswap_ulong(x)
#    define be64toh(x) _byteswap_uint64(x)
#  elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
//   MinGW >= 4.3 - Use builtin intrinsic for byte swapping
#    define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#    define be16toh(x) __builtin_bswap16(x)
#    define be32toh(x) __builtin_bswap32(x)
#    define be64toh(x) __builtin_bswap64(x)
#  else
//   MinGW <= 4.2, we must manually implement the byte swapping (*)
#    define ORTHANC_HAS_BUILTIN_BYTE_SWAP 0
#    define be16toh(x) __orthanc_bswap16(x)
#    define be32toh(x) __orthanc_bswap32(x)
#    define be64toh(x) __orthanc_bswap64(x)
#  endif

#  define htobe16(x) be16toh(x)
#  define htobe32(x) be32toh(x)
#  define htobe64(x) be64toh(x)

#  define htole16(x) x
#  define htole32(x) x
#  define htole64(x) x

#  define le16toh(x) x
#  define le32toh(x) x
#  define le64toh(x) x
#endif


/********************************************************************
 ** FREEBSD ARCHITECTURES
 ********************************************************************/

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#  define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#  include <arpa/inet.h>
#endif


/********************************************************************
 ** OPENBSD ARCHITECTURES
 ********************************************************************/

#if defined(__OpenBSD__)
#  define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#  include <endian.h>
#endif


/********************************************************************
 ** APPLE ARCHITECTURES (including OS X)
 ********************************************************************/

#if defined(__APPLE__)
#  define ORTHANC_HAS_BUILTIN_BYTE_SWAP 1
#  include <libkern/OSByteOrder.h>
#  define be16toh(x) OSSwapBigToHostInt16(x)
#  define be32toh(x) OSSwapBigToHostInt32(x)
#  define be64toh(x) OSSwapBigToHostInt64(x)

#  define htobe16(x) OSSwapHostToBigInt16(x)
#  define htobe32(x) OSSwapHostToBigInt32(x)
#  define htobe64(x) OSSwapHostToBigInt64(x)

#  define htole16(x) OSSwapHostToLittleInt16(x)
#  define htole32(x) OSSwapHostToLittleInt32(x)
#  define htole64(x) OSSwapHostToLittleInt64(x)

#  define le16toh(x) OSSwapLittleToHostInt16(x)
#  define le32toh(x) OSSwapLittleToHostInt32(x)
#  define le64toh(x) OSSwapLittleToHostInt64(x)
#endif


/********************************************************************
 ** PORTABLE (BUT SLOW) IMPLEMENTATION OF BYTE-SWAPPING
 ********************************************************************/

#if ORTHANC_HAS_BUILTIN_BYTE_SWAP != 1

#include <stdint.h>

static inline uint16_t __orthanc_bswap16(uint16_t a)
{
  /**
   * Note that an alternative implementation was included in Orthanc
   * 1.4.0 and 1.4.1:
   * 
   *  # hg log -p -r 2706
   *
   * This alternative implementation only hid an underlying problem
   * with pointer alignment on some architectures, and was thus
   * reverted. Check out issue #99:
   * https://bugs.orthanc-server.com/show_bug.cgi?id=99
   **/
  return (a << 8) | (a >> 8);
}

static inline uint32_t __orthanc_bswap32(uint32_t a)
{
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&a);
  return (static_cast<uint32_t>(p[0]) << 24 |
          static_cast<uint32_t>(p[1]) << 16 |
          static_cast<uint32_t>(p[2]) << 8 |
          static_cast<uint32_t>(p[3]));
}

static inline uint64_t __orthanc_bswap64(uint64_t a)
{
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&a);
  return (static_cast<uint64_t>(p[0]) << 56 |
          static_cast<uint64_t>(p[1]) << 48 |
          static_cast<uint64_t>(p[2]) << 40 |
          static_cast<uint64_t>(p[3]) << 32 |
          static_cast<uint64_t>(p[4]) << 24 |
          static_cast<uint64_t>(p[5]) << 16 |
          static_cast<uint64_t>(p[6]) << 8 |
          static_cast<uint64_t>(p[7]));
}

#if defined(_WIN32)
// Implemented above (*)
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define be16toh(x) __orthanc_bswap16(x)
#    define be32toh(x) __orthanc_bswap32(x)
#    define be64toh(x) __orthanc_bswap64(x)
#    define htobe16(x) __orthanc_bswap16(x)
#    define htobe32(x) __orthanc_bswap32(x)
#    define htobe64(x) __orthanc_bswap64(x)
#    define htole16(x) x
#    define htole32(x) x
#    define htole64(x) x
#    define le16toh(x) x
#    define le32toh(x) x
#    define le64toh(x) x
#  elif __BYTE_ORDER == __BIG_ENDIAN
#    define be16toh(x) x
#    define be32toh(x) x
#    define be64toh(x) x
#    define htobe16(x) x
#    define htobe32(x) x
#    define htobe64(x) x
#    define htole16(x) __orthanc_bswap16(x)
#    define htole32(x) __orthanc_bswap32(x)
#    define htole64(x) __orthanc_bswap64(x)
#    define le16toh(x) __orthanc_bswap16(x)
#    define le32toh(x) __orthanc_bswap32(x)
#    define le64toh(x) __orthanc_bswap64(x)
#  else
#    error Please support your platform here
#  endif
#else
#  error Please support your platform here
#endif

#endif
