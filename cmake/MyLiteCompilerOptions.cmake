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
endfunction()
