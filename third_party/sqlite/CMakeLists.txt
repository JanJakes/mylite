find_package(Threads REQUIRED)

add_library(mylite_sqlite STATIC
  amalgamation/sqlite3.c
)
add_library(MyLite::sqlite ALIAS mylite_sqlite)

target_include_directories(mylite_sqlite
  SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/amalgamation"
)

target_compile_definitions(mylite_sqlite
  PUBLIC
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_THREADSAFE=1
)

target_link_libraries(mylite_sqlite
  PUBLIC
    Threads::Threads
)

set_target_properties(mylite_sqlite PROPERTIES
  C_STANDARD 17
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
)
