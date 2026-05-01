add_library(mylite_sqlite STATIC
  upstream/sqlite3.c
)
add_library(MyLite::sqlite ALIAS mylite_sqlite)

target_compile_features(mylite_sqlite PUBLIC c_std_17)
target_include_directories(mylite_sqlite
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream"
)
target_compile_definitions(mylite_sqlite
  PUBLIC
    SQLITE_OMIT_LOAD_EXTENSION
)

if(MSVC)
  target_compile_options(mylite_sqlite PRIVATE /w)
else()
  target_compile_options(mylite_sqlite PRIVATE -w)
endif()

set_target_properties(mylite_sqlite PROPERTIES
  C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden
)
