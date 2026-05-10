#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_set_fixed_system_variables_expectations_$$"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_set_fixed_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "autocommit unqualified" \
    "1	0	0	0" \
    "SET autocommit = 1; SELECT @@autocommit, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "autocommit session/local/system scopes" \
    "1	0	0	0" \
    "SET SESSION autocommit = 1;
     SET LOCAL autocommit = 1;
     SET @@autocommit = 1;
     SET @@session.autocommit = 1;
     SET @@local.autocommit = 1;
     SELECT @@autocommit, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "autocommit fixed boolean values" \
    "1	0	0	0" \
    "SET autocommit = DEFAULT;
     SET autocommit = ON;
     SET autocommit = TRUE;
     SELECT @@autocommit, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "fixed true and false boolean variables" \
    "1	1	1	0	0	0	0	0" \
    "SET sql_notes = 1;
     SET foreign_key_checks = TRUE;
     SET unique_checks = ON;
     SET sql_warnings = 0;
     SET sql_safe_updates = FALSE;
     SET sql_buffer_result = OFF;
     SELECT @@sql_notes, @@foreign_key_checks, @@unique_checks,
            @@sql_warnings, @@sql_safe_updates, @@sql_buffer_result,
            @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "sql mode default forms" \
    "${DEFAULT_SQL_MODE}	0	0	0" \
    "SET sql_mode = DEFAULT;
     SET SESSION sql_mode = DEFAULT;
     SET @@sql_mode = DEFAULT;
     SET @@session.sql_mode = DEFAULT;
     SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "sql mode exact default string" \
    "${DEFAULT_SQL_MODE}	0	0	0" \
    "SET sql_mode = '${DEFAULT_SQL_MODE}';
     SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "quoted variable target forms" \
    "1	${DEFAULT_SQL_MODE}	0" \
    "SET SESSION \`autocommit\` = ON;
     SET LOCAL \`sql_mode\` = DEFAULT;
     SELECT @@autocommit, @@sql_mode, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "mysql mutable autocommit deferred by mylite" \
    "0	0" \
    "SET autocommit = 0; SELECT @@autocommit, ROW_COUNT(); SET autocommit = 1;" \
    "$DATABASE"

expect_output \
    "mysql mutable sql mode deferred by mylite" \
    "ANSI_QUOTES	0" \
    "SET sql_mode = 'ANSI_QUOTES'; SELECT @@sql_mode, ROW_COUNT(); SET sql_mode = DEFAULT;" \
    "$DATABASE"

expect_output \
    "mysql assignment list deferred by mylite" \
    "1	1	0" \
    "SET autocommit = 1, sql_notes = 1; SELECT @@autocommit, @@sql_notes, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "mysql assignment operator deferred by mylite" \
    "1	0" \
    "SET autocommit := 1; SELECT @@autocommit, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "mysql global assignment deferred by mylite" \
    "1	0" \
    "SET GLOBAL autocommit = 1; SELECT @@global.autocommit, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "unknown variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SET no_such_mylite_variable = 1;" \
    "$DATABASE"

expect_error \
    "read only variable" \
    1238 \
    HY000 \
    "Variable 'version' is a read only variable" \
    "SET version = '8.4.9';" \
    "$DATABASE"

expect_error \
    "invalid autocommit integer" \
    1231 \
    42000 \
    "Variable 'autocommit' can't be set to the value of '2'" \
    "SET autocommit = 2;" \
    "$DATABASE"

expect_error \
    "invalid autocommit null" \
    1231 \
    42000 \
    "Variable 'autocommit' can't be set to the value of 'NULL'" \
    "SET autocommit = NULL;" \
    "$DATABASE"

expect_error \
    "invalid sql mode" \
    1231 \
    42000 \
    "Variable 'sql_mode' can't be set to the value of 'BOGUS'" \
    "SET sql_mode = 'BOGUS';" \
    "$DATABASE"

expect_error \
    "schema qualified name is an unknown system variable" \
    1193 \
    HY000 \
    "Unknown system variable 'app.autocommit'" \
    "SET app.autocommit = 1;" \
    "$DATABASE"

expect_error \
    "quoted system variable scope is invalid in mysql" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET @@\`session\`.autocommit = 1;" \
    "$DATABASE"
