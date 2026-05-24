#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_big_tables_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "SET GLOBAL big_tables = DEFAULT;" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

readback_expected=$(cat <<EXPECTED
default	0	0	0	0
big_tables	OFF
big_tables	OFF
big_tables	OFF
session1	1	0	1	1	0	0
local_off	0	0	0
direct_on	1	0	0
default_again	0	0	0
local_true	1	0	0
false0	0	0	0
string_on	1	0	0
string_off	0	0	0
plus1	1	0	0
minus0	0	0	0
user1	1	0	0
user_on	1	0	0
user_off	0	0	0
global_noop	0	0	0
EXPECTED
)
expect_output \
    "readback and supported assignments" \
    "$readback_expected" \
    "SELECT 'default', @@big_tables, @@global.big_tables, @@session.big_tables, "\
"@@local.big_tables; "\
"SHOW VARIABLES LIKE 'big_tables'; "\
"SHOW SESSION VARIABLES LIKE 'big_tables'; "\
"SHOW GLOBAL VARIABLES LIKE 'big_tables'; "\
"SET SESSION big_tables = 1; "\
"SELECT 'session1', @@big_tables, @@GLOBAL.big_tables, @@SESSION.big_tables, "\
"@@LOCAL.big_tables, @@warning_count, ROW_COUNT(); "\
"SET LOCAL big_tables = OFF; "\
"SELECT 'local_off', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @@big_tables = ON; "\
"SELECT 'direct_on', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @@SESSION.big_tables = DEFAULT; "\
"SELECT 'default_again', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @@LOCAL.big_tables = TRUE; "\
"SELECT 'local_true', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET big_tables = FALSE; "\
"SELECT 'false0', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET big_tables = ('ON'); "\
"SELECT 'string_on', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET big_tables = 'OFF'; "\
"SELECT 'string_off', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET big_tables = (+1); "\
"SELECT 'plus1', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET big_tables = -0; "\
"SELECT 'minus0', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @bt = 1; "\
"SET big_tables = @bt; "\
"SELECT 'user1', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @bt = 'ON'; "\
"SET big_tables = @bt; "\
"SELECT 'user_on', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET @bt = 'OFF'; "\
"SET big_tables = @bt; "\
"SELECT 'user_off', @@big_tables, @@warning_count, ROW_COUNT(); "\
"SET GLOBAL big_tables = DEFAULT; "\
"SET @@GLOBAL.big_tables = OFF; "\
"SELECT 'global_noop', @@big_tables, @@GLOBAL.big_tables, @@warning_count;"

expect_error \
    "integer two rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '2'" \
    "SET SESSION big_tables = 2;"
expect_error \
    "negative rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '-1'" \
    "SET SESSION big_tables = -1;"
expect_error \
    "string one rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '1'" \
    "SET SESSION big_tables = '1';"
expect_error \
    "string true rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of 'TRUE'" \
    "SET SESSION big_tables = 'TRUE';"
expect_error \
    "null rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of 'NULL'" \
    "SET SESSION big_tables = NULL;"
expect_error \
    "decimal rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'big_tables'" \
    "SET SESSION big_tables = 1.5;"
expect_error \
    "parenthesized keyword rejected by grammar" \
    1064 \
    42000 \
    "You have an error" \
    "SET SESSION big_tables = (ON);"
expect_error \
    "integer user variable two rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '2'" \
    "SET @bt = 2; SET SESSION big_tables = @bt;"
expect_error \
    "string user variable one rejected" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '1'" \
    "SET @bt = '1'; SET SESSION big_tables = @bt;"
expect_error \
    "decimal user variable rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'big_tables'" \
    "SET @bt = 1.0; SET SESSION big_tables = @bt;"
expect_error \
    "decimal-looking string user variable rejected as value" \
    1231 \
    42000 \
    "Variable 'big_tables' can't be set to the value of '1.0'" \
    "SET @bt = '1.0'; SET SESSION big_tables = @bt;"

printf '%s\n' "mysql_baseline_big_tables_system_variable_expectations: ok"
