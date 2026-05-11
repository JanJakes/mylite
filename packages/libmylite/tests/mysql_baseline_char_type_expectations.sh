#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_char_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_char_type_expectations: $1" >&2
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
c	char(1)	YES		NULL	
c0	char(0)	YES		NULL	
c3	char(3)	YES		NULL	
c255	char(255)	YES		NULL	
nn	char(2)	NO		NULL	
EXPECTED
)
expect_output \
    "show columns renders char descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE chars ("\
"id INT NOT NULL, c CHAR, c0 CHAR(0), c3 CHAR(3), c255 CHAR(255), nn CHAR(2) NOT NULL); "\
"SHOW COLUMNS FROM chars;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
chars	CREATE TABLE `chars` (
  `id` int NOT NULL,
  `c` char(1) DEFAULT NULL,
  `c0` char(0) DEFAULT NULL,
  `c3` char(3) DEFAULT NULL,
  `c255` char(255) DEFAULT NULL,
  `nn` char(2) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders char descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE chars;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
c	char	char(1)	1	4	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
c0	char	char(0)	0	0	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
c3	char	char(3)	3	12	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
c255	char	char(255)	255	1020	utf8mb4	utf8mb4_0900_ai_ci	YES	NULL
nn	char	char(2)	2	8	utf8mb4	utf8mb4_0900_ai_ci	NO	NULL
EXPECTED
)
expect_output \
    "information schema renders char descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'chars' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

readback_expected=$(cat <<\EXPECTED
[x]	1	78	[]	0	[ab]	2	6162	[zz]	2	7A7A
EXPECTED
)
expect_output \
    "char values read back in default trimmed form" \
    "$readback_expected" \
    "INSERT INTO chars(id, c, c0, c3, nn) VALUES (1, 'x', '', 'ab  ', 'zz'); "\
"SELECT CONCAT('[', c, ']'), LENGTH(c), HEX(c), CONCAT('[', c0, ']'), LENGTH(c0), "\
"CONCAT('[', c3, ']'), LENGTH(c3), HEX(c3), CONCAT('[', nn, ']'), LENGTH(nn), HEX(nn) "\
"FROM chars WHERE id = 1;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0
[y]	1	79
1
[z]	1	7A
EXPECTED
)
expect_output \
    "char update affected rows use trimmed changed value" \
    "$update_expected" \
    "CREATE TABLE update_chars (id INT, c CHAR(1)); "\
"INSERT INTO update_chars VALUES (1, 'y '); "\
"UPDATE update_chars SET c = 'y ' WHERE id = 1; SELECT ROW_COUNT(); "\
"SELECT CONCAT('[', c, ']'), LENGTH(c), HEX(c) FROM update_chars WHERE id = 1; "\
"UPDATE update_chars SET c = 'z ' WHERE id = 1; SELECT ROW_COUNT(); "\
"SELECT CONCAT('[', c, ']'), LENGTH(c), HEX(c) FROM update_chars WHERE id = 1;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'c' at row 1
Warning	1265	Data truncated for column 'c0' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
Warning	1048	Column 'nn' cannot be null
2	[x]	[]	[abc]	[]	1	0	3	0
EXPECTED
)
expect_output \
    "mysql insert ignore char adjustments" \
    "$ignore_expected" \
    "INSERT IGNORE INTO chars(id, c, c0, c3, nn) VALUES (2, 'xy', 'a', 'abcd', NULL); "\
"SHOW WARNINGS; "\
"SELECT id, CONCAT('[', c, ']'), CONCAT('[', c0, ']'), CONCAT('[', c3, ']'), "\
"CONCAT('[', nn, ']'), LENGTH(c), LENGTH(c0), LENGTH(c3), LENGTH(nn) "\
"FROM chars WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "char nonspace overlength fails in strict mode" \
    1406 \
    "22001" \
    "Data too long for column 'c3' at row 1" \
    "INSERT INTO chars(id, c3, nn) VALUES (3, 'abcd', 'ok');" \
    "$DATABASE"

expect_error \
    "char zero rejects nonspace data in strict mode" \
    1406 \
    "22001" \
    "Data too long for column 'c0' at row 1" \
    "INSERT INTO chars(id, c0, nn) VALUES (4, 'a', 'ok');" \
    "$DATABASE"

expect_error \
    "char length above 255 fails" \
    1074 \
    "42000" \
    "Column length too big for column 'c'" \
    "CREATE TABLE bad_length (c CHAR(256));" \
    "$DATABASE"

expect_error \
    "char negative length is syntax error" \
    1064 \
    "42000" \
    "near '-1))'" \
    "CREATE TABLE bad_negative (c CHAR(-1));" \
    "$DATABASE"

expect_error \
    "char empty length is syntax error" \
    1064 \
    "42000" \
    "near '))'" \
    "CREATE TABLE bad_empty (c CHAR());" \
    "$DATABASE"

expect_error \
    "char not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO chars(id, nn) VALUES (5, NULL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred char string defaults" \
    "CREATE TABLE deferred_char_default (c CHAR(2) DEFAULT 'x'); DROP TABLE deferred_char_default;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred character alias" \
    "CREATE TABLE deferred_character_alias (c CHARACTER(2)); DROP TABLE deferred_character_alias;" \
    "$DATABASE"
