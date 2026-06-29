#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_stored_procedure_local_variables_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_stored_procedure_local_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_delimited() {
    sql=$1
    shift
    printf '%s\n//\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --delimiter='//' "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_error \
    "local variable procedure without selected schema" \
    1046 \
    "3D000" \
    "No database selected" \
    "CREATE PROCEDURE p_no_db()
BEGIN
  DECLARE a INT;
  SELECT a;
END//" \
    --delimiter='//'

run_mysql_delimited \
    "CREATE PROCEDURE ${DATABASE}.p_defaults()
BEGIN
  DECLARE a INT;
  DECLARE b INT DEFAULT 7;
  DECLARE c VARCHAR(10) DEFAULT 'hi';
  SELECT a, b, c;
END" >/dev/null
defaults_output=$(run_mysql "CALL ${DATABASE}.p_defaults();")
expect_value "default local values" "NULL	7	hi" "$defaults_output"

run_mysql_delimited \
    "CREATE PROCEDURE ${DATABASE}.p_set()
BEGIN
  DECLARE a INT DEFAULT 1;
  DECLARE b VARCHAR(10) DEFAULT '5';
  SET a = a + 4;
  SET b = CONCAT('x', b);
  SELECT a, b;
END" >/dev/null
set_output=$(run_mysql "CALL ${DATABASE}.p_set();")
expect_value "set local values" "5	x5" "$set_output"

run_mysql_delimited \
    "CREATE PROCEDURE ${DATABASE}.p_multi()
BEGIN
  DECLARE a, b INT DEFAULT 3;
  SET b = a + 2;
  SELECT a, b;
END" >/dev/null
multi_output=$(run_mysql "CALL ${DATABASE}.p_multi();")
expect_value "multi-name declaration" "3	5" "$multi_output"

run_mysql_delimited \
    "CREATE PROCEDURE ${DATABASE}.p_case()
BEGIN
  DECLARE Value INT DEFAULT 9;
  SET value = value + 1;
  SELECT Value;
END" >/dev/null
case_output=$(run_mysql "CALL ${DATABASE}.p_case();")
expect_value "case-insensitive local name" "10" "$case_output"

expect_error \
    "duplicate local variable" \
    1331 \
    "42000" \
    "Duplicate variable: A" \
    "CREATE PROCEDURE ${DATABASE}.p_duplicate()
BEGIN
  DECLARE a INT;
  DECLARE A INT;
  SELECT a;
END//" \
    --delimiter='//'

expect_error \
    "late declare" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "CREATE PROCEDURE ${DATABASE}.p_late_declare()
BEGIN
  SET @x = 1;
  DECLARE a INT;
  SELECT a;
END//" \
    --delimiter='//'

expect_error \
    "unknown local assignment target" \
    1193 \
    "HY000" \
    "Unknown system variable 'missing'" \
    "CREATE PROCEDURE ${DATABASE}.p_unknown_set()
BEGIN
  SET missing = 1;
  SELECT missing;
END//" \
    --delimiter='//'

printf '%s\n' "mysql_baseline_stored_procedure_local_variables_expectations: ok"
