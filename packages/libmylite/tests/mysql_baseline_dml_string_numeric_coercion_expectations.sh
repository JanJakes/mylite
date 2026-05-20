#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_dml_string_numeric_coercion_$$"

fail() {
    printf '%s\n' "mysql_baseline_dml_string_numeric_coercion_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | mysql --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
    "strict quoted numeric values store" \
    "strict_insert	1	1	0
row	1	123	456	-9223372036854775808	9223372036854775807	12.35	125	3.5" \
    "CREATE TABLE nums ("\
"id INT PRIMARY KEY, i INT, u INT UNSIGNED, bi BIGINT, bu BIGINT UNSIGNED, "\
"d DECIMAL(5,2), f DOUBLE, fl FLOAT); "\
"INSERT INTO nums VALUES "\
"(1,'123','+456','-9223372036854775808','9223372036854775807',"\
"'12.345','1.25e2','3.5'); "\
"SELECT 'strict_insert', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'row', id, i, u, bi, bu, d, f, fl FROM nums;" \
    "$DATABASE"

expect_output \
    "strict quoted decimal warning rows" \
    "Note	1265	Data truncated for column 'd' at row 1" \
    "TRUNCATE nums; "\
"INSERT INTO nums(id,d) VALUES (2,'12.345'); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "strict quoted update and duplicate update" \
    "update_values	1	1	0
updated_row	1	-10	20	10.00	-25
duplicate_values	2	1	0
duplicate_row	1	42	3.46	40" \
    "TRUNCATE nums; "\
"INSERT INTO nums(id,i,u,d,f) VALUES (1,1,2,3.00,4.00); "\
"UPDATE nums SET i='-10', u='+20', d='9.999', f='-2.5e1' WHERE id=1; "\
"SELECT 'update_values', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'updated_row', id, i, u, d, f FROM nums; "\
"INSERT INTO nums(id,i,u,d,f) VALUES (1,1,2,3.00,4.00) "\
"ON DUPLICATE KEY UPDATE i='42', d='3.456', f='4e1'; "\
"SELECT 'duplicate_values', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'duplicate_row', id, i, d, f FROM nums;" \
    "$DATABASE"

expect_error \
    "strict invalid integer string" \
    1366 \
    HY000 \
    "Incorrect integer value: 'abc' for column 'i' at row 1" \
    "TRUNCATE nums; INSERT INTO nums(id,i) VALUES (3,'abc');" \
    "$DATABASE"

expect_error \
    "strict integer string out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "TRUNCATE nums; INSERT INTO nums(id,i) VALUES (3,'999999999999999999999');" \
    "$DATABASE"

expect_error \
    "strict decimal string out of range" \
    1264 \
    22003 \
    "Out of range value for column 'd' at row 1" \
    "TRUNCATE nums; INSERT INTO nums(id,d) VALUES (3,'9999.99');" \
    "$DATABASE"

expect_output \
    "insert ignore quoted numeric clipping" \
    "ignore_insert	1	3	0
ignore_row	3	2147483647	0	999.99" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id,i,u,d) "\
"VALUES (3,'999999999999999999999','-1','9999.99'); "\
"SELECT 'ignore_insert', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'ignore_row', id, i, u, d FROM nums;" \
    "$DATABASE"

expect_output \
    "insert ignore quoted numeric warning rows" \
    "Warning	1264	Out of range value for column 'i' at row 1
Warning	1264	Out of range value for column 'u' at row 1
Warning	1264	Out of range value for column 'd' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE nums; "\
"INSERT IGNORE INTO nums(id,i,u,d) "\
"VALUES (3,'999999999999999999999','-1','9999.99'); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred leading-space integer string" \
    "SET sql_mode='STRICT_TRANS_TABLES'; TRUNCATE nums; INSERT INTO nums(id,i) VALUES (4,' 123');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred quoted exponent decimal string" \
    "SET sql_mode='STRICT_TRANS_TABLES'; TRUNCATE nums; INSERT INTO nums(id,d) VALUES (5,'1e2');" \
    "$DATABASE"
