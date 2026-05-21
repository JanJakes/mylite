#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_quote_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_quote_function_expectations: $1" >&2
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

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

scalar_expected=$(printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n%s\t%s" \
    "'Don\\'t!'" "NULL" "''" "'abc'" "'123'" "'-7'" "'1'" "'0'" "'1.50'" "0" \
    "-1" "0")
scalar_sql=$(printf '%s\n' \
    "SET NAMES utf8mb4;" \
    "DO 0;" \
    "SELECT QUOTE('Don\\'t!'), QUOTE(NULL), QUOTE(''), QUOTE('abc')," \
    "       QUOTE(123), QUOTE(-7), QUOTE(TRUE), QUOTE(FALSE), QUOTE(1.50)," \
    "       @@warning_count;" \
    "SELECT ROW_COUNT(), @@warning_count;")
expect_output \
    "scalar quote values" \
    "$scalar_expected" \
    "$scalar_sql" \
    "$DATABASE"

null_expected=$(printf "%s\t%s\t%s\t%s\t%s\t%s" \
    "0" "1" "4" "utf8mb4" "utf8mb4_0900_ai_ci" "0")
expect_output \
    "quote null returns text NULL" \
    "$null_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT QUOTE(NULL) IS NULL, QUOTE(NULL) = 'NULL', LENGTH(QUOTE(NULL)), "\
"CHARSET(QUOTE(NULL)), COLLATION(QUOTE(NULL)), @@warning_count;"

escape_expected=$(printf "%s\t%s\t%s" \
    "27615C30625C5A5C5C5C272227" \
    "'a\\0b\\Z\\\\\\'\"'" \
    "0")
escape_sql=$(printf '%s\n' \
    "SET NAMES utf8mb4;" \
    "SELECT HEX(QUOTE(CONCAT('a', CHAR(0), 'b', CHAR(26), '\\\\', CHAR(39), '\"')))," \
    "       QUOTE(CONCAT('a', CHAR(0), 'b', CHAR(26), '\\\\', CHAR(39), '\"'))," \
    "       @@warning_count;")
expect_output \
    "escape bytes" \
    "$escape_expected" \
    "$escape_sql" \
    "$DATABASE"

control_expected=$(printf "%s\t%s" "276109620A630D64086527" "0")
control_sql=$(printf '%s\n' \
    "SET NAMES utf8mb4;" \
    "SELECT HEX(QUOTE(CONCAT('a', CHAR(9), 'b', CHAR(10), 'c', CHAR(13), 'd', CHAR(8), 'e')))," \
    "       @@warning_count;")
expect_output \
    "preserved control bytes" \
    "$control_expected" \
    "$control_sql" \
    "$DATABASE"

mode_expected=$(printf "%s\t%s\t%s\t%s" \
    "615C5C62" "'a\\\\\\\\b'" "27615C5C5C5C6227" "0")
mode_sql=$(printf '%s\n' \
    "SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';" \
    "SELECT HEX('a\\\\b'), QUOTE('a\\\\b'), HEX(QUOTE('a\\\\b')), @@warning_count;")
expect_output \
    "no backslash escapes keeps quote escaping" \
    "$mode_expected" \
    "$mode_sql" \
    "$DATABASE"

table_expected=$(printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" \
    "1" "'a\\'b'" "'x\\\\y'" "'12'" "'-12.30'" "'2024'" "'2024-01-02'" \
    "'03:04:05'" "'2024-01-02 03:04:05'" "'2024-01-02 03:04:06'" \
    "2" "NULL" "NULL" "NULL" "NULL" "NULL" "NULL" "NULL" "NULL" "NULL")
table_sql=$(printf '%s\n' \
    "SET NAMES utf8mb4;" \
    "CREATE TABLE t(" \
    "    id INT PRIMARY KEY," \
    "    v VARCHAR(10)," \
    "    tx TEXT," \
    "    n INT," \
    "    d DECIMAL(6,2)," \
    "    y YEAR," \
    "    da DATE," \
    "    ti TIME," \
    "    dt DATETIME," \
    "    ts TIMESTAMP NULL DEFAULT NULL" \
    ");" \
    "INSERT INTO t VALUES" \
    "    (1, CONCAT('a', CHAR(39), 'b'), CONCAT('x', '\\\\', 'y'), 12, -12.30, 2024, '2024-01-02', '03:04:05'," \
    "     '2024-01-02 03:04:05', '2024-01-02 03:04:06')," \
    "    (2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "SELECT id, QUOTE(v), QUOTE(tx), QUOTE(n), QUOTE(d), QUOTE(y), QUOTE(da)," \
    "       QUOTE(ti), QUOTE(dt), QUOTE(ts)" \
    "FROM t" \
    "ORDER BY id;")
expect_output \
    "table-backed quote values" \
    "$table_expected" \
    "$table_sql" \
    "$DATABASE"

labels_expected=$(printf "%s\t%s\n%s\t%s" "q" "QUOTE('b')" "'b'" "'b'")
expect_output_with_headers \
    "labels and whitespace" \
    "$labels_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT QUOTE ('b') AS q, QUOTE('b') FROM DUAL;"

expect_output \
    "do status" \
    "0	0" \
    "USE ${DATABASE}; SET NAMES utf8mb4; DO QUOTE('abc'), QUOTE(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "quote zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'QUOTE'" \
    "USE ${DATABASE}; SELECT QUOTE();"

expect_error \
    "quote two arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'QUOTE'" \
    "USE ${DATABASE}; SELECT QUOTE('a','b');"

expect_upstream_accepts \
    "binary string argument is deferred" \
    "USE ${DATABASE}; SELECT QUOTE(CAST('abc' AS BINARY));"

expect_upstream_accepts \
    "approximate numeric argument is deferred" \
    "USE ${DATABASE}; SELECT QUOTE(1.5E0);"

printf '%s\n' "mysql_baseline_quote_function_expectations: ok"
