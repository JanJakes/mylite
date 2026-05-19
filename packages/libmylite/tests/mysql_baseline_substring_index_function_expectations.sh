#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_substring_index_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_substring_index_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" >/dev/null

scalar_expected=$(cat <<\EXPECTED
www.mysql	mysql.com	abc	abc			NULL	NULL	NULL	A	Aa	é/🙂	c		aa		aa	a	c	abc	abc
-1	0
EXPECTED
)
expect_output \
    "scalar substring index values" \
    "$scalar_expected" \
    "DO 0; SELECT SUBSTRING_INDEX('www.mysql.com', '.', 2), "\
"SUBSTRING_INDEX('www.mysql.com', '.', -2), SUBSTRING_INDEX('abc', '.', 1), "\
"SUBSTRING_INDEX('abc', '.', -1), SUBSTRING_INDEX('abc', '.', 0), "\
"SUBSTRING_INDEX('abc', '', 1), SUBSTRING_INDEX(NULL, '.', 1), "\
"SUBSTRING_INDEX('abc', NULL, 1), SUBSTRING_INDEX('abc', '.', NULL), "\
"SUBSTRING_INDEX('AaA', 'a', 1), SUBSTRING_INDEX('AaA', 'A', 2), "\
"SUBSTRING_INDEX('é/🙂/x', '/', 2), SUBSTRING_INDEX('a--b--c', '--', -1), "\
"SUBSTRING_INDEX('aaaa', 'aa', 1), SUBSTRING_INDEX('aaaa', 'aa', 2), "\
"SUBSTRING_INDEX('aaaa', 'aa', -1), SUBSTRING_INDEX('aaaa', 'aa', -2), "\
"SUBSTRING_INDEX('abc', 'b', TRUE), SUBSTRING_INDEX('abc', 'b', -1), "\
"SUBSTRING_INDEX('abc', 'x', 1), SUBSTRING_INDEX('abc', 'x', -1); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual values and whitespace" \
    "www	a" \
    "SELECT SUBSTRING_INDEX ('www.mysql.com', '.', 1), SUBSTRING_INDEX('abc', 'b', 1) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO SUBSTRING_INDEX('abc', 'b', 1), SUBSTRING_INDEX(NULL, '.', 1); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(40), delim VARCHAR(8), n INT, c CHAR(6), txt TEXT, i INT, "\
"d DECIMAL(8,2), y YEAR, dt DATETIME, b VARBINARY(16), f DOUBLE"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'www.mysql.com', '.', 2, 'ab.cd', 'a/b/c', 12345, 12.30, 2024, "\
"'2024-01-02 13:29:17', X'7777772e6d7973716c', 1.5), "\
"(2, 'AaA', 'a', 1, 'AA', '', -22, -4.50, 70, NULL, X'416141', -2.5), "\
"(3, NULL, '.', 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), "\
"(4, 'é/🙂/x', '/', 2, 'é/🙂', 'left/right', NULL, NULL, NULL, NULL, NULL, NULL), "\
"(5, 'a--b--c', '--', -1, 'xy', NULL, 7, 100.00, 2024, "\
"'2025-12-31 01:02:03', NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	www.mysql	mysql.com	ab	1	12		2024-01	c
2	A	AaA	AA	-	-4	1970	NULL	
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
4	é/🙂	é/🙂/x	é/🙂	NULL	NULL	NULL	NULL	right
5	c	a--b--c	xy	7	100		2025-12	NULL
EXPECTED
)
expect_output \
    "table substring index values" \
    "$table_expected" \
    "SELECT id, SUBSTRING_INDEX(v, delim, n), SUBSTRING_INDEX(v, '.', -2), "\
"SUBSTRING_INDEX(c, '.', 1), SUBSTRING_INDEX(i, '2', 1), "\
"SUBSTRING_INDEX(d, '.', 1), SUBSTRING_INDEX(y, '2', 1), "\
"SUBSTRING_INDEX(dt, '-', 2), SUBSTRING_INDEX(txt, '/', -1) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "5	c
4	é/🙂
3	NULL" \
    "SELECT id, SUBSTRING_INDEX(v, delim, n) AS s FROM t WHERE id >= 2 ORDER BY id DESC LIMIT 3;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
SUBSTRING_INDEX(v, '.', 1)	s
www	mysql.com
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT SUBSTRING_INDEX(v, '.', 1), SUBSTRING_INDEX(v, '.', -2) AS s FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred binary and count conversions" \
    "777777	abc	abc	abc
1
Warning	1292	Truncated incorrect DECIMAL value: '18446744073709551616'" \
    "SELECT HEX(SUBSTRING_INDEX(b, _binary '.', 1)), SUBSTRING_INDEX('abc', 'b', 1.5), "\
"SUBSTRING_INDEX('abc', 'b', '2'), SUBSTRING_INDEX('abc', 'b', 18446744073709551616) "\
"FROM t WHERE id = 1; SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred predicate" \
    "1" \
    "SELECT COUNT(*) FROM t WHERE SUBSTRING_INDEX(v, '.', 1) = 'www';" \
    "$DATABASE"

expect_error \
    "substring index rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'" \
    "SELECT SUBSTRING_INDEX();" \
    "$DATABASE"

expect_error \
    "substring index rejects one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'" \
    "SELECT SUBSTRING_INDEX('a');" \
    "$DATABASE"

expect_error \
    "substring index rejects two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'" \
    "SELECT SUBSTRING_INDEX('a', 'b');" \
    "$DATABASE"

expect_error \
    "substring index rejects four arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SUBSTRING_INDEX'" \
    "SELECT SUBSTRING_INDEX('a', 'b', 1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_substring_index_function_expectations: ok"
