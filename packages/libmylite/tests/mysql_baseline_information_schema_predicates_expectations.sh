#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_information_schema_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE t ("\
"id INT AUTO_INCREMENT PRIMARY KEY, "\
"v VARCHAR(20), "\
"n INT, "\
"KEY idx_v (v)"\
"); "\
"CREATE TABLE wp_options (id INT); "\
"CREATE TABLE wpa (id INT);" \
    "$DATABASE" >/dev/null

expect_output \
    "like case insensitive column name" \
    "id" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND COLUMN_NAME LIKE 'ID%';"

expect_output \
    "like escaped wildcard" \
    "wp_options" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME LIKE 'wp\\\\_%' "\
"ORDER BY TABLE_NAME;"

expect_output \
    "like wildcard" \
    "wp_options
wpa" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME LIKE 'wp_%' "\
"ORDER BY TABLE_NAME;"

expect_output \
    "no backslash escapes like pattern" \
    "" \
    "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME LIKE 'wp\\\\_%' "\
"ORDER BY TABLE_NAME;"

expect_output \
    "column name in list" \
    "id
v" \
    "SET SESSION sql_mode = ''; "\
"SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND COLUMN_NAME IN ('ID','v','missing') ORDER BY ORDINAL_POSITION;"

expect_output \
    "not like filters false and true values" \
    "t" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME NOT LIKE 'wp%' "\
"ORDER BY TABLE_NAME;"

expect_output \
    "alias-qualified predicate columns" \
    "id" \
    "SELECT c.COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS AS c "\
"WHERE c.TABLE_SCHEMA = '${DATABASE}' AND c.TABLE_NAME = 't' "\
"AND c.COLUMN_NAME LIKE 'ID%';"

expect_output \
    "column name not in list" \
    "n" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND COLUMN_NAME NOT IN ('id','v') ORDER BY ORDINAL_POSITION;"

expect_output \
    "not in null remains unknown" \
    "" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND COLUMN_NAME NOT IN ('missing', NULL) ORDER BY ORDINAL_POSITION;"

expect_output \
    "text between" \
    "t
wp_options" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME BETWEEN 't' AND 'wp_options' "\
"ORDER BY TABLE_NAME;"

expect_output \
    "numeric between" \
    "id
v" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND ORDINAL_POSITION BETWEEN 1 AND 2 ORDER BY ORDINAL_POSITION;"

expect_output \
    "numeric in with string coercion" \
    "id
n" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND ORDINAL_POSITION IN ('01', 3) ORDER BY ORDINAL_POSITION;"

expect_output \
    "not between" \
    "n" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"AND ORDINAL_POSITION NOT BETWEEN 1 AND 2 ORDER BY ORDINAL_POSITION;"

expect_output \
    "null comparison remains unknown under not" \
    "0" \
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND NOT ENGINE = 'InnoDB';"

expect_output \
    "equal null remains unknown" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND ENGINE = NULL;"

expect_output \
    "not equal null remains unknown" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND NOT ENGINE = NULL;"

expect_output \
    "null safe equal matches null metadata" \
    "3" \
    "SELECT COUNT(*) FROM ("\
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND ENGINE <=> NULL "\
"ORDER BY TABLE_NAME LIMIT 3"\
") AS limited;"

expect_output \
    "and unknown filters row" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND ENGINE = NULL AND TABLE_NAME = 'TABLES';"

expect_output \
    "or with true passes null side" \
    "1" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND (ENGINE = NULL OR TABLE_NAME = 'TABLES');"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE nope IN ('x');"

printf '%s\n' "mysql_baseline_information_schema_predicates_expectations: ok"
