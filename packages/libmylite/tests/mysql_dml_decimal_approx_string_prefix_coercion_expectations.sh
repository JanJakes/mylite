#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_dml_decimal_approx_string_prefix_coercion_$$"

fail() {
    printf '%s\n' "mysql_dml_decimal_approx_string_prefix_coercion_expectations: $1" >&2
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
run_mysql \
    "CREATE TABLE nums(id INT PRIMARY KEY, d DECIMAL(5,2), du DECIMAL(5,2) UNSIGNED, "\
"f DOUBLE, fu DOUBLE UNSIGNED, fl FLOAT, flu FLOAT UNSIGNED);" \
    "$DATABASE" >/dev/null

expect_output \
    "strict accepted string forms" \
    "status	4	0	0
row	1	12.30	100	-2.5
row	2	100.00	100	100
row	3	0.50	0.5	0.5
row	4	100.00	100	100" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; "\
"INSERT INTO nums(id, d, f, fl) VALUES "\
"(1, ' 12.30 ', ' 1e2 ', ' -2.5 '), "\
"(2, '1e2', '1e2', '1e2'), "\
"(3, '.5', '.5', '.5'), "\
"(4, '1.e2', '1.e2', '1.e2'); "\
"SELECT 'status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums ORDER BY id;" \
    "$DATABASE"

expect_error \
    "strict decimal trailing text" \
    1366 \
    HY000 \
    "Incorrect decimal value: '12.3abc' for column 'd' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, d) VALUES (5, '12.3abc');" \
    "$DATABASE"

expect_error \
    "strict decimal incomplete exponent" \
    1366 \
    HY000 \
    "Incorrect decimal value: '1e' for column 'd' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, d) VALUES (5, '1e');" \
    "$DATABASE"

expect_error \
    "strict decimal invalid string" \
    1366 \
    HY000 \
    "Incorrect decimal value: 'abc123' for column 'd' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, d) VALUES (5, 'abc123');" \
    "$DATABASE"

expect_error \
    "strict decimal range" \
    1264 \
    22003 \
    "Out of range value for column 'd' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, d) VALUES (5, '9999.99');" \
    "$DATABASE"

expect_error \
    "strict decimal unsigned negative" \
    1264 \
    22003 \
    "Out of range value for column 'du' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, du) VALUES (5, '-1');" \
    "$DATABASE"

expect_error \
    "strict approximate trailing text" \
    1265 \
    01000 \
    "Data truncated for column 'f' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, f) VALUES (5, '1e2abc');" \
    "$DATABASE"

expect_error \
    "strict approximate invalid string" \
    1265 \
    01000 \
    "Data truncated for column 'f' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, f) VALUES (5, 'abc123');" \
    "$DATABASE"

expect_error \
    "strict approximate double range" \
    1264 \
    22003 \
    "Out of range value for column 'f' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, f) VALUES (5, '1e309');" \
    "$DATABASE"

expect_error \
    "strict approximate float range" \
    1264 \
    22003 \
    "Out of range value for column 'fl' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; INSERT INTO nums(id, fl) VALUES (5, '1e39');" \
    "$DATABASE"

expect_output \
    "insert ignore trailing adjustment" \
    "Note	1265	Data truncated for column 'd' at row 1
Warning	1265	Data truncated for column 'f' at row 1
Warning	1265	Data truncated for column 'fl' at row 1
status	1	3	0
row	10	12.30	100	3.5" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id, d, f, fl) "\
"VALUES (10, '12.3abc', '1e2abc', '3.5abc'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums;" \
    "$DATABASE"

expect_output \
    "insert ignore invalid adjustment" \
    "Warning	1366	Incorrect decimal value: 'abc123' for column 'd' at row 1
Warning	1265	Data truncated for column 'f' at row 1
Warning	1265	Data truncated for column 'fl' at row 1
status	1	3	0
row	11	0.00	0	0" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id, d, f, fl) "\
"VALUES (11, 'abc123', 'abc123', 'abc123'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums;" \
    "$DATABASE"

expect_output \
    "insert ignore range adjustment" \
    "Note	1265	Data truncated for column 'd' at row 1
Warning	1264	Out of range value for column 'd' at row 1
Warning	1264	Out of range value for column 'f' at row 1
Warning	1265	Data truncated for column 'fl' at row 1
Warning	1264	Out of range value for column 'fl' at row 1
status	1	5	0
row	12	999.99	1.7976931348623157e308	3.40282e38" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id, d, f, fl) "\
"VALUES (12, '9999.99abc', '1e309abc', '1e39abc'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums;" \
    "$DATABASE"

expect_output \
    "insert ignore unsigned negative adjustment" \
    "Note	1265	Data truncated for column 'du' at row 1
Warning	1264	Out of range value for column 'du' at row 1
Warning	1265	Data truncated for column 'fu' at row 1
Warning	1264	Out of range value for column 'fu' at row 1
Warning	1265	Data truncated for column 'flu' at row 1
Warning	1264	Out of range value for column 'flu' at row 1
status	1	6	0
row	13	0.00	0	0" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id, du, fu, flu) "\
"VALUES (13, '-1abc', '-1abc', '-1abc'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, du, fu, flu FROM nums;" \
    "$DATABASE"

expect_output \
    "nonstrict invalid adjustment" \
    "Warning	1366	Incorrect decimal value: 'abc123' for column 'd' at row 1
Warning	1265	Data truncated for column 'f' at row 1
Warning	1265	Data truncated for column 'fl' at row 1
status	1	3	0
row	20	0.00	0	0" \
    "SET sql_mode=''; "\
"TRUNCATE nums; "\
"INSERT INTO nums(id, d, f, fl) VALUES (20, 'abc123', 'abc123', 'abc123'); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums;" \
    "$DATABASE"

expect_output \
    "nonstrict update matched rows warn no-match does not" \
    "Note	1265	Data truncated for column 'd' at row 1
Warning	1265	Data truncated for column 'f' at row 1
Warning	1265	Data truncated for column 'fl' at row 1
Note	1265	Data truncated for column 'd' at row 2
Warning	1265	Data truncated for column 'f' at row 2
Warning	1265	Data truncated for column 'fl' at row 2
matched	2	6	0
row	30	7.89	80	9.5
row	31	7.89	80	9.5
nomatch	0	0	0	0" \
    "SET sql_mode=''; "\
"TRUNCATE nums; "\
"INSERT INTO nums(id, d, f, fl) VALUES (30, 0, 0, 0), (31, 0, 0, 0); "\
"UPDATE nums SET d = '7.89abc', f = '8e1abc', fl = '9.5abc' WHERE id IN (30, 31); "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'matched', @rc, @cond, @@error_count; "\
"SELECT 'row', id, d, f, fl FROM nums ORDER BY id; "\
"UPDATE nums SET d = 'bad', f = 'bad', fl = 'bad' WHERE id = 999; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SELECT 'nomatch', @rc, @cond, @@warning_count, @@error_count;" \
    "$DATABASE"
