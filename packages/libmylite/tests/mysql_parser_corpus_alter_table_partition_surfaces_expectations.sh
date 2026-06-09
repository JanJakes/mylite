#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_alter_partition_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_alter_table_partition_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "hash add and coalesce partition" \
    "3
2" \
    "USE ${DATABASE}; "\
"CREATE TABLE hash_t (id INT) PARTITION BY HASH (id) PARTITIONS 2; "\
"ALTER TABLE hash_t ADD PARTITION PARTITIONS 1; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='hash_t'; "\
"ALTER TABLE hash_t COALESCE PARTITION 1; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='hash_t';"

expect_output \
    "range add and drop partition" \
    "2
1" \
    "USE ${DATABASE}; "\
"CREATE TABLE range_t (id INT) PARTITION BY RANGE (id) "\
"(PARTITION p0 VALUES LESS THAN (10)); "\
"ALTER TABLE range_t ADD PARTITION (PARTITION p1 VALUES LESS THAN (20)); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='range_t'; "\
"ALTER TABLE range_t DROP PARTITION p0; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='range_t';"

expect_output \
    "reorganize partition" \
    "3" \
    "USE ${DATABASE}; "\
"CREATE TABLE reorg_t (id INT) PARTITION BY RANGE (id) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION pmax VALUES LESS THAN MAXVALUE); "\
"ALTER TABLE reorg_t REORGANIZE PARTITION pmax INTO "\
"(PARTITION p1 VALUES LESS THAN (20), PARTITION pmax VALUES LESS THAN MAXVALUE); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reorg_t';"

expect_output \
    "remove partitioning" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE remove_t (id INT) PARTITION BY HASH (id) PARTITIONS 2; "\
"ALTER TABLE remove_t REMOVE PARTITIONING; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='remove_t';"

expect_output \
    "partition by alter" \
    "2" \
    "USE ${DATABASE}; "\
"CREATE TABLE partby_t (id INT); "\
"ALTER TABLE partby_t PARTITION BY HASH (id) PARTITIONS 2; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='partby_t';"

run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE maint_t (id INT) PARTITION BY RANGE (id) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE); "\
"ALTER TABLE maint_t REBUILD PARTITION p0; "\
"ALTER TABLE maint_t ANALYZE PARTITION p0; "\
"ALTER TABLE maint_t CHECK PARTITION p0; "\
"ALTER TABLE maint_t OPTIMIZE PARTITION p0; "\
"ALTER TABLE maint_t REPAIR PARTITION p0;" >/dev/null

expect_output \
    "exchange and truncate partition" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE ex_t (id INT) PARTITION BY RANGE (id) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE); "\
"CREATE TABLE staging (id INT); "\
"ALTER TABLE ex_t EXCHANGE PARTITION p0 WITH TABLE staging WITHOUT VALIDATION; "\
"INSERT INTO ex_t VALUES (1), (11); "\
"ALTER TABLE ex_t TRUNCATE PARTITION p0; "\
"SELECT COUNT(*) FROM ex_t;"

cleanup

printf '%s\n' "mysql_parser_corpus_alter_table_partition_surfaces_expectations: ok"
