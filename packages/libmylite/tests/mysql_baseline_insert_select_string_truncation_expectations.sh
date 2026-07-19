#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_insert_select_string_truncation_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_string_truncation_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
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

run_mysql \
    "CREATE TABLE src_v(id INT, v VARCHAR(8)); "\
"CREATE TABLE src_c(id INT, c CHAR(8)); "\
"CREATE TABLE src_t(id INT, t TEXT); "\
"INSERT INTO src_v VALUES (1,'abcd'),(2,'éééx'),(3,'ab  '); "\
"INSERT INTO src_c VALUES (1,'abcd'),(2,'éééx'),(3,'ab  '); "\
"INSERT INTO src_t VALUES (1,'abcd'),(2,'ab  '); "\
"CREATE TABLE dst_v(id INT, v VARCHAR(3)); "\
"CREATE TABLE dst_c(id INT, c CHAR(3)); "\
"CREATE TABLE dst_pair(id INT, v VARCHAR(3), c CHAR(3));" \
    "$DATABASE" >/dev/null

expect_error \
    "strict row-scalar varchar overflow" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_v SELECT 1, 'abcd';" \
    "$DATABASE"

expect_error \
    "strict table-backed varchar overflow" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_v SELECT id, v FROM src_v WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "strict table-backed char to varchar overflow" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_v SELECT id, c FROM src_c WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "strict table-backed text to varchar overflow" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_v SELECT id, t FROM src_t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "strict compound table-backed varchar overflow" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_v SELECT id, v FROM src_v WHERE id = 1 "\
"UNION ALL SELECT id, v FROM src_v WHERE id = 99;" \
    "$DATABASE"

expect_error \
    "strict row-scalar char overflow" \
    1406 \
    22001 \
    "Data too long for column 'c' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_c SELECT 1, 'abcd';" \
    "$DATABASE"

expect_error \
    "strict table-backed char overflow" \
    1406 \
    22001 \
    "Data too long for column 'c' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"INSERT INTO dst_c SELECT id, c FROM src_c WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "strict table-backed text trailing space behavior" \
    "Note	1265	Data truncated for column 'v' at row 1
status	1	1	0
row	2	[ab ]	3	3" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE dst_v; "\
"INSERT INTO dst_v SELECT id, t FROM src_t WHERE id = 2; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, CONCAT('[', v, ']'), LENGTH(v), CHAR_LENGTH(v) FROM dst_v;" \
    "$DATABASE"

expect_output \
    "strict row-scalar trailing space behavior" \
    "Note	1265	Data truncated for column 'v' at row 1
status_v	1	1	0
row_v	1	[ab ]	3	3
status_c	1	0	0
row_c	1	[ab]	2	2" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE dst_v; "\
"TRUNCATE dst_c; "\
"INSERT INTO dst_v SELECT 1, 'ab  '; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status_v', @rc, @cond, @@error_count; "\
"SELECT 'row_v', id, CONCAT('[', v, ']'), LENGTH(v), CHAR_LENGTH(v) FROM dst_v; "\
"INSERT INTO dst_c SELECT 1, 'ab  '; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SELECT 'status_c', @rc, @cond, @@error_count; "\
"SELECT 'row_c', id, CONCAT('[', c, ']'), LENGTH(c), CHAR_LENGTH(c) FROM dst_c;" \
    "$DATABASE"

expect_output \
    "nonstrict selected string truncation" \
    "Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
Warning	1265	Data truncated for column 'v' at row 2
Warning	1265	Data truncated for column 'c' at row 2
status	2	4	0
row	1	[abc]	[abc]
row	2	[ééé]	[ééé]" \
    "SET sql_mode=''; "\
"TRUNCATE dst_pair; "\
"INSERT INTO dst_pair SELECT src_v.id, src_v.v, src_c.c "\
"FROM src_v JOIN src_c ON src_c.id = src_v.id WHERE src_v.id IN (1,2) ORDER BY src_v.id; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, CONCAT('[', v, ']'), CONCAT('[', c, ']') FROM dst_pair ORDER BY id;" \
    "$DATABASE"

expect_output \
    "nonstrict row-scalar selected string truncation" \
    "Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
status	1	2	0
row	1	[abc]	[abc]" \
    "SET sql_mode=''; "\
"TRUNCATE dst_pair; "\
"INSERT INTO dst_pair SELECT 1, 'abcd', 'abcd'; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, CONCAT('[', v, ']'), CONCAT('[', c, ']') FROM dst_pair;" \
    "$DATABASE"

expect_output \
    "strict insert ignore selected string truncation" \
    "Warning	1265	Data truncated for column 'v' at row 1
Warning	1265	Data truncated for column 'c' at row 1
status	1	2	0
row	1	[abc]	[abc]" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE dst_pair; "\
"INSERT IGNORE INTO dst_pair SELECT src_v.id, src_v.v, src_c.c "\
"FROM src_v JOIN src_c ON src_c.id = src_v.id WHERE src_v.id = 1; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'row', id, CONCAT('[', v, ']'), CONCAT('[', c, ']') FROM dst_pair;" \
    "$DATABASE"

expect_output \
    "zero-row source has no selected string diagnostics" \
    "status	0	0	0
rows	0" \
    "SET sql_mode=''; "\
"TRUNCATE dst_pair; "\
"INSERT INTO dst_pair SELECT src_v.id, src_v.v, src_c.c "\
"FROM src_v JOIN src_c ON src_c.id = src_v.id WHERE src_v.id > 99; "\
"GET DIAGNOSTICS @rc = ROW_COUNT, @cond = NUMBER; "\
"SHOW WARNINGS; "\
"SELECT 'status', @rc, @cond, @@error_count; "\
"SELECT 'rows', COUNT(*) FROM dst_pair;" \
    "$DATABASE"

cleanup
