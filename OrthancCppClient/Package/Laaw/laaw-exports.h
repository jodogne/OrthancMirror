/**
 * Laaw - Lightweight, Automated API Wrapper
 * Copyright (C) 2010-2013 Jomago - Alain Mazy, Benjamin Golinvaux,
 * Sebastien Jodogne
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
 ** Windows target
 ********************************************************************/

#if defined _WIN32

#include <windows.h>

#if defined(__GNUC__)
// This is Mingw
#define LAAW_EXPORT_DLL_API  // The exports are handled by the .DEF file
#else
// This is MSVC
#define LAAW_EXPORT_DLL_API __declspec(dllexport)
#endif

#ifdef _M_X64
// 64 bits target
#define LAAW_CALL_CONVENTION
#else
// 32 bits target
#define LAAW_CALL_CONVENTION  __stdcall  // Use the StdCall in Windows32 (for VB6)
#endif


/********************************************************************
 ** Linux target
 ********************************************************************/

#elif defined(__linux)

// Try the gcc visibility support
// http://gcc.gnu.org/wiki/Visibility
#if ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
#define LAAW_EXPORT_DLL_API  __attribute__ ((visibility("default")))
#define LAAW_CALL_CONVENTION
#else
#error No support for visibility in your version of GCC
#endif


/********************************************************************
 ** Max OS X target
 ********************************************************************/

#else

#define LAAW_EXPORT_DLL_API  __attribute__ ((visibility("default")))
#define LAAW_CALL_CONVENTION

#endif
