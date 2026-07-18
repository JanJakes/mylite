function(mylite_configure_c_target target)
  target_compile_features("${target}" PUBLIC c_std_17)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions("${target}" PRIVATE _DEFAULT_SOURCE)
  endif()

  set_target_properties("${target}" PROPERTIES
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
  )

  if(WIN32)
    target_compile_definitions("${target}" PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4)
    if(MYLITE_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE /WX)
    endif()
  else()
    target_compile_options("${target}" PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
    )
    if(MYLITE_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE -Werror)
    endif()
  endif()

  if(MYLITE_ENABLE_ASAN_UBSAN)
    target_compile_options("${target}" PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
      -fno-sanitize-recover=all
    )
    mylite_configure_sanitizer_link_options("${target}" -fsanitize=address,undefined)
  elseif(MYLITE_ENABLE_TSAN)
    target_compile_options("${target}" PRIVATE
      -fsanitize=thread
      -fno-omit-frame-pointer
      -fno-sanitize-recover=all
    )
    mylite_configure_sanitizer_link_options("${target}" -fsanitize=thread)
  endif()

  if(MYLITE_ENABLE_SECTION_GC)
    mylite_configure_section_gc("${target}")
  endif()
endfunction()

function(mylite_configure_section_gc target)
  get_target_property(target_type "${target}" TYPE)

  if(MSVC)
    target_compile_options("${target}" PRIVATE /Gy /Gw)
    if(target_type STREQUAL "STATIC_LIBRARY" OR target_type STREQUAL "OBJECT_LIBRARY")
      target_link_options("${target}" INTERFACE /OPT:REF /OPT:ICF)
    else()
      target_link_options("${target}" PRIVATE /OPT:REF /OPT:ICF)
    endif()
  else()
    target_compile_options("${target}" PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
      set(section_gc_link_option -Wl,-dead_strip)
    else()
      set(section_gc_link_option -Wl,--gc-sections)
    endif()
    if(target_type STREQUAL "STATIC_LIBRARY" OR target_type STREQUAL "OBJECT_LIBRARY")
      target_link_options("${target}" INTERFACE "${section_gc_link_option}")
    else()
      target_link_options("${target}" PRIVATE "${section_gc_link_option}")
    endif()
  endif()
endfunction()

function(mylite_configure_sanitizer_link_options target sanitizer_option)
  get_target_property(target_type "${target}" TYPE)
  if(target_type STREQUAL "STATIC_LIBRARY"
     OR target_type STREQUAL "OBJECT_LIBRARY"
     OR target_type STREQUAL "INTERFACE_LIBRARY")
    target_link_options("${target}" INTERFACE "${sanitizer_option}")
  else()
    target_link_options("${target}" PRIVATE "${sanitizer_option}")
  endif()
endfunction()
