#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_remaining_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift || true
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected [$haystack] to contain [$needle]" ;;
    esac
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
        "SELECT @@insert_id, @@rand_seed1, @@rand_seed2,
                @@GLOBAL.open_files_limit,
                @@GLOBAL.temptable_max_ram;"
)
open_files_limit=$(printf '%s' "$scalar_values" | cut -f4)
temptable_default=$(printf '%s' "$scalar_values" | cut -f5)
expect_value \
    "default scalar values" \
    "0${TAB}0${TAB}0${TAB}${open_files_limit}${TAB}${temptable_default}" \
    "$scalar_values"

show_rows=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
            'insert_id','open_files_limit','pseudo_thread_id','rand_seed1',
            'rand_seed2','statement_id','temptable_max_ram'
        );" \
        | normalize_tsv
)
expect_contains "show insert_id" "insert_id|0" "$show_rows"
expect_contains "show open_files_limit" "open_files_limit|$open_files_limit" "$show_rows"
expect_contains "show pseudo_thread_id" "pseudo_thread_id|" "$show_rows"
expect_contains "show rand_seed1" "rand_seed1|0" "$show_rows"
expect_contains "show rand_seed2" "rand_seed2|0" "$show_rows"
expect_contains "show statement_id" "statement_id|" "$show_rows"
expect_contains "show temptable_max_ram" "temptable_max_ram|$temptable_default" "$show_rows"

global_rows=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
            'insert_id','open_files_limit','pseudo_thread_id','rand_seed1',
            'rand_seed2','statement_id','temptable_max_ram'
        );" \
        | normalize_tsv
)
expect_value \
    "show global remaining rows" \
    "open_files_limit|$open_files_limit
temptable_max_ram|$temptable_default" \
    "$global_rows"

session_rows=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN (
            'insert_id','open_files_limit','pseudo_thread_id','rand_seed1',
            'rand_seed2','statement_id','temptable_max_ram'
        );" \
        | normalize_tsv
)
expect_contains "show session insert_id" "insert_id|0" "$session_rows"
expect_contains "show session open_files_limit" "open_files_limit|$open_files_limit" "$session_rows"
expect_contains "show session pseudo_thread_id" "pseudo_thread_id|" "$session_rows"
expect_contains "show session rand_seed1" "rand_seed1|0" "$session_rows"
expect_contains "show session rand_seed2" "rand_seed2|0" "$session_rows"
expect_contains "show session statement_id" "statement_id|" "$session_rows"
expect_contains "show session temptable" "temptable_max_ram|$temptable_default" "$session_rows"

insert_id_values=$(
    run_mysql \
        "DROP DATABASE IF EXISTS mylite_remaining_vars;
         CREATE DATABASE mylite_remaining_vars;
         USE mylite_remaining_vars;
         CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(10));
         SET insert_id = 100;
         SELECT @@insert_id, LAST_INSERT_ID();
         INSERT INTO t (name) VALUES ('a');
         SELECT id, @@insert_id, LAST_INSERT_ID() FROM t;
         SET insert_id = 200;
         INSERT INTO t (name) VALUES ('b'),('c');
         SELECT id, name FROM t ORDER BY id;
         SELECT @@insert_id, LAST_INSERT_ID();" \
        | normalize_tsv
)
expect_value \
    "insert_id allocation" \
    "100|0
100|0|100
100|a
200|b
201|c
0|200" \
    "$insert_id_values"

negative_insert_warning=$(
    run_mysql "SET insert_id = -1; SHOW WARNINGS LIMIT 1;" | normalize_tsv
)
expect_contains "negative insert_id warning" "Warning|1292|Truncated incorrect insert_id value: '-1'" "$negative_insert_warning"

pseudo_values=$(
    run_mysql \
        "SELECT @@pseudo_thread_id = CONNECTION_ID();
         SET pseudo_thread_id = 456;
         SELECT @@pseudo_thread_id, CONNECTION_ID();
         SET pseudo_thread_id = DEFAULT;
         SELECT @@pseudo_thread_id, CONNECTION_ID();" \
        | normalize_tsv
)
expect_value \
    "pseudo_thread_id values" \
    "1
