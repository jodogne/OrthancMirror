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

/**
 * Besides the "pragma once" above that only protects this file,
 * define a macro to prevent including different versions of
 * "OrthancFramework.h"
 **/
#ifndef __ORTHANC_FRAMEWORK_H
#define __ORTHANC_FRAMEWORK_H

#if !defined(ORTHANC_BUILDING_FRAMEWORK_LIBRARY)
#  error The macro ORTHANC_BUILDING_FRAMEWORK_LIBRARY must be defined
#endif

/**
 * It is implied that if this file is used, we're building the Orthanc
 * framework (not using it as a shared library): We don't use the
 * common "BUILDING_DLL"
 * construction. https://gcc.gnu.org/wiki/Visibility
 **/
#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
#  if defined(_WIN32) || defined (__CYGWIN__)
#    define ORTHANC_PUBLIC __declspec(dllexport)
#    define ORTHANC_LOCAL
#  else
#    if __GNUC__ >= 4
#      define ORTHANC_PUBLIC __attribute__((visibility ("default")))
#      define ORTHANC_LOCAL  __attribute__((visibility ("hidden")))
#    else
#      define ORTHANC_PUBLIC
#      define ORTHANC_LOCAL
#      pragma warning Unknown dynamic link import/export semantics
#    endif
#  endif
#else
#  define ORTHANC_PUBLIC
#  define ORTHANC_LOCAL
#endif


#define ORTHANC_FRAMEWORK_VERSION_IS_ABOVE(major, minor, revision)      \
  (ORTHANC_VERSION_MAJOR > major ||                                     \
   (ORTHANC_VERSION_MAJOR == major &&                                   \
    (ORTHANC_VERSION_MINOR > minor ||                                   \
     (ORTHANC_VERSION_MINOR == minor &&                                 \
      ORTHANC_VERSION_REVISION >= revision))))


#include <string>

namespace Orthanc
{
  ORTHANC_PUBLIC void InitializeFramework(const std::string& locale,
                                          bool loadPrivateDictionary);
  
  ORTHANC_PUBLIC void FinalizeFramework();
}


#endif /* __ORTHANC_FRAMEWORK_H */
