#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_character_expression_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_character_expression_defaults_expectations: $1" >&2
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
                mysql -uroot --batch --raw --skip-column-names \
                    --default-character-set=utf8mb4 "$@"
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
    "character expression metadata and materialization" \
    "c	char(4)	YES		_utf8mb4\\'ab\\'	DEFAULT_GENERATED
vc	varchar(4)	YES		_utf8mb4\\'xy\\'	DEFAULT_GENERATED
ce	char(4)	YES		_utf8mb4\\'\\'	DEFAULT_GENERATED
v0	varchar(0)	YES		_utf8mb4\\'\\'	DEFAULT_GENERATED
vn	varchar(4)	YES		NULL	DEFAULT_GENERATED
c:_utf8mb4\\'ab\\':DEFAULT_GENERATED
vc:_utf8mb4\\'xy\\':DEFAULT_GENERATED
ce:_utf8mb4\\'\\':DEFAULT_GENERATED
v0:_utf8mb4\\'\\':DEFAULT_GENERATED
vn:NULL:DEFAULT_GENERATED
ab	xy			1" \
    "CREATE TABLE character_defaults ("\
"c CHAR(4) DEFAULT ('ab'), "\
"vc VARCHAR(4) DEFAULT ('xy'), "\
"ce CHAR(4) DEFAULT (''), "\
"v0 VARCHAR(0) DEFAULT (''), "\
"vn VARCHAR(4) DEFAULT (NULL)); "\
"SHOW COLUMNS FROM character_defaults; "\
"SELECT CONCAT(COLUMN_NAME, ':', COALESCE(COLUMN_DEFAULT, '<NULL>'), ':', EXTRA) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_defaults' "\
"ORDER BY ORDINAL_POSITION; "\
"INSERT INTO character_defaults () VALUES (); "\
"SELECT c, vc, ce, v0, vn IS NULL FROM character_defaults;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
character_defaults	CREATE TABLE `character_defaults` (
  `c` char(4) DEFAULT (_utf8mb4'ab'),
  `vc` varchar(4) DEFAULT (_utf8mb4'xy'),
  `ce` char(4) DEFAULT (_utf8mb4''),
  `v0` varchar(0) DEFAULT (_utf8mb4''),
  `vn` varchar(4) DEFAULT (NULL)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "character expression SHOW CREATE" \
    "$show_create_expected" \
    "SHOW CREATE TABLE character_defaults;" \
    "$DATABASE"

expect_output \
    "DML DEFAULT materialization and no-op row count" \
    "0	0
ab	xy" \
    "UPDATE character_defaults SET c = DEFAULT, vc = DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT c, vc FROM character_defaults;" \
    "$DATABASE"

expect_output \
    "ALTER column-definition variants preserve generated defaults" \
    "1	base	add
2	mod	chg" \
    "CREATE TABLE alter_character_defaults (id INT, v VARCHAR(5) DEFAULT ('base')); "\
"INSERT INTO alter_character_defaults (id) VALUES (1); "\
"ALTER TABLE alter_character_defaults ADD COLUMN added VARCHAR(5) DEFAULT ('add'); "\
"ALTER TABLE alter_character_defaults MODIFY COLUMN v VARCHAR(5) DEFAULT ('mod'); "\
"ALTER TABLE alter_character_defaults CHANGE COLUMN added changed VARCHAR(5) DEFAULT ('chg'); "\
"INSERT INTO alter_character_defaults (id) VALUES (2); "\
"SELECT id, v, changed FROM alter_character_defaults ORDER BY id;" \
    "$DATABASE"

alter_set_expected=$(cat <<\EXPECTED
v	varchar(5)	YES		set	
alter_set_character_default	CREATE TABLE `alter_set_character_default` (
  `v` varchar(5) DEFAULT 'set'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "ALTER COLUMN SET DEFAULT stores ordinary default" \
    "$alter_set_expected" \
    "CREATE TABLE alter_set_character_default (v VARCHAR(5)); "\
"ALTER TABLE alter_set_character_default ALTER COLUMN v SET DEFAULT ('set'); "\
"SHOW COLUMNS FROM alter_set_character_default; "\
"SHOW CREATE TABLE alter_set_character_default;" \
    "$DATABASE"

expect_error \
    "NOT NULL generated NULL fails when materialized" \
    1048 \
    23000 \
    "cannot be null" \
    "CREATE TABLE not_null_character_default (v VARCHAR(3) NOT NULL DEFAULT (NULL)); "\
"INSERT INTO not_null_character_default () VALUES ();" \
    "$DATABASE"

expect_error \
    "overlength VARCHAR generated default fails when materialized" \
    1406 \
    22001 \
    "Data too long" \
    "CREATE TABLE varchar_overlength_default (v VARCHAR(3) DEFAULT ('abcd')); "\
"INSERT INTO varchar_overlength_default () VALUES ();" \
    "$DATABASE"

expect_error \
    "overlength CHAR generated default fails when materialized" \
    1406 \
    22001 \
    "Data too long" \
    "CREATE TABLE char_overlength_default (v CHAR(3) DEFAULT ('abcd')); "\
"INSERT INTO char_overlength_default () VALUES ();" \
    "$DATABASE"

expect_accepts \
    "numeric VARCHAR expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_varchar_numeric_expression_default (v VARCHAR(10) DEFAULT (1 + 2));" \
    "$DATABASE"

expect_accepts \
    "hex VARCHAR expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_varchar_hex_expression_default (v VARCHAR(10) DEFAULT (0x41));" \
    "$DATABASE"

expect_accepts \
    "function VARCHAR expression defaults are deferred by MyLite" \
    "CREATE TABLE mysql_varchar_function_expression_default "\
"(v VARCHAR(10) DEFAULT (CONCAT('a','b')));" \
    "$DATABASE"

expect_accepts \
    "binary generated defaults are deferred by MyLite" \
    "CREATE TABLE mysql_binary_expression_default (b BINARY(3) DEFAULT (X'41'));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_character_expression_defaults_expectations: ok"
