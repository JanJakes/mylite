#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_soundex_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_soundex_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names \
            --default-character-set=utf8mb4 "$@"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "core scalar results" \
    "H400	Q36324	R163	R163	R150	A2613	T520	P236		A000	E000	A000	B2312		A120	A100	O165	NULL		A1231	A000	B000" \
    "SELECT SOUNDEX('Hello'), SOUNDEX('Quadratically'), SOUNDEX('Robert'), "\
"SOUNDEX('Rupert'), SOUNDEX('Rubin'), SOUNDEX('Ashcraft'), SOUNDEX('Tymczak'), "\
"SOUNDEX('Pfister'), SOUNDEX(''), SOUNDEX('a'), SOUNDEX('e'), "\
"SOUNDEX('aeiou'), SOUNDEX('bcdfg'), SOUNDEX('123'), SOUNDEX('  abc'), "\
"SOUNDEX('a-b'), SOUNDEX('O''Brien'), SOUNDEX(NULL), SOUNDEX(12345), "\
"SOUNDEX('abc123def'), SOUNDEX('Aaaa'), SOUNDEX('BbBb');" \
    "$DATABASE"

expect_output \
    "leading separators and duplicate state" \
    "A120	A120	A120	A120			B000	B000	B000	B000	B000	B210	A120	A320	A132	A12312451262312	15" \
    "SELECT SOUNDEX('1abc'), SOUNDEX('-abc'), SOUNDEX(' abc'), SOUNDEX('.abc'), "\
"SOUNDEX('1'), SOUNDEX(' '), SOUNDEX('BAB'), SOUNDEX('BHB'), SOUNDEX('BWB'), "\
"SOUNDEX('BFB'), SOUNDEX('BPB'), SOUNDEX('BCB'), SOUNDEX('abc'), "\
"SOUNDEX('adc'), SOUNDEX('abdc'), SOUNDEX('abcdefghijklmnopqrstuvwxyz'), "\
"LENGTH(SOUNDEX('abcdefghijklmnopqrstuvwxyz'));" \
    "$DATABASE"

expect_output \
    "utf8 observations" \
    "é000	é246	🙂000	é246	A120	A120	é120	🙂120	🙂123" \
    "SELECT SOUNDEX('é'), SOUNDEX('éclair'), SOUNDEX('🙂'), "\
"SOUNDEX('éclair'), SOUNDEX('aébc'), SOUNDEX('abéc'), "\
"SOUNDEX('éébc'), SOUNDEX('🙂abc'), SOUNDEX('🙂bcd');" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE strings (id INT, v VARCHAR(32), txt TEXT, i INT, d DECIMAL(8,2)); "\
"INSERT INTO strings VALUES "\
"(1, 'Robert', 'Quadratically', 123, -12.30), "\
"(2, 'Ashcraft', 'Pfister', NULL, NULL), "\
"(3, NULL, NULL, 0, 0.00);" \
    "$DATABASE" >/dev/null

table_projection_expected=$(printf '1\tR163\tQ36324\t\t\n2\tA2613\tP236\tNULL\tNULL\n3\tNULL\tNULL\t\t')
expect_output \
    "table projection" \
    "$table_projection_expected" \
    "SELECT id, SOUNDEX(v), SOUNDEX(txt), SOUNDEX(i), SOUNDEX(d) "\
"FROM strings ORDER BY id;" \
    "$DATABASE"

expect_output \
    "select status" \
    "A120
-1	0" \
    "SELECT SOUNDEX('abc'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "do status" \
    "0	0" \
    "DO SOUNDEX('abc'), SOUNDEX(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SOUNDEX'" \
    "SELECT SOUNDEX();" \
    "$DATABASE"

expect_error \
    "too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SOUNDEX'" \
    "SELECT SOUNDEX('a', 'b');" \
    "$DATABASE"
