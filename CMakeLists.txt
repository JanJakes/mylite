cmake_minimum_required(VERSION 3.25)

project(MyLite
  VERSION 0.1.0
  DESCRIPTION "Embedded MySQL compatibility layer built on SQLite"
  LANGUAGES C
)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(MyLiteCompilerOptions)
include(MyLiteDeveloperTools)
include(MyLitePHP)

option(MYLITE_BUILD_TOOLS "Build MyLite command-line tools" ON)
option(MYLITE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MYLITE_BUILD_PHP_EXTENSIONS "Build MyLite PHP extensions" OFF)
option(BUILD_SHARED_LIBS "Build libmylite as a versioned shared library" OFF)
option(MYLITE_ENABLE_PROFILING "Enable opt-in runtime profiling instrumentation" OFF)
option(MYLITE_ENABLE_LTO "Enable supported interprocedural optimization" OFF)
option(MYLITE_ENABLE_SECTION_GC "Enable function/data sections and linker dead stripping" OFF)
option(MYLITE_ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MYLITE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MYLITE_BUILD_FUZZERS "Build Clang libFuzzer qualification targets" OFF)
option(
  MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS
  "Enable thread-local allocator failpoints for qualification tests"
  OFF
)

if(MYLITE_ENABLE_ASAN_UBSAN AND MYLITE_ENABLE_TSAN)
  message(FATAL_ERROR "MYLITE_ENABLE_ASAN_UBSAN and MYLITE_ENABLE_TSAN are mutually exclusive")
endif()

if(MYLITE_BUILD_FUZZERS AND NOT MYLITE_ENABLE_ASAN_UBSAN)
  message(FATAL_ERROR "Fuzz targets require MYLITE_ENABLE_ASAN_UBSAN")
endif()

if(MYLITE_BUILD_FUZZERS AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "MyLite fuzz targets require Clang libFuzzer")
endif()

if(MYLITE_BUILD_FUZZERS AND NOT BUILD_TESTING)
  message(FATAL_ERROR "MyLite fuzz targets require BUILD_TESTING")
endif()

if(MYLITE_ENABLE_PROFILING AND MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS)
  message(FATAL_ERROR "Profiling and allocator failpoint shims are mutually exclusive")
endif()

if(MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS AND NOT BUILD_TESTING)
  message(FATAL_ERROR "Allocator failpoints require BUILD_TESTING")
endif()

if(MYLITE_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT mylite_ipo_supported OUTPUT mylite_ipo_error LANGUAGES C)
  if(NOT mylite_ipo_supported)
    message(FATAL_ERROR "MYLITE_ENABLE_LTO is unsupported: ${mylite_ipo_error}")
  endif()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if((MYLITE_ENABLE_ASAN_UBSAN OR MYLITE_ENABLE_TSAN)
   AND NOT CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
  message(FATAL_ERROR "MyLite sanitizer modes require Clang, AppleClang, or GCC")
endif()

if(MYLITE_BUILD_PHP_EXTENSIONS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory(third_party/sqlite)
add_subdirectory(tools/lemon)
add_subdirectory(packages/libmylite)

if(MYLITE_ENABLE_SECTION_GC)
  mylite_configure_section_gc(mylite_sqlite)
endif()

if(MYLITE_BUILD_PHP_EXTENSIONS)
  add_subdirectory(packages/php-ext-mylite)
  add_subdirectory(packages/php-ext-mysqli-mylite)
  add_subdirectory(packages/php-ext-pdo-mylite)
endif()

if(MYLITE_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

mylite_add_developer_tool_targets()
