#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_lower_case_table_names_system_variable_expectations: $1" >&2
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

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(run_mysql 'SELECT @@lower_case_table_names, @@GLOBAL.lower_case_table_names;')
expect_value "scalar default and global" "0${TAB}0" "$scalar"

case_scalar=$(run_mysql 'SELECT @@LOWER_CASE_TABLE_NAMES, @@global.`lower_case_table_names`;')
expect_value "case-insensitive and quoted scalar name" "0${TAB}0" "$case_scalar"

show_default=$(run_mysql "SHOW VARIABLES LIKE 'lower_case_table_names';" | normalize_tsv)
expect_value "show default" "lower_case_table_names|0" "$show_default"

show_session=$(run_mysql "SHOW SESSION VARIABLES LIKE 'lower_case_table_names';" | normalize_tsv)
expect_value "show session" "lower_case_table_names|0" "$show_session"

show_local=$(run_mysql "SHOW LOCAL VARIABLES LIKE 'lower_case_table_names';" | normalize_tsv)
expect_value "show local" "lower_case_table_names|0" "$show_local"

show_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'lower_case_table_names';" | normalize_tsv)
expect_value "show global" "lower_case_table_names|0" "$show_global"

where_rows=$(
    run_mysql \
        "SHOW VARIABLES WHERE Value = '0' AND Variable_name IN ('autocommit','lower_case_table_names');" \
        | normalize_tsv
)
expect_value "show variables where value" "lower_case_table_names|0" "$where_rows"

show_status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'lower_case_table_names'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$show_status"

expect_error \
    "session scalar" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a GLOBAL variable" \
    "SELECT @@SESSION.lower_case_table_names;"

expect_error \
    "local scalar" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a GLOBAL variable" \
    "SELECT @@LOCAL.lower_case_table_names;"

expect_error \
    "set default scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET lower_case_table_names = 0;"

expect_error \
    "set session scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET SESSION lower_case_table_names = 0;"

expect_error \
    "set local scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET LOCAL lower_case_table_names = 0;"

expect_error \
    "set global scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET GLOBAL lower_case_table_names = 0;"

expect_error \
    "set system variable default scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET @@lower_case_table_names = 0;"

expect_error \
    "set system variable global scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET @@GLOBAL.lower_case_table_names = 0;"

expect_error \
    "set system variable session scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET @@SESSION.lower_case_table_names = 0;"

expect_error \
    "set system variable local scope" \
    1238 \
    HY000 \
    "Variable 'lower_case_table_names' is a read only variable" \
    "SET @@LOCAL.lower_case_table_names = 0;"
