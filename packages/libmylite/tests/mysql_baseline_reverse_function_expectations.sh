#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_reverse_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_reverse_function_expectations: $1" >&2
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
cba		NULL	54321	7-	1	0	aé	a🙂	0
-1	0
EXPECTED
)
expect_output \
    "scalar reverse values" \
    "$scalar_expected" \
    "DO 0; SET NAMES utf8mb4; SELECT REVERSE('abc'), REVERSE(''), REVERSE(NULL), "\
"REVERSE(12345), REVERSE(-7), REVERSE(TRUE), REVERSE(FALSE), REVERSE('éa'), "\
"REVERSE('🙂a'), @@warning_count; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "session and system reverse arguments" \
    "$(printf '%s' "$DATABASE" | rev)	0" \
    "DO 0; SELECT REVERSE(DATABASE()), REVERSE(@@warning_count);" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "cba	cba" \
    "SELECT REVERSE ('abc'), REVERSE(('abc')) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO REVERSE('abc'), REVERSE(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "binary input metadata" \
    "BA	4241	binary	binary" \
    "SELECT REVERSE(CAST('AB' AS BINARY)), HEX(REVERSE(CAST('AB' AS BINARY))), "\
"CHARSET(REVERSE(CAST('AB' AS BINARY))), COLLATION(REVERSE(CAST('AB' AS BINARY)));" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "\
"dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, b VARBINARY(8), f DOUBLE); "\
"INSERT INTO t VALUES "\
"(1, 'Abc', 'xy   ', 'hello', 123, 12.30, 2024, '2024-01-02', '01:02:03', "\
"'2024-01-02 03:04:05', '2024-01-02 03:04:05', X'616263', 1.25), "\
"(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, X'00', NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	cbA	yx	olleh	321	03.21	4202	20-10-4202	30:20:10	50:40:30 20-10-4202	50:40:30 20-10-4202
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table reverse values" \
    "$table_expected" \
    "SELECT id, REVERSE(v), REVERSE(c), REVERSE(txt), REVERSE(i), REVERSE(d), "\
"REVERSE(y), REVERSE(dt), REVERSE(tm), REVERSE(dttm), REVERSE(ts) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table envelope" \
    "1	cbA" \
    "SELECT id, REVERSE(v) FROM t WHERE id >= 1 ORDER BY id ASC LIMIT 1;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
REVERSE(v)	reversed
cbA	cbA
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT REVERSE(v), REVERSE(v) AS reversed FROM t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "reverse rejects zero arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REVERSE();" \
    "$DATABASE"

expect_error \
    "reverse rejects two arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT REVERSE('a', 'b');" \
    "$DATABASE"

expect_error \
    "bare reverse keyword is not a scalar expression" \
    1054 \
    42S22 \
    "Unknown column 'REVERSE' in 'field list'" \
    "SELECT REVERSE;" \
    "$DATABASE"

expect_error \
    "unknown no-source reverse argument" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT REVERSE(missing);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_reverse_function_expectations: ok"
