#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_compression_random_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_compression_random_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --binary-as-hex=1 --batch --raw --skip-column-names "$@"
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

compression_expected=$(cat <<EXPECTED
	0	0		0
03000000789C4B4C4A0600024D0127	15	616263	3
1	1	1
EXPECTED
)
expect_output \
    "compression scalar values" \
    "$compression_expected" \
    "SELECT HEX(COMPRESS('')), LENGTH(COMPRESS('')), UNCOMPRESS(COMPRESS('')) IS NULL, "\
"HEX(UNCOMPRESS(COMPRESS(''))), UNCOMPRESSED_LENGTH(COMPRESS('')); "\
"SELECT HEX(COMPRESS('abc')), LENGTH(COMPRESS('abc')), HEX(UNCOMPRESS(COMPRESS('abc'))), "\
"UNCOMPRESSED_LENGTH(COMPRESS('abc')); "\
"SELECT COMPRESS(NULL) IS NULL, UNCOMPRESS(NULL) IS NULL, UNCOMPRESSED_LENGTH(NULL) IS NULL;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
1	0
Warning	1259	ZLIB: Input data corrupted
Warning	1259	ZLIB: Input data corrupted
EXPECTED
)
expect_output \
    "invalid compressed warnings" \
    "$invalid_expected" \
    "SELECT UNCOMPRESS('abc') IS NULL, UNCOMPRESSED_LENGTH('abc'); SHOW WARNINGS;" \
    "$DATABASE"

oversized_invalid_expected=$(cat <<EXPECTED
1	1073741823
Warning	1256	Uncompressed data size too large; the maximum size is 67108864 (probably, length of uncompressed data was corrupted)
EXPECTED
)
expect_output \
    "oversized invalid compressed warning" \
    "$oversized_invalid_expected" \
    "SELECT UNCOMPRESS(X'FFFFFFFF00') IS NULL, UNCOMPRESSED_LENGTH(X'FFFFFFFF00'); "\
"SHOW WARNINGS;" \
    "$DATABASE"

random_expected=$(cat <<EXPECTED
1	1024	1	4
1	2	2	1024
4
Warning	1292	Truncated incorrect INTEGER value: '4x'
EXPECTED
)
expect_output \
    "random byte values" \
    "$random_expected" \
    "SELECT LENGTH(RANDOM_BYTES(1)), LENGTH(RANDOM_BYTES(1024)), "\
"HEX(RANDOM_BYTES(4)) REGEXP '^[0-9A-F]{8}$', LENGTH(RANDOM_BYTES('4')); "\
"SELECT LENGTH(RANDOM_BYTES(1.4)), LENGTH(RANDOM_BYTES(1.5)), "\
"LENGTH(RANDOM_BYTES(1.9)), LENGTH(RANDOM_BYTES(1024.4)); "\
"SELECT LENGTH(RANDOM_BYTES('4x')); SHOW WARNINGS;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t(id INT, v VARCHAR(20), vb VARBINARY(20)); "\
"INSERT INTO t VALUES (1, 'abc', X'610062'), (2, '', X''), (3, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	616263	3	3	4
2		0	0	4
3	NULL	NULL	NULL	4
EXPECTED
)
expect_output \
    "row-backed compression and random values" \
    "$table_expected" \
    "SELECT id, HEX(UNCOMPRESS(COMPRESS(v))), UNCOMPRESSED_LENGTH(COMPRESS(v)), "\
"UNCOMPRESSED_LENGTH(COMPRESS(vb)), LENGTH(RANDOM_BYTES(4)) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "random zero length" \
    1690 \
    22003 \
    "length value is out of range in 'random_bytes'" \
    "SELECT RANDOM_BYTES(0);" \
    "$DATABASE"

expect_error \
    "random negative length" \
    1690 \
    22003 \
    "length value is out of range in 'random_bytes'" \
    "SELECT RANDOM_BYTES(-1);" \
    "$DATABASE"

expect_error \
    "random excessive length" \
    1690 \
    22003 \
    "length value is out of range in 'random_bytes'" \
    "SELECT RANDOM_BYTES(1025);" \
    "$DATABASE"

expect_error \
    "random nonnumeric string" \
    1690 \
    22003 \
    "length value is out of range in 'random_bytes'" \
    "SELECT RANDOM_BYTES('abc');" \
    "$DATABASE"

expect_error \
    "compress rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'COMPRESS'" \
    "SELECT COMPRESS();" \
    "$DATABASE"

expect_error \
    "random rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'RANDOM_BYTES'" \
    "SELECT RANDOM_BYTES(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_compression_random_functions_expectations: ok"
