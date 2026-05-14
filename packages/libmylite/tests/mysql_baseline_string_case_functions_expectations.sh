#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_case_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_case_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

database_lower=$(printf '%s' "$DATABASE" | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz')
database_upper=$(printf '%s' "$DATABASE" | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')

scalar_expected=$(cat <<EXPECTED
abc	abc	ABC	ABC	a1-z!	NULL	NULL	123	-7	1	0	${database_lower}	${database_upper}
-1	0
EXPECTED
)
expect_output \
    "scalar string case values" \
    "$scalar_expected" \
    "DO 0; SELECT LOWER('ABC'), LCASE('ABC'), UPPER('abc'), UCASE('abc'), "\
"LOWER('A1-Z!'), LOWER(NULL), UPPER(NULL), LOWER(123), UPPER(-7), "\
"LOWER(TRUE), UPPER(FALSE), LOWER(DATABASE()), UPPER(DATABASE()); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "abc	ABC	abc	ABC" \
    "SELECT LOWER ('ABC'), UPPER ('abc'), LCASE ('ABC'), UCASE ('abc') FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO LOWER('ABC'), UPPER(NULL), LCASE(TRUE), UCASE(FALSE); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "system variable string case values" \
    "no_engine_substitution	NO_ENGINE_SUBSTITUTION" \
    "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; "\
"SELECT LOWER(@@sql_mode), UPPER(@@sql_mode);" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, dt DATETIME"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'AbC', 'A  ', 'HeLLo', 123, 12.30, 2024, '2024-01-02 13:29:17'), "\
"(2, 'xYz', 'b', '', -7, -4.50, 70, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	abc	ABC	a	A	hello	HELLO	123	12.30	2024	2024-01-02 13:29:17
2	xyz	XYZ	b	B			-7	-4.50	1970	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table string case values" \
    "$table_expected" \
    "SELECT id, LOWER(v), UPPER(v), LOWER(c), UPPER(c), LOWER(txt), UPPER(txt), "\
"LOWER(i), UPPER(d), LOWER(y), UPPER(dt) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	XYZ" \
    "SELECT id, UPPER(v) AS upper_v FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
LOWER(v)	u	LCASE(v)	UCASE(v)
abc	ABC	abc	ABC
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT LOWER(v), UPPER(v) AS u, LCASE(v), UCASE(v) FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred unicode and binary forms" \
    "é	É	straße	STRAßE	ABC	abc	414243	616263" \
    "SELECT LOWER('É'), UPPER('é'), LOWER('Straße'), UPPER('Straße'), "\
"LOWER(CAST('ABC' AS BINARY)), UPPER(CAST('abc' AS BINARY)), "\
"HEX(LOWER(CAST('ABC' AS BINARY))), HEX(UPPER(CAST('abc' AS BINARY)));" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred predicate" \
    "1" \
    "SELECT COUNT(*) FROM t WHERE LOWER(v) = 'abc';" \
    "$DATABASE"

expect_error \
    "lower rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LOWER'" \
    "SELECT LOWER();" \
    "$DATABASE"

expect_error \
    "lcase rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LCASE'" \
    "SELECT LCASE();" \
    "$DATABASE"

expect_error \
    "upper rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UPPER'" \
    "SELECT UPPER('a', 'b');" \
    "$DATABASE"

expect_error \
    "ucase rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UCASE'" \
    "SELECT UCASE('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_case_functions_expectations: ok"
