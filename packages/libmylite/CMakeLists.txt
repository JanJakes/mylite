include("${CMAKE_CURRENT_LIST_DIR}/cmake/LibMyLiteLibrary.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/LibMyLiteBenchmark.cmake")

if(BUILD_TESTING)
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/LibMyLiteTests.cmake")
endif()

if(MYLITE_BUILD_FUZZERS)
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/LibMyLiteFuzzTargets.cmake")
endif()
