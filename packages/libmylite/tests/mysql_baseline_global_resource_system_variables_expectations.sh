#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_global_resource_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar_values=$(
    run_mysql \
        "SELECT @@ngram_token_size, @@GLOBAL.ngram_token_size,
                @@offline_mode, @@GLOBAL.offline_mode,
                @@persist_only_admin_x509_subject,
                @@GLOBAL.persist_only_admin_x509_subject,
                @@persist_sensitive_variables_in_plaintext,
                @@GLOBAL.persist_sensitive_variables_in_plaintext,
                @@persisted_globals_load, @@GLOBAL.persisted_globals_load,
                @@protocol_compression_algorithms,
                @@GLOBAL.protocol_compression_algorithms,
                @@schema_definition_cache, @@GLOBAL.schema_definition_cache,
                @@stored_program_cache, @@GLOBAL.stored_program_cache,
                @@stored_program_definition_cache,
                @@GLOBAL.stored_program_definition_cache,
                @@sync_binlog, @@GLOBAL.sync_binlog,
                @@table_definition_cache, @@GLOBAL.table_definition_cache,
                @@table_encryption_privilege_check,
                @@GLOBAL.table_encryption_privilege_check,
                @@table_open_cache, @@GLOBAL.table_open_cache,
                @@table_open_cache_instances, @@GLOBAL.table_open_cache_instances,
                @@tablespace_definition_cache, @@GLOBAL.tablespace_definition_cache,
                @@temptable_max_mmap, @@GLOBAL.temptable_max_mmap,
                @@temptable_use_mmap, @@GLOBAL.temptable_use_mmap,
                @@thread_cache_size, @@GLOBAL.thread_cache_size,
                @@thread_stack, @@GLOBAL.thread_stack;"
)
expect_value \
    "default/global scalar values" \
    "2${TAB}2${TAB}0${TAB}0${TAB}${TAB}${TAB}1${TAB}1${TAB}1${TAB}1${TAB}zlib,zstd,uncompressed${TAB}zlib,zstd,uncompressed${TAB}256${TAB}256${TAB}256${TAB}256${TAB}256${TAB}256${TAB}1${TAB}1${TAB}2000${TAB}2000${TAB}0${TAB}0${TAB}4000${TAB}4000${TAB}16${TAB}16${TAB}256${TAB}256${TAB}0${TAB}0${TAB}0${TAB}0${TAB}9${TAB}9${TAB}1048576${TAB}1048576" \
    "$scalar_values"

expected_show="ngram_token_size|2
offline_mode|OFF
persist_only_admin_x509_subject|
persist_sensitive_variables_in_plaintext|ON
persisted_globals_load|ON
protocol_compression_algorithms|zlib,zstd,uncompressed
schema_definition_cache|256
stored_program_cache|256
stored_program_definition_cache|256
sync_binlog|1
table_definition_cache|2000
table_encryption_privilege_check|OFF
table_open_cache|4000
table_open_cache_instances|16
tablespace_definition_cache|256
temptable_max_mmap|0
temptable_use_mmap|OFF
thread_cache_size|9
thread_stack|1048576"

for scope in "" "GLOBAL" "SESSION"; do
    show_rows=$(
        run_mysql \
            "SHOW $scope VARIABLES WHERE Variable_name IN (
             'ngram_token_size','offline_mode','persist_only_admin_x509_subject',
             'persist_sensitive_variables_in_plaintext','persisted_globals_load',
             'protocol_compression_algorithms','schema_definition_cache',
             'stored_program_cache','stored_program_definition_cache','sync_binlog',
             'table_definition_cache','table_encryption_privilege_check',
             'table_open_cache','table_open_cache_instances',
             'tablespace_definition_cache','temptable_max_mmap',
             'temptable_use_mmap','thread_cache_size','thread_stack');" \
            | normalize_tsv
    )
    expect_value "show ${scope:-default} rows" "$expected_show" "$show_rows"
done

for variable in \
    ngram_token_size \
    offline_mode \
    persist_only_admin_x509_subject \
    persist_sensitive_variables_in_plaintext \
    persisted_globals_load \
    protocol_compression_algorithms \
    schema_definition_cache \
    stored_program_cache \
    stored_program_definition_cache \
    sync_binlog \
    table_definition_cache \
    table_encryption_privilege_check \
    table_open_cache \
    table_open_cache_instances \
    tablespace_definition_cache \
    temptable_max_mmap \
    temptable_use_mmap \
    thread_cache_size \
    thread_stack
do
    expect_error \
        "scalar session $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "scalar local $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@LOCAL.$variable;"
done

for variable in \
    ngram_token_size \
    persist_only_admin_x509_subject \
    persist_sensitive_variables_in_plaintext \
    persisted_globals_load \
    table_open_cache_instances \
    thread_stack
do
    expect_error \
        "set global read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
done

for variable in \
    offline_mode \
    protocol_compression_algorithms \
    schema_definition_cache \
    stored_program_cache \
    stored_program_definition_cache \
    sync_binlog \
    table_definition_cache \
    table_encryption_privilege_check \
    table_open_cache \
    tablespace_definition_cache \
    temptable_max_mmap \
    thread_cache_size
do
    expect_error \
        "set session global-only $variable" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

noop_values=$(
    run_mysql \
        "SET GLOBAL offline_mode = DEFAULT;
         SET GLOBAL protocol_compression_algorithms = DEFAULT;
         SET GLOBAL schema_definition_cache = DEFAULT;
         SET GLOBAL stored_program_cache = DEFAULT;
         SET GLOBAL stored_program_definition_cache = DEFAULT;
         SET GLOBAL sync_binlog = DEFAULT;
         SET GLOBAL table_definition_cache = DEFAULT;
         SET GLOBAL table_encryption_privilege_check = DEFAULT;
         SET GLOBAL table_open_cache = DEFAULT;
         SET GLOBAL tablespace_definition_cache = DEFAULT;
         SET GLOBAL temptable_max_mmap = DEFAULT;
         SET GLOBAL thread_cache_size = DEFAULT;
         SELECT @@GLOBAL.offline_mode, @@GLOBAL.protocol_compression_algorithms,
                @@GLOBAL.schema_definition_cache, @@GLOBAL.stored_program_cache,
                @@GLOBAL.stored_program_definition_cache, @@GLOBAL.sync_binlog,
                @@GLOBAL.table_definition_cache, @@GLOBAL.table_encryption_privilege_check,
                @@GLOBAL.table_open_cache, @@GLOBAL.tablespace_definition_cache,
                @@GLOBAL.temptable_max_mmap, @@GLOBAL.thread_cache_size,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "global no-op values" \
    "0${TAB}zlib,zstd,uncompressed${TAB}256${TAB}256${TAB}256${TAB}1${TAB}2000${TAB}0${TAB}4000${TAB}256${TAB}0${TAB}9${TAB}0${TAB}0${TAB}0" \
    "$noop_values"

warning=$(
    run_mysql \
        "SET GLOBAL temptable_use_mmap = DEFAULT;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
case "$warning" in
    *"Warning|1287|'temptable_use_mmap' is deprecated"*) ;;
    *) fail "temptable_use_mmap warning: got [$warning]" ;;
esac

printf '%s\n' "mysql_baseline_global_resource_system_variables_expectations: ok"
