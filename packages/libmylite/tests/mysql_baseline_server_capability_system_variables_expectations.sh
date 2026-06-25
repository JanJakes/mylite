#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_capability_system_variables_expectations: $1" >&2
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
        "SELECT @@have_compress, @@GLOBAL.have_compress,
                @@have_dynamic_loading, @@GLOBAL.have_dynamic_loading,
                @@have_geometry, @@GLOBAL.have_geometry,
                @@have_profiling, @@GLOBAL.have_profiling,
                @@have_query_cache, @@GLOBAL.have_query_cache,
                @@have_rtree_keys, @@GLOBAL.have_rtree_keys,
                @@have_statement_timeout, @@GLOBAL.have_statement_timeout,
                @@have_symlink, @@GLOBAL.have_symlink;"
)
expect_value \
    "default/global scalar values" \
    "YES${TAB}YES${TAB}YES${TAB}YES${TAB}YES${TAB}YES${TAB}YES${TAB}YES${TAB}NO${TAB}NO${TAB}YES${TAB}YES${TAB}YES${TAB}YES${TAB}DISABLED${TAB}DISABLED" \
    "$scalar_values"

expected_show="have_compress|YES
have_dynamic_loading|YES
have_geometry|YES
have_profiling|YES
have_query_cache|NO
have_rtree_keys|YES
have_statement_timeout|YES
have_symlink|DISABLED"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
           'have_compress','have_dynamic_loading','have_geometry','have_profiling',
           'have_query_cache','have_rtree_keys','have_statement_timeout','have_symlink');" \
        | normalize_tsv
)
expect_value "show default rows" "$expected_show" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
           'have_compress','have_dynamic_loading','have_geometry','have_profiling',
           'have_query_cache','have_rtree_keys','have_statement_timeout','have_symlink');" \
        | normalize_tsv
)
expect_value "show global rows" "$expected_show" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN (
           'have_compress','have_dynamic_loading','have_geometry','have_profiling',
           'have_query_cache','have_rtree_keys','have_statement_timeout','have_symlink');" \
        | normalize_tsv
)
expect_value "show session rows" "$expected_show" "$show_session"

for variable in \
    have_compress \
    have_dynamic_loading \
    have_geometry \
    have_profiling \
    have_query_cache \
    have_rtree_keys \
    have_statement_timeout \
    have_symlink
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
    expect_error \
        "set global read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
    expect_error \
        "set read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET $variable = DEFAULT;"
done

expect_error \
    "set global read-only have_compress from user variable" \
    1238 \
    HY000 \
    "Variable 'have_compress' is a read only variable" \
    "SET @capability_value = 'YES'; SET GLOBAL have_compress = @capability_value;"

printf '%s\n' "mysql_baseline_server_capability_system_variables_expectations: ok"
