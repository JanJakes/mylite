#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_tbl_i64arith_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_backed_signed_integer_arithmetic_projection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw "$@"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null
run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE t(id INT, a INT, i INTEGER, b BIGINT, n INT NULL, v VARCHAR(10)); "\
"INSERT INTO t VALUES "\
"(1, 2, 3, 9223372036854775806, NULL, 'x'), "\
"(2, -5, 7, -9223372036854775807, 10, 'y'), "\
"(3, 0, -2, 0, NULL, 'z');" >/dev/null

core_expected=$(cat <<EXPECTED
5	-1	6	7	7	10	8	NULL	3	2	6	-2	NULL	NULL
2	-12	-35	0	0	4	9	11	-4	-5	3	-13	NULL	NULL
-2	2	0	5	5	-4	-4	NULL	1	0	-1	1	NULL	NULL
EXPECTED
)
expect_output \
    "core table-backed signed integer arithmetic" \
    "$core_expected" \
    "USE ${DATABASE}; "\
"SELECT a+i, a-i, a*i, a+5, 5+a, (a+i)*2, a+i*2, n+1, TRUE+a, "\
"FALSE+a, a+i+1, a-i-1, a+NULL, NULL+a FROM t ORDER BY id;"

signed_literal_expected=$(cat <<EXPECTED
-3	7
EXPECTED
)
expect_output \
    "signed integer literal operands" \
    "$signed_literal_expected" \
    "USE ${DATABASE}; SELECT a+-5 AS signed_literal, a- -5 AS signed_subtract "\
"FROM t WHERE id = 1;"

qualified_expected=$(cat <<EXPECTED
5
5
EXPECTED
)
expect_output \
    "qualified arithmetic operands" \
    "$qualified_expected" \
    "USE ${DATABASE}; SELECT t.a+t.i FROM t WHERE t.id = 1; "\
"SELECT x.a+x.i FROM t AS x WHERE x.id = 1;"

label_expected=$(cat <<EXPECTED
expr_alias	(a+i)*2
5	10
EXPECTED
)
expect_output_with_headers \
    "arithmetic labels" \
    "$label_expected" \
    "USE ${DATABASE}; SELECT a+i AS expr_alias, (a+i)*2 FROM t WHERE id = 1;"

envelope_expected=$(cat <<EXPECTED
-2
2
EXPECTED
)
expect_output \
    "where order limit envelope" \
    "$envelope_expected" \
    "USE ${DATABASE}; SELECT a+i AS limited FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;"

warning_expected=$(cat <<EXPECTED
5
0
-1
EXPECTED
)
expect_output \
    "warning count and row count after arithmetic select" \
    "$warning_expected" \
    "USE ${DATABASE}; SELECT a+i FROM t WHERE id = 1; SELECT @@warning_count; SELECT ROW_COUNT();"

division_expected=$(cat <<EXPECTED
3.5000	3	1	1	-3	-1	NULL	NULL	NULL	NULL
4
EXPECTED
)
expect_output \
    "division and modulo arithmetic" \
    "$division_expected" \
    "USE ${DATABASE}; "\
"SELECT 7 / 2 AS quotient, 7 DIV 2 AS int_quotient, 7 % 2 AS remainder, "\
"MOD(7, 2) AS mod_function, -7 DIV 2 AS negative_int_quotient, "\
"-7 % 2 AS negative_remainder, 7 / 0 AS divide_zero, "\
"7 DIV 0 AS int_divide_zero, 7 % 0 AS mod_zero, MOD(7, 0) AS mod_function_zero "\
"FROM t WHERE id = 1; SELECT @@warning_count;"

string_expected=$(cat <<EXPECTED
1
1
1
3
EXPECTED
)
expect_output \
    "string numeric arithmetic warnings" \
    "$string_expected" \
    "USE ${DATABASE}; SELECT v+1 FROM t ORDER BY id; SELECT @@warning_count;"

expect_error \
    "signed arithmetic overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "USE ${DATABASE}; SELECT b+2 FROM t WHERE id = 1;"

no_match_expected=""
expect_output \
    "no matched rows skip overflow evaluation" \
    "$no_match_expected" \
    "USE ${DATABASE}; SELECT b+2 FROM t WHERE id = 99;"

printf '%s\n' "mysql_baseline_table_backed_signed_integer_arithmetic_projection_expectations: ok"
