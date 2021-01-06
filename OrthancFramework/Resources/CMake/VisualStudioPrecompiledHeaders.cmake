# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


macro(ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS PrecompiledHeaders PrecompiledSource Sources Target)
  get_filename_component(PrecompiledBasename ${PrecompiledHeaders} NAME_WE)
  set(PrecompiledBinary "${PrecompiledBasename}_${CMAKE_BUILD_TYPE}_${CMAKE_GENERATOR_PLATFORM}.pch")

  set_source_files_properties(${PrecompiledSource}
    PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeaders}\" /Fp\"${PrecompiledBinary}\""
    OBJECT_OUTPUTS "${PrecompiledBinary}")

  set_source_files_properties(${${Sources}}
    PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeaders}\" /FI\"${PrecompiledHeaders}\" /Fp\"${PrecompiledBinary}\""
    OBJECT_DEPENDS "${PrecompiledBinary}")

  set(${Target} ${PrecompiledSource})
endmacro()
