macro(ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS PrecompiledHeaders PrecompiledSource Sources Target)
  get_filename_component(PrecompiledBasename ${PrecompiledHeaders} NAME_WE)
  set(PrecompiledBinary "${PrecompiledBasename}_$(ConfigurationName).pch")

  set_source_files_properties(${PrecompiledSource}
    PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeaders}\" /Fp\"${PrecompiledBinary}\""
    OBJECT_OUTPUTS "${PrecompiledBinary}")

  set_source_files_properties(${${Sources}}
    PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeaders}\" /FI\"${PrecompiledHeaders}\" /Fp\"${PrecompiledBinary}\""
    OBJECT_DEPENDS "${PrecompiledBinary}")

  set(${Target} ${PrecompiledSource})
endmacro()
