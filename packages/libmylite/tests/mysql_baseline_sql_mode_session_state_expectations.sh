#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --binary-as-hex=1 --skip-column-names"
DATABASE="mylite_sql_mode_session_state_expectations_$$"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_sql_mode_session_state_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "listed sql_mode assignment forms" \
    "	${DEFAULT_SQL_MODE}	0	0
STRICT_TRANS_TABLES	${DEFAULT_SQL_MODE}	1	0
NO_ZERO_DATE	${DEFAULT_SQL_MODE}	1	0
NO_ZERO_IN_DATE	${DEFAULT_SQL_MODE}	1	0
NO_AUTO_VALUE_ON_ZERO	${DEFAULT_SQL_MODE}	0	0
NO_ENGINE_SUBSTITUTION	${DEFAULT_SQL_MODE}	0	0
NO_ZERO_DATE	${DEFAULT_SQL_MODE}	1	0
NO_ZERO_IN_DATE	${DEFAULT_SQL_MODE}	1	0
ONLY_FULL_GROUP_BY	${DEFAULT_SQL_MODE}	0	0
ANSI_QUOTES,STRICT_TRANS_TABLES	${DEFAULT_SQL_MODE}	1	0
ANSI_QUOTES	${DEFAULT_SQL_MODE}	0	0
${DEFAULT_SQL_MODE}	${DEFAULT_SQL_MODE}	0	0
NO_ENGINE_SUBSTITUTION	${DEFAULT_SQL_MODE}	0	0
${DEFAULT_SQL_MODE}	${DEFAULT_SQL_MODE}	0	0
NO_ENGINE_SUBSTITUTION	${DEFAULT_SQL_MODE}	0	0
${DEFAULT_SQL_MODE}	${DEFAULT_SQL_MODE}	0	0
NO_BACKSLASH_ESCAPES	${DEFAULT_SQL_MODE}	0	0" \
    "SET sql_mode = '';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = 'STRICT_TRANS_TABLES';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = 'NO_ZERO_DATE';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = 'NO_ZERO_IN_DATE';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET @@sql_mode = \"NO_ENGINE_SUBSTITUTION\";
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET SESSION sql_mode = \"NO_ZERO_DATE\";
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET @@SESSION.sql_mode = \"NO_ZERO_IN_DATE\";
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET @@session.SQL_mode = \"only_full_group_by\";
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET LOCAL sql_mode = ',ansi_quotes,,strict_trans_tables,';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET @@LOCAL.sql_mode = 'ANSI_QUOTES';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET @@LOCAL.sql_mode = DEFAULT;
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET SESSION sql_mode = DEFAULT;
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET sql_mode = DEFAULT;
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();
     SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
     SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "strict warning" \
    "Warning	3135	'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used with strict mode. They will be merged with strict mode in a future release." \
    "SET sql_mode = 'STRICT_TRANS_TABLES'; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "canonical combination modes" \
    "REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,NO_UNSIGNED_SUBTRACTION,NO_DIR_IN_CREATE,ANSI,NO_AUTO_VALUE_ON_ZERO,STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION	0" \
    "SET sql_mode = 'ANSI,TRADITIONAL,NO_AUTO_VALUE_ON_ZERO,NO_UNSIGNED_SUBTRACTION,NO_DIR_IN_CREATE';
     SELECT @@sql_mode, @@warning_count;" \
    "$DATABASE"

expect_output \
    "show variables session and global" \
    "sql_mode	NO_ENGINE_SUBSTITUTION
sql_mode	${DEFAULT_SQL_MODE}" \
    "SET @@sql_mode = 'NO_ENGINE_SUBSTITUTION';
     SHOW VARIABLES LIKE 'sql_mode';
     SHOW GLOBAL VARIABLES LIKE 'sql_mode';" \
    "$DATABASE"

expect_output \
    "NO_BACKSLASH_ESCAPES string effect" \
    "a\\" \
    "SET sql_mode = 'NO_BACKSLASH_ESCAPES';
     CREATE TABLE slash_strings (v VARCHAR(10));
     INSERT INTO slash_strings VALUES ('a\\');
     SELECT v FROM slash_strings;" \
    "$DATABASE"

expect_output \
    "ANSI_QUOTES identifier effect" \
    "7" \
    "SET sql_mode = 'ANSI';
     CREATE TABLE \"ansi_mode_table\" (id INT);
     INSERT INTO \"ansi_mode_table\" VALUES (7);
     SELECT id FROM \"ansi_mode_table\";" \
    "$DATABASE"

expect_output \
    "REAL_AS_FLOAT effect" \
    "double
float" \
    "SET sql_mode = '';
     CREATE TABLE real_double (r REAL);
     SET sql_mode = 'REAL_AS_FLOAT';
     CREATE TABLE real_float (r REAL);
     SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS
       WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'real_double' AND COLUMN_NAME = 'r';
     SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS
       WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'real_float' AND COLUMN_NAME = 'r';" \
    "$DATABASE"

expect_output \
    "NO_AUTO_VALUE_ON_ZERO effect" \
    "0	20
1	30" \
    "SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO';
     CREATE TABLE auto_zero (id INT AUTO_INCREMENT PRIMARY KEY, v INT);
     INSERT INTO auto_zero (id, v) VALUES (0, 20);
     INSERT INTO auto_zero (v) VALUES (30);
     SELECT id, v FROM auto_zero WHERE v IN (20, 30) ORDER BY v;" \
    "$DATABASE"

expect_output \
    "strict plus pad warning count" \
    "2" \
    "SET sql_mode = 'STRICT_TRANS_TABLES,PAD_CHAR_TO_FULL_LENGTH';
     SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_error \
    "invalid sql_mode" \
    1231 \
    42000 \
    "Variable 'sql_mode' can't be set to the value of 'BOGUS'" \
    "SET sql_mode = 'BOGUS';" \
    "$DATABASE"
