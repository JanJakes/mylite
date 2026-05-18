#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_replace_string_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_replace_string_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<\EXPECTED
WwWwWw.mysql.com	bb	AxAx	abc		NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "scalar replace values" \
    "$scalar_expected" \
    "DO 0; SELECT REPLACE('www.mysql.com', 'w', 'Ww'), REPLACE('aaaa', 'aa', 'b'), "\
"REPLACE('AaAa', 'a', 'x'), REPLACE('abc', '', 'x'), REPLACE('', '', 'x'), "\
"REPLACE(NULL, 'a', 'b'), REPLACE('abc', NULL, 'b'), REPLACE('abc', 'a', NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "converted replace arguments" \
    "1XX45	9	7" \
    "SELECT REPLACE(12345, 23, 'XX'), REPLACE(TRUE, 1, 9), REPLACE(FALSE, 0, 7);" \
    "$DATABASE"

expect_output \
    "session and system replace arguments" \
    "db	schema	zero" \
    "DO 0; SELECT REPLACE(DATABASE(), DATABASE(), 'db'), "\
"REPLACE(SCHEMA(), SCHEMA(), 'schema'), REPLACE(@@warning_count, 0, 'zero');" \
    "$DATABASE"

expect_output \
    "multibyte exact replacement" \
    "ee	éé	axa	0" \
    "SET NAMES utf8mb4; SELECT REPLACE('éé', 'é', 'e'), REPLACE('éé', 'É', 'e'), "\
"REPLACE('a😀a', '😀', 'x'), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "Abc	aBc" \
    "SELECT REPLACE ('abc', 'a', 'A'), REPLACE(('abc'), ('b'), ('B')) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO REPLACE('abc', 'b', 'B'), REPLACE(NULL, 'a', 'b'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "\
"dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, b VARBINARY(8), f DOUBLE); "\
"INSERT INTO t VALUES "\
"(1, 'abcabc', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02', '01:02:03', "\
"'2024-01-02 13:29:17', '2024-01-02 13:29:17', X'616263', 1.25), "\
"(2, 'AaAa', 'B', 'x', NULL, -4.50, 70, NULL, NULL, NULL, NULL, X'00', -2.5), "\
"(3, NULL, NULL, NULL, -77, NULL, NULL, '2024-12-31', '00:00:00', "\
"'2024-12-31 23:59:58', '2024-12-31 23:59:58', NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	XbcXbc	X	heLLo	1x345	1x.30	x0x4	2024/01/02	01.02.03	2024/01/02 13:29:17	2024/01/02 13:29:17
2	AXAX	B	x	NULL	-4.50	1970	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	-77	NULL	NULL	2024/12/31	00.00.00	2024/12/31 23:59:58	2024/12/31 23:59:58
EXPECTED
)
expect_output \
    "table replace values" \
    "$table_expected" \
    "SELECT id, REPLACE(v, 'a', 'X'), REPLACE(c, 'a', 'X'), REPLACE(txt, 'l', 'L'), "\
"REPLACE(i, 2, 'x'), REPLACE(d, '2', 'x'), REPLACE(y, '2', 'x'), "\
"REPLACE(dt, '-', '/'), REPLACE(tm, ':', '.'), REPLACE(dttm, '-', '/'), "\
"REPLACE(ts, '-', '/') FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table envelope" \
    "3	NULL	NULL	NULL
2	AxAx	A${DATABASE}A${DATABASE}	A0A0" \
    "DO 0; SELECT id, REPLACE(v, 'a', 'x'), REPLACE(v, 'a', DATABASE()), "\
"REPLACE(v, 'a', @@warning_count) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
REPLACE(v, 'a', 'x')	replaced
xbcxbc	AbcAbc
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT REPLACE(v, 'a', 'x'), REPLACE(v, 'a', 'A') AS replaced FROM t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "replace rejects zero arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REPLACE();" \
    "$DATABASE"

expect_error \
    "replace rejects one argument" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REPLACE(1);" \
    "$DATABASE"

expect_error \
    "replace rejects two arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REPLACE(1, 2);" \
    "$DATABASE"

expect_error \
    "replace rejects four arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REPLACE(1, 2, 3, 4);" \
    "$DATABASE"

expect_error \
    "bare replace keyword is not a scalar expression" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REPLACE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_replace_string_function_expectations: ok"
