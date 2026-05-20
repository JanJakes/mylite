#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_dml_int_string_prefix_coercion_$$"

fail() {
    printf '%s\n' "mysql_dml_int_string_prefix_coercion_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" \
                -uroot --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" \
                mysql -uroot --batch --raw --skip-column-names "$@"
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "strict whitespace and rounding" \
    "status	1	0	0
row	1	123	12	-3	100" \
    "CREATE TABLE nums(id INT PRIMARY KEY, i INT, u INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED); "\
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO nums VALUES (1, ' 123 ', '\t12\n', '-2.5', '1e2'); "\
"SELECT 'status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'row', id, i, u, b, bu FROM nums;" \
    "$DATABASE"

expect_output \
    "strict boundary decimals and malformed exponent tails" \
    "status	8	0	0
row	5	NULL	9223372036854775807	9223372036854775807
row	6	NULL	9223372036854775807	9223372036854775807
row	7	1	NULL	NULL
row	8	1	NULL	NULL
row	9	1	NULL	NULL
row	10	1	NULL	NULL
row	11	-1	NULL	NULL
row	12	100	NULL	NULL" \
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; "\
"INSERT INTO nums(id, i, b, bu) VALUES "\
"(5, NULL, '9223372036854775807.0', '9223372036854775807.0'), "\
"(6, NULL, '9.223372036854775807e18', '9.223372036854775807e18'), "\
"(7, '1e', NULL, NULL), (8, '1e+', NULL, NULL), "\
"(9, '1e-', NULL, NULL), (10, '.5', NULL, NULL), "\
"(11, '-.5', NULL, NULL), (12, '1.e2', NULL, NULL); "\
"SELECT 'status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'row', id, i, b, bu FROM nums ORDER BY id;" \
    "$DATABASE"

expect_error \
    "strict numeric prefix truncation" \
    1265 \
    01000 \
    "Data truncated for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '123abc');" \
    "$DATABASE"

expect_error \
    "strict decimal prefix truncation" \
    1265 \
    01000 \
    "Data truncated for column 'i' at row 1" \
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '1.2abc');" \
    "$DATABASE"

expect_error \
    "strict hexadecimal-looking prefix truncation" \
    1265 \
    01000 \
    "Data truncated for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '0x10');" \
    "$DATABASE"

expect_error \
    "strict incomplete exponent with suffix truncation" \
    1265 \
    01000 \
    "Data truncated for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '1efoo');" \
    "$DATABASE"

expect_error \
    "strict dotted integer with suffix truncation" \
    1265 \
    01000 \
    "Data truncated for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '1.foo');" \
    "$DATABASE"

expect_error \
    "strict invalid string" \
    1366 \
    HY000 \
    "Incorrect integer value: 'abc123' for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, 'abc123');" \
    "$DATABASE"

expect_error \
    "strict sign space invalid string" \
    1366 \
    HY000 \
    "Incorrect integer value: '+ 1' for column 'i' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (2, '+ 1');" \
    "$DATABASE"

expect_error \
    "strict unsigned negative range" \
    1264 \
    22003 \
    "Out of range value for column 'u' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; INSERT INTO nums(id, u) VALUES (2, '-1');" \
    "$DATABASE"

expect_output \
    "nonstrict warning adjustment" \
    "Warning	1265	Data truncated for column 'i' at row 1
Warning	1366	Incorrect integer value: 'abc123' for column 'u' at row 1
Warning	1264	Out of range value for column 'b' at row 1
Warning	1264	Out of range value for column 'bu' at row 1
status	1	4	0
row	3	123	0	9223372036854775807	0" \
    "SET sql_mode=''; "\
"TRUNCATE nums; "\
"INSERT INTO nums(id, i, u, b, bu) "\
"VALUES (3, '123abc', 'abc123', '999999999999999999999', '-1'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, i, u, b, bu FROM nums;" \
    "$DATABASE"

expect_output \
    "insert ignore warning adjustment" \
    "Warning	1265	Data truncated for column 'i' at row 1
Warning	1366	Incorrect integer value: 'abc123' for column 'u' at row 1
Warning	1264	Out of range value for column 'b' at row 1
Warning	1264	Out of range value for column 'bu' at row 1
status	1	4	0
row	4	100	0	9223372036854775807	0" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id, i, u, b, bu) "\
"VALUES (4, '1e2x', 'abc123', '999999999999999999999', '-1'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, i, u, b, bu FROM nums;" \
    "$DATABASE"

expect_output \
    "nonstrict update matched rows warn no-match does not" \
    "Warning	1265	Data truncated for column 'i' at row 1
Warning	1265	Data truncated for column 'i' at row 2
matched	2	2	0
rows	1	123
rows	2	123
nomatch	0	0	0" \
    "SET sql_mode=''; "\
"TRUNCATE nums; INSERT INTO nums(id, i) VALUES (1, 0), (2, 0); "\
"UPDATE nums SET i = '123abc' WHERE id IN (1, 2); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'matched', @rc, @cond, @@error_count; "\
"SELECT 'rows', id, i FROM nums ORDER BY id; "\
"UPDATE nums SET i = 'abc123' WHERE id = 999; "\
"SELECT 'nomatch', ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"
