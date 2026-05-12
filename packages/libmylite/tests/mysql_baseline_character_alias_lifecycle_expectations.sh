#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_character_alias_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_character_alias_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
c	char(1)	YES		q	
c0	char(0)	YES		NULL	
c2	char(2)	NO		NULL	
v	varchar(3)	YES		xy	
cv	varchar(4)	YES		NULL	
EXPECTED
)
expect_output \
    "show columns normalizes character aliases" \
    "$show_columns_expected" \
    "CREATE TABLE aliases ("\
"id INT NOT NULL, "\
"c CHARACTER DEFAULT 'q', "\
"c0 CHARACTER(0), "\
"c2 CHARACTER(2) NOT NULL, "\
"v CHARACTER VARYING(3) DEFAULT 'xy', "\
"cv CHAR VARYING(4)); "\
"SHOW COLUMNS FROM aliases;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
aliases	CREATE TABLE `aliases` (
  `id` int NOT NULL,
  `c` char(1) DEFAULT 'q',
  `c0` char(0) DEFAULT NULL,
  `c2` char(2) NOT NULL,
  `v` varchar(3) DEFAULT 'xy',
  `cv` varchar(4) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create normalizes character aliases" \
    "$show_create_expected" \
    "SHOW CREATE TABLE aliases;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
c	char	char(1)	1	4	utf8mb4	utf8mb4_0900_ai_ci	YES	q
c0	char	char(0)	0	0	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
c2	char	char(2)	2	8	utf8mb4	utf8mb4_0900_ai_ci	NO	NULL
v	varchar	varchar(3)	3	12	utf8mb4	utf8mb4_0900_ai_ci	YES	xy
cv	varchar	varchar(4)	4	16	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
EXPECTED
)
expect_output \
    "information schema normalizes character aliases" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'aliases' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

readback_expected=$(cat <<\EXPECTED
1	[x]	1	[]	0	[ab]	2	[a  ]	3	[b   ]	4
EXPECTED
)
expect_output \
    "aliases use normalized char and varchar row semantics" \
    "$readback_expected" \
    "INSERT INTO aliases VALUES (1, 'x ', '', 'ab  ', 'a  ', 'b   '); "\
"SELECT id, CONCAT('[', c, ']'), LENGTH(c), CONCAT('[', c0, ']'), LENGTH(c0), "\
"CONCAT('[', c2, ']'), LENGTH(c2), CONCAT('[', v, ']'), LENGTH(v), "\
"CONCAT('[', cv, ']'), LENGTH(cv) FROM aliases;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	[z]	1
0	0	[z]	1
1	0	[cc]	2
EXPECTED
)
expect_output \
    "aliases use normalized update affected rows" \
    "$update_expected" \
    "UPDATE aliases SET c2 = 'z ' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', c2, ']'), LENGTH(c2) FROM aliases; "\
"UPDATE aliases SET c2 = 'z ' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', c2, ']'), LENGTH(c2) FROM aliases; "\
"UPDATE aliases SET cv = 'cc' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', cv, ']'), LENGTH(cv) FROM aliases;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
extra	char(2)	NO		x	
vv	varchar(2)	YES		y	
EXPECTED
)
expect_output \
    "alter add normalizes character aliases" \
    "$alter_expected" \
    "ALTER TABLE aliases ADD COLUMN extra CHARACTER(2) NOT NULL DEFAULT 'x'; "\
"ALTER TABLE aliases ADD COLUMN vv CHARACTER VARYING(2) DEFAULT 'y'; "\
"SHOW COLUMNS FROM aliases WHERE Field IN ('extra', 'vv');" \
    "$DATABASE"

expect_error \
    "character varying requires length" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_varying (v CHARACTER VARYING);" \
    "$DATABASE"

expect_error \
    "char varying requires length" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_char_varying (v CHAR VARYING);" \
    "$DATABASE"

expect_error \
    "character empty length syntax" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_empty (v CHARACTER());" \
    "$DATABASE"

expect_error \
    "character varying signed length syntax" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_negative (v CHARACTER VARYING(-1));" \
    "$DATABASE"

expect_error \
    "character length cap" \
    1074 \
    42000 \
    "Column length too big" \
    "CREATE TABLE bad_char_length (v CHARACTER(256));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts wider varying aliases deferred by MyLite" \
    "CREATE TABLE upstream_wide_varying (v CHARACTER VARYING(256)); DROP TABLE upstream_wide_varying;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts binary character byte aliases deferred by MyLite" \
    "CREATE TABLE upstream_byte (v CHARACTER BYTE, c CHAR BYTE); DROP TABLE upstream_byte;" \
    "$DATABASE"
