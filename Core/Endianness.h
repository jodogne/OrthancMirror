/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once


/********************************************************************
 ** LINUX ARCHITECTURES
 ********************************************************************/

#if defined(__linux)
#  include <endian.h>
#endif


/********************************************************************
 ** WINDOWS ARCHITECTURES
 **
 ** On Windows x86, "host" will always be little-endian ("le").
 ********************************************************************/

#if defined(_WIN32)
#  if defined(_MSC_VER)
//   http://msdn.microsoft.com/en-us/library/a3140177.aspx
#    define be16toh(x) _byteswap_ushort(x)
#    define be32toh(x) _byteswap_ulong(x)
#    define be64toh(x) _byteswap_uint64(x)
#  else   // MinGW
#    define be16toh(x) __builtin_bswap16(x)
#    define be32toh(x) __builtin_bswap32(x)
#    define be64toh(x) __builtin_bswap64(x)
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
#  include <arpa/inet.h>
#endif


/********************************************************************
 ** APPLE ARCHITECTURES (including OS X)
 ********************************************************************/

#if defined(__APPLE__)
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
