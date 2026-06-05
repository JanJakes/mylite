function(mylite_add_developer_tool_targets)
  file(GLOB_RECURSE mylite_first_party_c_files CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/packages/*.c"
    "${PROJECT_SOURCE_DIR}/packages/*.h"
    "${PROJECT_SOURCE_DIR}/tests/*.c"
    "${PROJECT_SOURCE_DIR}/tests/*.h"
    "${PROJECT_SOURCE_DIR}/tools/*.c"
    "${PROJECT_SOURCE_DIR}/tools/*.h"
  )
  list(SORT mylite_first_party_c_files)

  find_program(MYLITE_CLANG_FORMAT_EXECUTABLE NAMES clang-format-19 clang-format)
  if(MYLITE_CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
      COMMAND "${MYLITE_CLANG_FORMAT_EXECUTABLE}" -i ${mylite_first_party_c_files}
      COMMENT "Formatting first-party C sources"
      VERBATIM
    )

    add_custom_target(format-check
      COMMAND "${MYLITE_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${mylite_first_party_c_files}
      COMMENT "Checking first-party C formatting"
      VERBATIM
    )
  else()
    add_custom_target(format
      COMMAND "${CMAKE_COMMAND}" -E echo "clang-format not found. Install LLVM and put it on PATH."
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )

    add_custom_target(format-check
      COMMAND "${CMAKE_COMMAND}" -E echo "clang-format not found. Install LLVM and put it on PATH."
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )
  endif()

  find_program(MYLITE_RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy-19 run-clang-tidy)
  if(MYLITE_RUN_CLANG_TIDY_EXECUTABLE)
    set(mylite_run_clang_tidy_args
      -p "${CMAKE_BINARY_DIR}"
      packages
      tools
      tests
    )

    if(APPLE)
      set(mylite_macos_sdk_path "${CMAKE_OSX_SYSROOT}")
      if(NOT mylite_macos_sdk_path)
        find_program(MYLITE_XCRUN_EXECUTABLE NAMES xcrun)
        if(MYLITE_XCRUN_EXECUTABLE)
          execute_process(
            COMMAND "${MYLITE_XCRUN_EXECUTABLE}" --show-sdk-path
            OUTPUT_VARIABLE mylite_macos_sdk_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
          )
        endif()
      endif()

      if(mylite_macos_sdk_path)
        list(APPEND mylite_run_clang_tidy_args
          "-extra-arg=-isysroot"
          "-extra-arg=${mylite_macos_sdk_path}"
        )
      endif()
    endif()

    add_custom_target(tidy
      COMMAND "${MYLITE_RUN_CLANG_TIDY_EXECUTABLE}" ${mylite_run_clang_tidy_args}
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Running clang-tidy on first-party C sources"
      VERBATIM
    )
  else()
    add_custom_target(tidy
      COMMAND "${CMAKE_COMMAND}" -E echo "run-clang-tidy not found. Install LLVM and put it on PATH."
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )
  endif()
endfunction()
