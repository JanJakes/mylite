#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_binary_string_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_binary_string_defaults_expectations: $1" >&2
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

expect_output \
    "binary defaults materialize bytes" \
    "b	410000	3
v	4200	2
bs	610000	3
vs	62	1
b_empty	000000	3
v_empty		0
b_zero	000000	3
v_zero	0000	2
v_ff	4100FF	3" \
    "CREATE TABLE defaults_probe ("\
"b BINARY(3) DEFAULT X'41', "\
"v VARBINARY(3) DEFAULT X'4200', "\
"bs BINARY(3) DEFAULT 'a', "\
"vs VARBINARY(3) DEFAULT 'b', "\
"b_empty BINARY(3) DEFAULT X'', "\
"v_empty VARBINARY(3) DEFAULT X'', "\
"b_zero BINARY(3) DEFAULT X'0000', "\
"v_zero VARBINARY(3) DEFAULT X'0000', "\
"v_ff VARBINARY(3) DEFAULT X'4100FF'); "\
"INSERT INTO defaults_probe () VALUES (); "\
"SELECT 'b', HEX(b), LENGTH(b) FROM defaults_probe UNION ALL "\
"SELECT 'v', HEX(v), LENGTH(v) FROM defaults_probe UNION ALL "\
"SELECT 'bs', HEX(bs), LENGTH(bs) FROM defaults_probe UNION ALL "\
"SELECT 'vs', HEX(vs), LENGTH(vs) FROM defaults_probe UNION ALL "\
"SELECT 'b_empty', HEX(b_empty), LENGTH(b_empty) FROM defaults_probe UNION ALL "\
"SELECT 'v_empty', HEX(v_empty), LENGTH(v_empty) FROM defaults_probe UNION ALL "\
"SELECT 'b_zero', HEX(b_zero), LENGTH(b_zero) FROM defaults_probe UNION ALL "\
"SELECT 'v_zero', HEX(v_zero), LENGTH(v_zero) FROM defaults_probe UNION ALL "\
"SELECT 'v_ff', HEX(v_ff), LENGTH(v_ff) FROM defaults_probe;" \
    "$DATABASE"

expect_output \
    "information schema trims binary default display" \
    "b:0x41:4
v:0x42:4
bs:0x61:4
vs:0x62:4
b_empty:0x:2
v_empty::0
b_zero:0x:2
v_zero:0x:2
v_ff:0x41:4" \
    "SELECT CONCAT(COLUMN_NAME, ':', COALESCE(COLUMN_DEFAULT, '<NULL>'), ':', "\
"COALESCE(LENGTH(COLUMN_DEFAULT), -1)) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='defaults_probe' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
defaults_probe	CREATE TABLE `defaults_probe` (
  `b` binary(3) DEFAULT 'A\0\0',
  `v` varbinary(3) DEFAULT 'B\0',
  `bs` binary(3) DEFAULT 'a\0\0',
  `vs` varbinary(3) DEFAULT 'b',
  `b_empty` binary(3) DEFAULT '\0\0\0',
  `v_empty` varbinary(3) DEFAULT '',
  `b_zero` binary(3) DEFAULT '\0\0\0',
  `v_zero` varbinary(3) DEFAULT '\0\0',
  `v_ff` varbinary(3) DEFAULT 0x4100FF
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders binary defaults" \
    "$show_create_expected" \
    "SHOW CREATE TABLE defaults_probe;" \
    "$DATABASE"

expect_output \
    "alter set default and drop default" \
    "4100	2	42	1" \
    "CREATE TABLE alter_probe (id INT, b BINARY(2), v VARBINARY(2)); "\
"ALTER TABLE alter_probe ALTER COLUMN b SET DEFAULT X'41'; "\
"ALTER TABLE alter_probe ALTER COLUMN v SET DEFAULT X'42'; "\
"INSERT INTO alter_probe (id) VALUES (1); "\
"SELECT COALESCE(HEX(b), '<NULL>'), COALESCE(LENGTH(b), -1), HEX(v), LENGTH(v) "\
"FROM alter_probe ORDER BY id;" \
    "$DATABASE"

alter_drop_show_create_expected=$(cat <<\EXPECTED
alter_probe	CREATE TABLE `alter_probe` (
  `id` int DEFAULT NULL,
  `b` binary(2),
  `v` varbinary(2) DEFAULT 'B'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter drop default removes explicit binary default" \
    "$alter_drop_show_create_expected" \
    "ALTER TABLE alter_probe ALTER COLUMN b DROP DEFAULT; SHOW CREATE TABLE alter_probe;" \
    "$DATABASE"

expect_output \
    "alter add column backfills binary defaults" \
    "4100	2	42	1" \
    "CREATE TABLE add_probe (id INT); "\
"INSERT INTO add_probe VALUES (1); "\
"ALTER TABLE add_probe ADD COLUMN b BINARY(2) DEFAULT X'41'; "\
"ALTER TABLE add_probe ADD COLUMN v VARBINARY(2) DEFAULT X'42'; "\
"SELECT HEX(b), LENGTH(b), HEX(v), LENGTH(v) FROM add_probe;" \
    "$DATABASE"

expect_error \
    "binary overlength default" \
    1067 \
    42000 \
    "Invalid default value" \
    "CREATE TABLE bad_binary_default (b BINARY(3) DEFAULT X'41424344');" \
    "$DATABASE"

expect_error \
    "varbinary overlength default" \
    1067 \
    42000 \
    "Invalid default value" \
    "CREATE TABLE bad_varbinary_default (v VARBINARY(3) DEFAULT X'41424344');" \
    "$DATABASE"

expect_error \
    "blob literal default remains unsupported upstream" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_blob_default (b BLOB DEFAULT X'41');" \
    "$DATABASE"

expect_accepts \
    "blob expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_blob_expression_default (b BLOB DEFAULT (X'41'));" \
    "$DATABASE"
