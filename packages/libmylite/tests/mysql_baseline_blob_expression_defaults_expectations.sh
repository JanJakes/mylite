#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_blob_expression_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_blob_expression_defaults_expectations: $1" >&2
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

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept, got [$output]"
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_error \
    "bare BLOB hex default remains invalid" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_blob_hex_default (b BLOB DEFAULT X'41');" \
    "$DATABASE"

expect_error \
    "bare BLOB string default remains invalid" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_blob_string_default (b BLOB DEFAULT 'abc');" \
    "$DATABASE"

expect_output \
    "BLOB expression metadata and materialization" \
    "tb	tinyblob	YES		0x4100ff	DEFAULT_GENERATED
b	blob	YES		0x01	DEFAULT_GENERATED
mb	mediumblob	YES		0x0abc	DEFAULT_GENERATED
lb	longblob	YES		X\\'\\'	DEFAULT_GENERATED
n	blob	YES		NULL	DEFAULT_GENERATED
tb:0x4100ff:DEFAULT_GENERATED
b:0x01:DEFAULT_GENERATED
mb:0x0abc:DEFAULT_GENERATED
lb:X\\'\\':DEFAULT_GENERATED
n:NULL:DEFAULT_GENERATED
4100FF	01	0ABC		1" \
    "CREATE TABLE blob_defaults ("\
"tb TINYBLOB DEFAULT (X'4100FF'), "\
"b BLOB DEFAULT (0x1), "\
"mb MEDIUMBLOB DEFAULT (0xabc), "\
"lb LONGBLOB DEFAULT (X''), "\
"n BLOB DEFAULT (NULL)); "\
"SHOW COLUMNS FROM blob_defaults; "\
"SELECT CONCAT(COLUMN_NAME, ':', COALESCE(COLUMN_DEFAULT, '<NULL>'), ':', EXTRA) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'blob_defaults' "\
"ORDER BY ORDINAL_POSITION; "\
"INSERT INTO blob_defaults () VALUES (); "\
"SELECT HEX(tb), HEX(b), HEX(mb), HEX(lb), n IS NULL FROM blob_defaults;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
blob_defaults	CREATE TABLE `blob_defaults` (
  `tb` tinyblob DEFAULT (0x4100ff),
  `b` blob DEFAULT (0x01),
  `mb` mediumblob DEFAULT (0x0abc),
  `lb` longblob DEFAULT (X''),
  `n` blob DEFAULT (NULL)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "BLOB expression SHOW CREATE" \
    "$show_create_expected" \
    "SHOW CREATE TABLE blob_defaults;" \
    "$DATABASE"

expect_output \
    "ALTER column-definition variants preserve BLOB expression defaults" \
    "1	NULL	4200
2	44	4200" \
    "CREATE TABLE alter_blob_defaults (id INT, b BLOB); "\
"INSERT INTO alter_blob_defaults (id) VALUES (1); "\
"ALTER TABLE alter_blob_defaults ADD COLUMN added BLOB DEFAULT (X'4200'); "\
"ALTER TABLE alter_blob_defaults MODIFY COLUMN b BLOB DEFAULT (0x43); "\
"ALTER TABLE alter_blob_defaults CHANGE COLUMN b renamed BLOB DEFAULT (X'44'); "\
"INSERT INTO alter_blob_defaults (id) VALUES (2); "\
"SELECT id, HEX(renamed), HEX(added) FROM alter_blob_defaults ORDER BY id;" \
    "$DATABASE"

expect_error \
    "ALTER COLUMN SET DEFAULT still rejects BLOB expression default" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE alter_set_blob_default (b BLOB); "\
"ALTER TABLE alter_set_blob_default ALTER COLUMN b SET DEFAULT (X'41');" \
    "$DATABASE"

expect_error \
    "NOT NULL BLOB DEFAULT NULL expression fails when materialized" \
    1048 \
    23000 \
    "cannot be null" \
    "CREATE TABLE not_null_blob_default (b BLOB NOT NULL DEFAULT (NULL)); "\
"INSERT INTO not_null_blob_default () VALUES ();" \
    "$DATABASE"

expect_error \
    "overlength TINYBLOB generated default fails when materialized" \
    1406 \
    22001 \
    "Data too long" \
    "CREATE TABLE tiny_overlength_default (b TINYBLOB DEFAULT "\
"(0x000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"\
"202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F"\
"404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F"\
"606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F"\
"808182838485868788898A8B8C8D8E8F909192939495969798999A9B9C9D9E9F"\
"A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"\
"C0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"\
"E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEFF0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF)); "\
"INSERT INTO tiny_overlength_default () VALUES ();" \
    "$DATABASE"

expect_accepts \
    "ordinary string BLOB expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_blob_string_expression_default (b BLOB DEFAULT ('abc'));" \
    "$DATABASE"

expect_accepts \
    "arithmetic BLOB expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_blob_arithmetic_expression_default (b BLOB DEFAULT (1 + 2));" \
    "$DATABASE"

expect_accepts \
    "BINARY generated defaults are deferred by MyLite" \
    "CREATE TABLE mysql_binary_expression_default (b BINARY(3) DEFAULT (X'41'));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_blob_expression_defaults_expectations: ok"
