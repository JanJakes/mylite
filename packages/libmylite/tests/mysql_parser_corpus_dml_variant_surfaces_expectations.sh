#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_dml_variants_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_dml_variant_surfaces_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

reset_tables() {
    run_mysql \
        "DROP TABLE IF EXISTS t; DROP TABLE IF EXISTS u; "\
"CREATE TABLE t (id INT PRIMARY KEY, v INT, other INT, dt DATETIME, da DATETIME); "\
"CREATE TABLE u (id INT PRIMARY KEY, v INT); "\
"INSERT INTO t VALUES "\
"(1,10,100,'2000-01-01','2000-01-01'),"\
"(2,20,200,'2000-01-02','2000-01-02'),"\
"(3,30,300,'2000-01-03','2000-01-03'); "\
"INSERT INTO u VALUES (1,100),(2,200);" \
        "$DATABASE" >/dev/null
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
    "delete low priority" \
    "1	0	2" \
    "DELETE LOW_PRIORITY FROM t WHERE id = 1; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "delete quick" \
    "1	0	2" \
    "DELETE QUICK FROM t WHERE id = 1; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "delete ignore" \
    "1	0	2" \
    "DELETE IGNORE FROM t WHERE id = 1; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "single-source multi-table delete" \
    "1	0	2,3" \
    "DELETE t FROM t WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "modifier single-source multi-table delete" \
    "1	0	2,3" \
    "DELETE LOW_PRIORITY QUICK t FROM t WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "delete using single source" \
    "1	0	2,3" \
    "DELETE FROM a USING t AS a WHERE a.id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "modifier delete using single source" \
    "1	0	2,3" \
    "DELETE LOW_PRIORITY QUICK FROM a USING t AS a WHERE a.id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "target-star multi-table delete syntax" \
    "4	0	1" \
    "DELETE t.*, u.* FROM t, u WHERE t.id = u.id; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "multi-key delete ordering" \
    "1	0	2,3" \
    "DELETE FROM t ORDER BY v, id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "update ignore comma join" \
    "2	0	1:100,2:200,3:30" \
    "UPDATE IGNORE t, u SET t.v = u.v WHERE t.id = u.id; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "update using join condition" \
    "3	0	1:100,2:200" \
    "UPDATE t LEFT JOIN u USING(id) SET t.v = u.v; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "multi-key update ordering" \
    "1	0	1:99,2:20,3:30" \
    "UPDATE t SET v = 99 ORDER BY v, id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "insert identifier values" \
    "0	NULL	0" \
    "INSERT INTO t (id, v) VALUES (id, v); "\
"SELECT id, v, @@warning_count FROM t WHERE id = 0;" \
    "$DATABASE"

reset_tables
expect_output \
    "insert set identifier value" \
    "7	70	70	0" \
    "INSERT INTO t SET id = 7, v = 70, other = v; "\
"SELECT id, v, other, @@warning_count FROM t WHERE id = 7;" \
    "$DATABASE"

reset_tables
expect_output \
    "duplicate update target-column value" \
    "0	0	1:10" \
    "INSERT INTO t VALUES (1, 20, 200, '2000-01-01', '2000-01-01') "\
"ON DUPLICATE KEY UPDATE v = v; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', v) FROM t WHERE id = 1;" \
    "$DATABASE"

reset_tables
expect_output \
    "replace compound select source" \
    "6	0	5" \
    "REPLACE INTO t (id, v) "\
"SELECT id, v FROM u UNION ALL SELECT id + 10, v FROM u; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_parser_corpus_dml_variant_surfaces_expectations: ok"
