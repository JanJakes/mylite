#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_hex_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_hex_function_expectations: $1" >&2
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

scalar_expected=$(cat <<EXPECTED
616263	C3A9	610062		0061	6162	414243	414243	414243	FF	FFFFFFFFFFFFFFFF	1	0	NULL
-1	0
EXPECTED
)
expect_output \
    "scalar hex values" \
    "$scalar_expected" \
    "DO 0; SELECT HEX('abc'), HEX('é'), HEX('a\\0b'), HEX(''), "\
"HEX(X'0061'), HEX(0x6162), HEX(CAST('ABC' AS BINARY)), HEX(CONVERT('ABC', BINARY)), "\
"HEX(CONVERT('ABC' USING utf8mb4)), HEX(255), HEX(-1), HEX(TRUE), HEX(FALSE), HEX(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual whitespace value" \
    "61" \
    "SELECT HEX ('a') FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO HEX('abc'), HEX(NULL), HEX(255); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(10), c CHAR(5), txt TEXT, b BINARY(3), vb VARBINARY(3), bl BLOB, bi BIGINT"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'é', 'a  ', 'hello', 'A', X'410042', X'00FF', -1), "\
"(2, '', '', '', X'', X'', X'', 255), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	C3A9	61	68656C6C6F	410000	410042	00FF	FFFFFFFFFFFFFFFF
2				000000			FF
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table hex values" \
    "$table_expected" \
    "SELECT id, HEX(v), HEX(c), HEX(txt), HEX(b), HEX(vb), HEX(bl), HEX(bi) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	FF" \
    "SELECT id, HEX(bi) AS h FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
HEX(v)	h
C3A9	FFFFFFFFFFFFFFFF
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT HEX(v), HEX(bi) AS h FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred decimal bit predicate" \
    "C	2	A	1" \
    "CREATE TABLE deferred (d DECIMAL(5,2), f DOUBLE, bitv BIT(4)); "\
"INSERT INTO deferred VALUES (12.30, 1.9, b'1010'); "\
"SELECT HEX(d), HEX(f), HEX(bitv), (SELECT COUNT(*) FROM t WHERE HEX(v) = 'C3A9') FROM deferred;" \
    "$DATABASE"

expect_error \
    "hex rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'HEX'" \
    "SELECT HEX();" \
    "$DATABASE"

expect_error \
    "hex rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'HEX'" \
    "SELECT HEX('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_hex_function_expectations: ok"