456|456
0|0" \
    "$pseudo_values"

pseudo_warnings=$(
    run_mysql \
        "SET pseudo_thread_id = -1;
         SHOW WARNINGS LIMIT 1;
         SET pseudo_thread_id = 18446744073709551615;
         SHOW WARNINGS LIMIT 1;
         SELECT @@pseudo_thread_id;" \
        | normalize_tsv
)
expect_value \
    "pseudo_thread_id warnings" \
    "Warning|1292|Truncated incorrect pseudo_thread_id value: '-1'
Warning|1292|Truncated incorrect pseudo_thread_id value: '18446744073709551615'
4294967295" \
    "$pseudo_warnings"

rand_values=$(
    run_mysql \
        "SET rand_seed1 = 1;
         SET rand_seed2 = 2;
         SELECT @@rand_seed1, @@rand_seed2, FORMAT(RAND(), 18), FORMAT(RAND(), 18);" \
        | normalize_tsv
)
expect_value \
    "rand_seed values" \
    "0|0|0.000000004656612877|0.000000051222741652" \
    "$rand_values"

negative_rand_warning=$(
    run_mysql "SET rand_seed1 = -1; SHOW WARNINGS LIMIT 1;" | normalize_tsv
)
expect_contains "negative rand_seed1 warning" "Warning|1292|Truncated incorrect rand_seed1 value: '-1'" "$negative_rand_warning"

statement_values=$(
    run_mysql \
        "SELECT @@statement_id;
         SELECT @@statement_id;" \
        | normalize_tsv
)
first_statement_id=$(printf '%s\n' "$statement_values" | sed -n '1p')
second_statement_id=$(printf '%s\n' "$statement_values" | sed -n '2p')
if [ "$second_statement_id" -le "$first_statement_id" ]; then
    fail "statement_id did not increase: first [$first_statement_id], second [$second_statement_id]"
fi

expect_error \
    "set statement_id read only" \
    1238 \
    HY000 \
    "read only variable" \
    "SET statement_id = 1;"

expect_error \
    "session open_files_limit scalar" \
    1238 \
    HY000 \
    "is a GLOBAL variable" \
    "SELECT @@SESSION.open_files_limit;"
expect_error \
    "set open_files_limit read only" \
    1238 \
    HY000 \
    "read only variable" \
    "SET GLOBAL open_files_limit = DEFAULT;"
expect_error \
    "session temptable scalar" \
    1238 \
    HY000 \
    "is a GLOBAL variable" \
    "SELECT @@SESSION.temptable_max_ram;"
expect_error \
    "set session temptable" \
    1229 \
    HY000 \
    "should be set with SET GLOBAL" \
    "SET SESSION temptable_max_ram = DEFAULT;"

temptable_set=$(
    run_mysql \
        "SET GLOBAL temptable_max_ram = 1073741824;
         SELECT @@GLOBAL.temptable_max_ram;
         SET GLOBAL temptable_max_ram = DEFAULT;
         SELECT @@GLOBAL.temptable_max_ram;" \
        | normalize_tsv
)
expect_value \
    "temptable SET GLOBAL" \
    "1073741824
$temptable_default" \
    "$temptable_set"

expect_error \
    "insert_id default error" \
    1230 \
    42000 \
    "doesn't have a default value" \
    "SET insert_id = DEFAULT;"
expect_error \
    "insert_id null error" \
    1232 \
    42000 \
    "Incorrect argument type" \
    "SET insert_id = NULL;"
expect_error \
    "pseudo string error" \
    1232 \
    42000 \
    "Incorrect argument type" \
    "SET pseudo_thread_id = 'abc';"
expect_error \
    "rand global error" \
    1228 \
    HY000 \
    "can't be used with SET GLOBAL" \
    "SET GLOBAL rand_seed1 = 1;"
expect_error \
    "rand default error" \
    1230 \
    42000 \
    "doesn't have a default value" \
    "SET rand_seed1 = DEFAULT;"

printf '%s\n' "mysql_baseline_remaining_system_variables_expectations: ok"
