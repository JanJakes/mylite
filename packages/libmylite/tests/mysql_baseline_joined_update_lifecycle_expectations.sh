#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_joined_update_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_joined_update_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

reset_tables() {
    run_mysql \
        "DROP TABLE IF EXISTS t; DROP TABLE IF EXISTS u; "\
"CREATE TABLE t (id INT, k INT, v INT, s VARCHAR(20)); "\
"CREATE TABLE u (id INT, k INT, w INT, s VARCHAR(20)); "\
"INSERT INTO t VALUES (1,10,0,'a'),(2,20,0,'b'),(3,30,5,'c'),(4,40,0,'d'); "\
"INSERT INTO u VALUES (1,10,100,'x'),(2,10,101,'y'),(3,30,300,'z'),(4,50,500,'q');" \
        "$DATABASE" >/dev/null
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

reset_tables
expect_output \
    "duplicate joined matches update target row once" \
    "2	0	1:7,2:0,3:7,4:0" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 7; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"
expect_output \
    "repeated joined update reports changed rows" \
    "0	0" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 7; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

reset_tables
expect_output \
    "joined update where filters right source" \
    "1	0	1:0,2:0,3:9,4:0" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 9 WHERE u.w > 200; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "left joined update unmatched target rows" \
    "2	0	1:0,2:8,3:5,4:8" \
    "UPDATE t LEFT JOIN u ON t.k = u.k SET t.v = 8 WHERE u.id IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "inner join without on uses where filter" \
    "4	0	1:6,2:6,3:6,4:6" \
    "UPDATE t JOIN u SET t.v = 6 WHERE u.id = 4; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "alias-qualified assignment target" \
    "1	0	1:11,2:0,3:5,4:0" \
    "UPDATE t AS a JOIN u AS b ON a.k = b.k SET a.v = 11 WHERE b.w = 100; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "bare alias assignment target" \
    "1	0	1:19,2:0,3:5,4:0" \
    "UPDATE t a JOIN u b ON a.k = b.k SET a.v = 19 WHERE b.w = 100; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "derived joined update with multiple target assignments" \
    "2	0	1:31:updated,2:31:updated,3:5:c,4:0:d" \
    "UPDATE t a JOIN (SELECT id FROM t WHERE k IS NOT NULL ORDER BY id LIMIT 2 FOR UPDATE) picked "\
"ON a.id = picked.id SET a.v = 31, a.s = 'updated'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', v, ':', s) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "unqualified unique assignment target" \
    "1	0	1:13,2:0,3:5,4:0" \
    "UPDATE t AS a JOIN u AS b ON a.k = b.k SET v = 13 WHERE b.w = 101; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "right source assignment target" \
    "2	0	1:777,2:777,3:300,4:500" \
    "UPDATE t JOIN u ON t.k = u.k SET u.w = 777 WHERE t.id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', w) ORDER BY id) FROM u;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE IF EXISTS left_no_target; DROP TABLE IF EXISTS right_no_target; "\
"CREATE TABLE left_no_target (id INT, k INT); "\
"CREATE TABLE right_no_target (id INT, k INT, w INT NOT NULL); "\
"INSERT INTO left_no_target VALUES (1,123);" \
    "$DATABASE" >/dev/null
expect_output \
    "right source left join without target rows skips assignment conversion" \
    "0	0	0" \
    "UPDATE left_no_target LEFT JOIN right_no_target "\
"ON left_no_target.k = right_no_target.k SET right_no_target.w = NULL; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM right_no_target;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_t (id INT, k INT, v INT); "\
"CREATE TABLE ${DATABASE}.qualified_u (id INT, k INT); "\
"INSERT INTO ${DATABASE}.qualified_t VALUES (1,10,0),(2,20,0),(3,30,0); "\
"INSERT INTO ${DATABASE}.qualified_u VALUES (9,10),(10,30);" >/dev/null
expect_output \
    "schema-qualified target without selected schema" \
    "1	0	1:0,2:0,3:21" \
    "UPDATE ${DATABASE}.qualified_t JOIN ${DATABASE}.qualified_u "\
"ON qualified_t.k = qualified_u.k SET qualified_t.v = 21 WHERE qualified_u.id = 10; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) "\
"FROM ${DATABASE}.qualified_t;"
expect_output \
    "schema-qualified alias target without selected schema" \
    "1	0	1:0,2:0,3:22" \
    "UPDATE ${DATABASE}.qualified_t AS a JOIN ${DATABASE}.qualified_u AS b "\
"ON a.k = b.k SET a.v = 22 WHERE b.id = 10; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) "\
"FROM ${DATABASE}.qualified_t;"

reset_tables
expect_error \
    "table name assignment rejected when alias declared" \
    1054 \
    42S22 \
    "Unknown column 't.v' in 'field list'" \
    "UPDATE t AS a JOIN u AS b ON a.k = b.k SET t.v = 12;" \
    "$DATABASE"

reset_tables
expect_error \
    "ambiguous assignment column" \
    1052 \
    23000 \
    "Column 's' in field list is ambiguous" \
    "UPDATE t JOIN u ON t.k = u.k SET s = 'amb';" \
    "$DATABASE"

reset_tables
expect_error \
    "unknown on column" \
    1054 \
    42S22 \
    "Unknown column 't.nope' in 'on clause'" \
    "UPDATE t JOIN u ON t.nope = u.k SET t.v = 1;" \
    "$DATABASE"

reset_tables
expect_error \
    "unknown assignment column" \
    1054 \
    42S22 \
    "Unknown column 't.nope' in 'field list'" \
    "UPDATE t JOIN u ON t.k = u.k SET t.nope = 1;" \
    "$DATABASE"

reset_tables
expect_error \
    "ambiguous where column" \
    1052 \
    23000 \
    "Column 'k' in where clause is ambiguous" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 1 WHERE k = 10;" \
    "$DATABASE"

reset_tables
expect_error \
    "joined update order by wrong usage" \
    1221 \
    HY000 \
    "Incorrect usage of UPDATE and ORDER BY" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 1 ORDER BY t.id;" \
    "$DATABASE"

reset_tables
expect_error \
    "joined update limit wrong usage" \
    1221 \
    HY000 \
    "Incorrect usage of UPDATE and LIMIT" \
    "UPDATE t JOIN u ON t.k = u.k SET t.v = 1 LIMIT 1;" \
    "$DATABASE"
