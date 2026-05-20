#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_nonstrict_string_truncation_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_nonstrict_string_truncation_expectations: $1" >&2
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

strict_insert_error_sql="SET NAMES utf8mb4; CREATE TABLE strict_i (id INT, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO strict_i VALUES (1, 'abcd', 'xyz');"
expect_error \
    "strict varchar overlength rejects insert" \
    1406 \
    "22001" \
    "Data too long for column 'v' at row 1" \
    "$strict_insert_error_sql" \
    "$DATABASE"

strict_update_error_sql="SET NAMES utf8mb4; CREATE TABLE strict_u (id INT, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO strict_u VALUES (1, 'abc', 'xyz'); UPDATE strict_u SET v = 'abcd' WHERE id = 1;"
expect_error \
    "strict varchar overlength rejects update" \
    1406 \
    "22001" \
    "Data too long for column 'v' at row 1" \
    "$strict_update_error_sql" \
    "$DATABASE"

nonstrict_insert_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2	0
1	abc	3	wxy	3
EXPECTED
)
expect_output \
    "non-strict insert truncates char and varchar" \
    "$nonstrict_insert_expected" \
    "SET NAMES utf8mb4; CREATE TABLE ni (id INT, v VARCHAR(3), c CHAR(3)); "\
"SET sql_mode=''; INSERT INTO ni VALUES (1, 'abcd', 'wxyz'); SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, v, CHAR_LENGTH(v), c, CHAR_LENGTH(c) FROM ni;" \
    "$DATABASE"

nonstrict_insert_set_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2
1	abc	pqr
EXPECTED
)
expect_output \
    "non-strict insert set truncates char and varchar" \
    "$nonstrict_insert_set_expected" \
    "SET NAMES utf8mb4; CREATE TABLE nis (id INT, v VARCHAR(3), c CHAR(3)); "\
"SET sql_mode=''; INSERT INTO nis SET id = 1, v = 'abcdef', c = 'pqrs'; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM nis;" \
    "$DATABASE"

nonstrict_replace_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2
1	abc	uvw
EXPECTED
)
expect_output \
    "non-strict replace set duplicate truncates char and varchar" \
    "$nonstrict_replace_expected" \
    "SET NAMES utf8mb4; CREATE TABLE nr (id INT PRIMARY KEY, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO nr VALUES (1, 'old', 'old'); SET sql_mode=''; "\
"REPLACE INTO nr SET id = 1, v = 'abcdef', c = 'uvwxy'; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM nr;" \
    "$DATABASE"

insert_ignore_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2
1	abc	wxy
EXPECTED
)
expect_output \
    "insert ignore truncates char and varchar under strict mode" \
    "$insert_ignore_expected" \
    "SET NAMES utf8mb4; CREATE TABLE ii (id INT, v VARCHAR(3), c CHAR(3)); "\
"SET sql_mode='STRICT_TRANS_TABLES'; INSERT IGNORE INTO ii VALUES (1, 'abcd', 'wxyz'); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM ii;" \
    "$DATABASE"

trailing_spaces_expected=$(cat <<\EXPECTED
Note	1265	Data truncated for column 'v' at row 1
-1	1
abc	3	xyz	3
EXPECTED
)
expect_output \
    "strict trailing-space overflow truncates varchar with note and char silently" \
    "$trailing_spaces_expected" \
    "SET NAMES utf8mb4; CREATE TABLE ts (id INT, v VARCHAR(3), c CHAR(3)); "\
"SET sql_mode='STRICT_TRANS_TABLES'; INSERT INTO ts VALUES (1, 'abc ', 'xyz '); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT v, CHAR_LENGTH(v), c, CHAR_LENGTH(c) FROM ts;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
Warning	1265	Data truncated for column 'v' at row 2
Warning	1265	Data truncated for column 'c' at row 2
-1	4
1	abc	pqr
2	abc	pqr
EXPECTED
)
expect_output \
    "non-strict update truncates matched rows" \
    "$update_expected" \
    "SET NAMES utf8mb4; CREATE TABLE upd (id INT, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO upd VALUES (1, 'old', 'old'), (2, 'two', 'two'); SET sql_mode=''; "\
"UPDATE upd SET v = 'abcdef', c = 'pqrs' WHERE id IN (1,2) ORDER BY id; "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM upd ORDER BY id;" \
    "$DATABASE"

update_same_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2
1	abc	pqr
EXPECTED
)
expect_output \
    "non-strict update truncation to current value keeps zero affected rows" \
    "$update_same_expected" \
    "SET NAMES utf8mb4; CREATE TABLE same_u (id INT, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO same_u VALUES (1, 'abc', 'pqr'); SET sql_mode=''; "\
"UPDATE same_u SET v = 'abcd', c = 'pqrs' WHERE id = 1; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM same_u;" \
    "$DATABASE"

update_limit_zero_expected=$(cat <<\EXPECTED
0	0
1	abc	pqr
EXPECTED
)
expect_output \
    "update limit zero records no truncation warnings" \
    "$update_limit_zero_expected" \
    "SET NAMES utf8mb4; CREATE TABLE lim0 (id INT, v VARCHAR(3), c CHAR(3)); "\
"INSERT INTO lim0 VALUES (1, 'abc', 'pqr'); SET sql_mode=''; "\
"UPDATE lim0 SET v = 'abcd', c = 'pqrs' WHERE id = 1 LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id, v, c FROM lim0;" \
    "$DATABASE"

zero_length_expected=$(cat <<\EXPECTED
Note	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'v' at row 2
Warning	1265	Data truncated for column 'c' at row 2
-1	3
1	0	0
2	0	0
EXPECTED
)
expect_output \
    "zero-length char and varchar truncate according to excess content" \
    "$zero_length_expected" \
    "SET NAMES utf8mb4; CREATE TABLE zl (id INT, v VARCHAR(0), c CHAR(0)); "\
"SET sql_mode=''; INSERT INTO zl VALUES (1, '   ', '   '), (2, 'a', 'b'); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, CHAR_LENGTH(v), CHAR_LENGTH(c) FROM zl ORDER BY id;" \
    "$DATABASE"

utf8_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
-1	2
2	4	2	4
EXPECTED
)
expect_output \
    "utf8 truncation preserves character boundaries" \
    "$utf8_expected" \
    "SET NAMES utf8mb4; CREATE TABLE utf8_t (id INT, v VARCHAR(2), c CHAR(2)); "\
"SET sql_mode=''; INSERT INTO utf8_t VALUES (1, _utf8mb4 0xC3A9C3A561, _utf8mb4 0xC3A9C3A561); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT CHAR_LENGTH(v), LENGTH(v), CHAR_LENGTH(c), LENGTH(c) FROM utf8_t;" \
    "$DATABASE"
