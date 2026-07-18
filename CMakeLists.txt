cmake_minimum_required(VERSION 3.25)

project(MyLite
  VERSION 0.1.0
  DESCRIPTION "Embedded MySQL compatibility layer built on SQLite"
  LANGUAGES C
)

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
option(MYLITE_ENABLE_PROFILING "Enable opt-in runtime profiling instrumentation" OFF)
option(MYLITE_ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MYLITE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(
  MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS
  "Enable thread-local allocator failpoints for qualification tests"
  OFF
)

if(MYLITE_ENABLE_ASAN_UBSAN AND MYLITE_ENABLE_TSAN)
  message(FATAL_ERROR "MYLITE_ENABLE_ASAN_UBSAN and MYLITE_ENABLE_TSAN are mutually exclusive")
endif()

if(MYLITE_ENABLE_PROFILING AND MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS)
  message(FATAL_ERROR "Profiling and allocator failpoint shims are mutually exclusive")
endif()

if(MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS AND NOT BUILD_TESTING)
  message(FATAL_ERROR "Allocator failpoints require BUILD_TESTING")
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
