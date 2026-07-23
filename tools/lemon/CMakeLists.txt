if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.25)
  project(MyLiteLemonHost LANGUAGES C)
endif()

get_filename_component(mylite_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(mylite_lemon
  "${mylite_root}/third_party/sqlite/upstream/tool/lemon.c"
)

set_target_properties(mylite_lemon PROPERTIES
  OUTPUT_NAME lemon
)
