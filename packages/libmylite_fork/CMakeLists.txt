set(MYLITE_FORK_UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream")
set(MYLITE_FORK_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated/libmylite_fork/sqlite")

find_program(MYLITE_FORK_TCLSH_EXECUTABLE NAMES tclsh tclsh8.6)
if(NOT MYLITE_FORK_TCLSH_EXECUTABLE)
  message(FATAL_ERROR "tclsh is required to build the SQLite source-tree fork")
endif()

add_executable(mylite_fork_lemon
  upstream/tool/lemon.c
)
set_target_properties(mylite_fork_lemon PROPERTIES
  OUTPUT_NAME lemon
)

add_executable(mylite_fork_mkkeywordhash
  upstream/tool/mkkeywordhash.c
)

add_executable(mylite_fork_mksourceid
  upstream/tool/mksourceid.c
)
set_target_properties(mylite_fork_mksourceid PROPERTIES
  OUTPUT_NAME mksourceid
)

if(MSVC)
  target_compile_options(mylite_fork_lemon PRIVATE /w)
  target_compile_options(mylite_fork_mkkeywordhash PRIVATE /w)
  target_compile_options(mylite_fork_mksourceid PRIVATE /w)
else()
  target_compile_options(mylite_fork_lemon PRIVATE -w)
  target_compile_options(mylite_fork_mkkeywordhash PRIVATE -w)
  target_compile_options(mylite_fork_mksourceid PRIVATE -w)
endif()

set(MYLITE_FORK_SQLITE3_H "${MYLITE_FORK_GENERATED_DIR}/sqlite3.h")
set(MYLITE_FORK_PARSE_C "${MYLITE_FORK_GENERATED_DIR}/parse.c")
set(MYLITE_FORK_PARSE_H "${MYLITE_FORK_GENERATED_DIR}/parse.h")
set(MYLITE_FORK_KEYWORDHASH_H "${MYLITE_FORK_GENERATED_DIR}/keywordhash.h")
set(MYLITE_FORK_OPCODES_H "${MYLITE_FORK_GENERATED_DIR}/opcodes.h")
set(MYLITE_FORK_OPCODES_C "${MYLITE_FORK_GENERATED_DIR}/opcodes.c")
set(MYLITE_FORK_PRAGMA_H "${MYLITE_FORK_GENERATED_DIR}/pragma.h")
set(MYLITE_FORK_CTIME_C "${MYLITE_FORK_GENERATED_DIR}/ctime.c")

add_custom_command(
  OUTPUT "${MYLITE_FORK_SQLITE3_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_FORK_GENERATED_DIR}"
  COMMAND "${MYLITE_FORK_TCLSH_EXECUTABLE}"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mksqlite3h.tcl"
    "${MYLITE_FORK_UPSTREAM_DIR}"
    -o
    "${MYLITE_FORK_SQLITE3_H}"
  DEPENDS
    "${MYLITE_FORK_UPSTREAM_DIR}/VERSION"
    "${MYLITE_FORK_UPSTREAM_DIR}/manifest"
    "${MYLITE_FORK_UPSTREAM_DIR}/manifest.tags"
    "${MYLITE_FORK_UPSTREAM_DIR}/manifest.uuid"
    "${MYLITE_FORK_UPSTREAM_DIR}/src/sqlite.h.in"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mksqlite3h.tcl"
    mylite_fork_mksourceid
  COMMENT "Generating SQLite public header for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT
    "${MYLITE_FORK_PARSE_C}"
    "${MYLITE_FORK_PARSE_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_FORK_GENERATED_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${MYLITE_FORK_UPSTREAM_DIR}/src/parse.y"
    "${MYLITE_FORK_GENERATED_DIR}/parse.y"
  COMMAND "$<TARGET_FILE:mylite_fork_lemon>"
    -q
    -S
    "-T${MYLITE_FORK_UPSTREAM_DIR}/tool/lempar.c"
    parse.y
  DEPENDS
    mylite_fork_lemon
    "${MYLITE_FORK_UPSTREAM_DIR}/src/parse.y"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/lempar.c"
  WORKING_DIRECTORY "${MYLITE_FORK_GENERATED_DIR}"
  COMMENT "Generating SQLite parser for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT "${MYLITE_FORK_KEYWORDHASH_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_FORK_GENERATED_DIR}"
  COMMAND "$<TARGET_FILE:mylite_fork_mkkeywordhash>" > "${MYLITE_FORK_KEYWORDHASH_H}"
  DEPENDS mylite_fork_mkkeywordhash
  COMMENT "Generating SQLite keyword hash for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT "${MYLITE_FORK_OPCODES_H}"
  COMMAND "${CMAKE_COMMAND}"
    "-DTCLSH=${MYLITE_FORK_TCLSH_EXECUTABLE}"
    "-DSCRIPT=${MYLITE_FORK_UPSTREAM_DIR}/tool/mkopcodeh.tcl"
    "-DPARSE_HEADER=${MYLITE_FORK_PARSE_H}"
    "-DVDBE_SOURCE=${MYLITE_FORK_UPSTREAM_DIR}/src/vdbe.c"
    "-DOUTPUT=${MYLITE_FORK_OPCODES_H}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateOpcodeHeader.cmake"
  DEPENDS
    "${MYLITE_FORK_PARSE_H}"
    "${MYLITE_FORK_UPSTREAM_DIR}/src/vdbe.c"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkopcodeh.tcl"
  COMMENT "Generating SQLite opcode header for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT "${MYLITE_FORK_OPCODES_C}"
  COMMAND "${CMAKE_COMMAND}"
    "-DTCLSH=${MYLITE_FORK_TCLSH_EXECUTABLE}"
    "-DSCRIPT=${MYLITE_FORK_UPSTREAM_DIR}/tool/mkopcodec.tcl"
    "-DSCRIPT_ARGUMENTS=${MYLITE_FORK_OPCODES_H}"
    "-DOUTPUT=${MYLITE_FORK_OPCODES_C}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTclToFile.cmake"
  DEPENDS
    "${MYLITE_FORK_OPCODES_H}"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkopcodec.tcl"
  COMMENT "Generating SQLite opcode table for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT "${MYLITE_FORK_PRAGMA_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_FORK_GENERATED_DIR}"
  COMMAND "${MYLITE_FORK_TCLSH_EXECUTABLE}"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkpragmatab.tcl"
    "${MYLITE_FORK_PRAGMA_H}"
  DEPENDS "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkpragmatab.tcl"
  COMMENT "Generating SQLite pragma table for MyLite fork"
  VERBATIM
)

add_custom_command(
  OUTPUT "${MYLITE_FORK_CTIME_C}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_FORK_GENERATED_DIR}"
  COMMAND "${MYLITE_FORK_TCLSH_EXECUTABLE}"
    "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkctimec.tcl"
    "${MYLITE_FORK_CTIME_C}"
  DEPENDS "${MYLITE_FORK_UPSTREAM_DIR}/tool/mkctimec.tcl"
  COMMENT "Generating SQLite compile option table for MyLite fork"
  VERBATIM
)

set(MYLITE_FORK_SQLITE_SOURCES
  upstream/src/alter.c
  upstream/src/analyze.c
  upstream/src/attach.c
  upstream/src/auth.c
  upstream/src/backup.c
  upstream/src/bitvec.c
  upstream/src/btmutex.c
  upstream/src/btree.c
  upstream/src/build.c
  upstream/src/callback.c
  upstream/src/complete.c
  "${MYLITE_FORK_CTIME_C}"
  upstream/src/date.c
  upstream/src/delete.c
  upstream/src/expr.c
  upstream/src/fault.c
  upstream/src/fkey.c
  upstream/src/func.c
  upstream/src/global.c
  upstream/src/hash.c
  upstream/src/insert.c
  upstream/src/json.c
  upstream/src/legacy.c
  upstream/src/loadext.c
  upstream/src/main.c
  upstream/src/malloc.c
  upstream/src/mem1.c
  upstream/src/memdb.c
  upstream/src/memjournal.c
  upstream/src/mylite.c
  upstream/src/mutex.c
  upstream/src/mutex_noop.c
  "${MYLITE_FORK_OPCODES_C}"
  upstream/src/os.c
  upstream/src/pager.c
  "${MYLITE_FORK_PARSE_C}"
  upstream/src/pcache.c
  upstream/src/pcache1.c
  upstream/src/pragma.c
  upstream/src/prepare.c
  upstream/src/printf.c
  upstream/src/random.c
  upstream/src/resolve.c
  upstream/src/rowset.c
  upstream/src/select.c
  upstream/src/status.c
  upstream/src/table.c
  upstream/src/threads.c
  upstream/src/tokenize.c
  upstream/src/trigger.c
  upstream/src/update.c
  upstream/src/upsert.c
  upstream/src/utf.c
  upstream/src/util.c
  upstream/src/vacuum.c
  upstream/src/vdbe.c
  upstream/src/vdbeapi.c
  upstream/src/vdbeaux.c
  upstream/src/vdbeblob.c
  upstream/src/vdbemem.c
  upstream/src/vdbesort.c
  upstream/src/vdbetrace.c
  upstream/src/vtab.c
  upstream/src/wal.c
  upstream/src/walker.c
  upstream/src/where.c
  upstream/src/wherecode.c
  upstream/src/whereexpr.c
  upstream/src/window.c
  "${MYLITE_FORK_SQLITE3_H}"
  "${MYLITE_FORK_KEYWORDHASH_H}"
  "${MYLITE_FORK_OPCODES_H}"
  "${MYLITE_FORK_PARSE_H}"
  "${MYLITE_FORK_PRAGMA_H}"
)

if(WIN32)
  list(APPEND MYLITE_FORK_SQLITE_SOURCES
    upstream/src/mutex_w32.c
    upstream/src/os_win.c
  )
else()
  list(APPEND MYLITE_FORK_SQLITE_SOURCES
    upstream/src/mutex_unix.c
    upstream/src/os_unix.c
  )
endif()

add_library(mylite_fork_sqlite STATIC
  ${MYLITE_FORK_SQLITE_SOURCES}
)
add_library(MyLite::mylite_fork_sqlite ALIAS mylite_fork_sqlite)

target_compile_features(mylite_fork_sqlite PUBLIC c_std_17)
target_include_directories(mylite_fork_sqlite
  PUBLIC
    "${MYLITE_FORK_GENERATED_DIR}"
  PRIVATE
    "${MYLITE_FORK_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${MYLITE_FORK_UPSTREAM_DIR}/src"
)
target_compile_definitions(mylite_fork_sqlite
  PUBLIC
    SQLITE_OMIT_LOAD_EXTENSION
  PRIVATE
    SQLITE_CORE
    SQLITE_ENABLE_MYLITE
)

if(NOT WIN32)
  find_library(MYLITE_FORK_MATH_LIBRARY m)
  if(MYLITE_FORK_MATH_LIBRARY)
    target_link_libraries(mylite_fork_sqlite PUBLIC "${MYLITE_FORK_MATH_LIBRARY}")
  endif()
endif()

if(MSVC)
  target_compile_options(mylite_fork_sqlite PRIVATE /w)
else()
  target_compile_options(mylite_fork_sqlite PRIVATE -w)
endif()

set_target_properties(mylite_fork_sqlite PROPERTIES
  C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden
)

add_library(mylite_fork STATIC
  src/mylite_fork_charset.c
  src/mylite_sqlite_fork.c
)
add_library(MyLite::mylite_fork ALIAS mylite_fork)

target_include_directories(mylite_fork
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(mylite_fork PUBLIC MyLite::mylite_fork_sqlite)
mylite_configure_c_target(mylite_fork)

if(BUILD_TESTING)
  add_executable(mylite_fork_build_test
    tests/fork_build_test.c
  )
  target_link_libraries(mylite_fork_build_test PRIVATE MyLite::mylite_fork)
  mylite_configure_c_target(mylite_fork_build_test)

  add_test(NAME libmylite_fork.build COMMAND mylite_fork_build_test)
endif()
