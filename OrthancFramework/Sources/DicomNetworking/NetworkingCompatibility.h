/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#ifdef _WIN32
/**
 * "The maximum length, in bytes, of the string returned in the buffer 
 * pointed to by the name parameter is dependent on the namespace provider,
 * but this string must be 256 bytes or less.
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738527(v=vs.85).aspx
 **/
#  define HOST_NAME_MAX 256
#  include <winsock.h>
#endif 


#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
/**
 * TO IMPROVE: "_POSIX_HOST_NAME_MAX is only the minimum value that
 * HOST_NAME_MAX can ever have [...] Therefore you cannot allocate an
 * array of size _POSIX_HOST_NAME_MAX, invoke gethostname() and expect
 * that the result will fit."
 * http://lists.gnu.org/archive/html/bug-gnulib/2009-08/msg00128.html
 **/
#  define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif
