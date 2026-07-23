include_guard(GLOBAL)

include(CMakeParseArguments)

get_filename_component(mylite_lemon_source_root
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(MYLITE_HOST_C_COMPILER "" CACHE FILEPATH
  "Host C compiler used to build generator tools while cross-compiling")
set(MYLITE_HOST_C_FLAGS "" CACHE STRING
  "Additional host C compiler flags used to build generator tools")
set(MYLITE_LEMON_EXECUTABLE "" CACHE FILEPATH
  "Existing host Lemon executable to use instead of building one")

get_filename_component(mylite_generated_cache_default
  "${PROJECT_BINARY_DIR}" DIRECTORY)
set(MYLITE_GENERATED_CACHE_DIR
  "${mylite_generated_cache_default}/mylite-generated-cache"
  CACHE PATH
  "Shared content-addressed cache for generated MyLite sources")
unset(mylite_generated_cache_default)

function(mylite_add_lemon_host_tool)
  if(TARGET mylite_lemon)
    return()
  endif()

  set(lemon_source
    "${mylite_lemon_source_root}/third_party/sqlite/upstream/tool/lemon.c")

  if(MYLITE_LEMON_EXECUTABLE)
    if(NOT EXISTS "${MYLITE_LEMON_EXECUTABLE}")
      message(FATAL_ERROR
        "MYLITE_LEMON_EXECUTABLE does not exist: ${MYLITE_LEMON_EXECUTABLE}")
    endif()

    add_executable(mylite_lemon IMPORTED GLOBAL)
    set_target_properties(mylite_lemon PROPERTIES
      IMPORTED_LOCATION "${MYLITE_LEMON_EXECUTABLE}"
    )
    set(lemon_compiler_context
      "prebuilt=${MYLITE_LEMON_EXECUTABLE}")
  elseif(CMAKE_CROSSCOMPILING)
    set(host_c_compiler "${MYLITE_HOST_C_COMPILER}")
    if(NOT host_c_compiler AND DEFINED ENV{CC_FOR_BUILD})
      set(host_c_compiler "$ENV{CC_FOR_BUILD}")
    endif()
    if(NOT host_c_compiler)
      find_program(host_c_compiler NAMES cc clang gcc)
    endif()
    if(NOT host_c_compiler)
      message(FATAL_ERROR
        "Cross-compiling MyLite requires a host C compiler. Set "
        "MYLITE_HOST_C_COMPILER or CC_FOR_BUILD.")
    endif()

    include(ExternalProject)

    set(host_tool_dir "${PROJECT_BINARY_DIR}/host-tools/lemon")
    if(CMAKE_HOST_WIN32)
      set(host_executable_suffix ".exe")
    else()
      set(host_executable_suffix "")
    endif()
    set(host_lemon_executable
      "${host_tool_dir}/lemon${host_executable_suffix}")

    ExternalProject_Add(mylite_lemon_host_build
      SOURCE_DIR "${mylite_lemon_source_root}/tools/lemon"
      BINARY_DIR "${PROJECT_BINARY_DIR}/host-tools/lemon-build"
      CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE:STRING=Release"
        "-DCMAKE_C_COMPILER:FILEPATH=${host_c_compiler}"
        "-DCMAKE_C_FLAGS:STRING=${MYLITE_HOST_C_FLAGS}"
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${host_tool_dir}"
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE:PATH=${host_tool_dir}"
      BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --config Release
        --target mylite_lemon
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS "${host_lemon_executable}"
      USES_TERMINAL_CONFIGURE TRUE
      USES_TERMINAL_BUILD TRUE
    )

    add_executable(mylite_lemon IMPORTED GLOBAL)
    set_target_properties(mylite_lemon PROPERTIES
      IMPORTED_LOCATION "${host_lemon_executable}"
    )
    add_dependencies(mylite_lemon mylite_lemon_host_build)
    set(lemon_compiler_context
      "host_compiler=${host_c_compiler}\n"
      "host_flags=${MYLITE_HOST_C_FLAGS}")
  else()
    add_subdirectory(
      "${mylite_lemon_source_root}/tools/lemon"
      "${PROJECT_BINARY_DIR}/tools/lemon"
    )
    set(lemon_compiler_context
      "host_compiler=${CMAKE_C_COMPILER}\n"
      "compiler_id=${CMAKE_C_COMPILER_ID}\n"
      "compiler_version=${CMAKE_C_COMPILER_VERSION}\n"
      "c_flags=${CMAKE_C_FLAGS}\n"
      "c_flags_debug=${CMAKE_C_FLAGS_DEBUG}\n"
      "c_flags_release=${CMAKE_C_FLAGS_RELEASE}\n"
      "c_flags_relwithdebinfo=${CMAKE_C_FLAGS_RELWITHDEBINFO}\n"
      "c_flags_minsizerel=${CMAKE_C_FLAGS_MINSIZEREL}")
  endif()

  set_property(GLOBAL PROPERTY MYLITE_LEMON_SOURCE "${lemon_source}")
  set_property(GLOBAL PROPERTY
    MYLITE_LEMON_COMPILER_CONTEXT
    "host_system=${CMAKE_HOST_SYSTEM_NAME}\n"
    "host_processor=${CMAKE_HOST_SYSTEM_PROCESSOR}\n"
    "cross_compiling=${CMAKE_CROSSCOMPILING}\n"
    "${lemon_compiler_context}"
  )
endfunction()

function(mylite_add_lemon_parser)
  set(one_value_arguments BASENAME GRAMMAR OUTPUT_DIR TEMPLATE)
  cmake_parse_arguments(PARSER "" "${one_value_arguments}" "" ${ARGN})

  foreach(required_argument BASENAME GRAMMAR OUTPUT_DIR TEMPLATE)
    if(NOT PARSER_${required_argument})
      message(FATAL_ERROR
        "mylite_add_lemon_parser requires ${required_argument}")
    endif()
  endforeach()
  if(NOT TARGET mylite_lemon)
    message(FATAL_ERROR
      "mylite_add_lemon_host_tool must run before mylite_add_lemon_parser")
  endif()

  get_property(lemon_source GLOBAL PROPERTY MYLITE_LEMON_SOURCE)
  get_property(lemon_compiler_context
    GLOBAL PROPERTY MYLITE_LEMON_COMPILER_CONTEXT)
  set(context_file
    "${PARSER_OUTPUT_DIR}/${PARSER_BASENAME}-lemon-context.txt")
  file(GENERATE
    OUTPUT "${context_file}"
    CONTENT "${lemon_compiler_context}\n")

  set(output_c "${PARSER_OUTPUT_DIR}/${PARSER_BASENAME}.c")
  set(output_h "${PARSER_OUTPUT_DIR}/${PARSER_BASENAME}.h")
  add_custom_command(
    OUTPUT
      "${output_c}"
      "${output_h}"
    COMMAND "${CMAKE_COMMAND}"
      "-DBASENAME=${PARSER_BASENAME}"
      "-DCACHE_DIR=${MYLITE_GENERATED_CACHE_DIR}"
      "-DCOMPILER_CONTEXT=${context_file}"
      "-DGRAMMAR=${PARSER_GRAMMAR}"
      "-DLEMON_EXECUTABLE=$<TARGET_FILE:mylite_lemon>"
      "-DLEMON_SOURCE=${lemon_source}"
      "-DOUTPUT_DIR=${PARSER_OUTPUT_DIR}"
      "-DTEMPLATE=${PARSER_TEMPLATE}"
      -P "${mylite_lemon_source_root}/cmake/MyLiteGenerateLemonParser.cmake"
    DEPENDS
      mylite_lemon
      "${context_file}"
      "${lemon_source}"
      "${PARSER_GRAMMAR}"
      "${PARSER_TEMPLATE}"
      "${mylite_lemon_source_root}/cmake/MyLiteGenerateLemonParser.cmake"
    COMMENT "Generating ${PARSER_BASENAME} with host Lemon"
    VERBATIM
  )

  set(MYLITE_LEMON_PARSER_C "${output_c}" PARENT_SCOPE)
  set(MYLITE_LEMON_PARSER_H "${output_h}" PARENT_SCOPE)
endfunction()
