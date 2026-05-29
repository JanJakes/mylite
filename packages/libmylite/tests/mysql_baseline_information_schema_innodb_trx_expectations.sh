#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_trx_expectations_$$"
BACKGROUND_PID=""
BACKGROUND_OUT="/tmp/mylite_innodb_trx_expectations_$$.out"
BACKGROUND_ERR="/tmp/mylite_innodb_trx_expectations_$$.err"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_trx_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

cleanup() {
    if [ -n "$BACKGROUND_PID" ]; then
        kill "$BACKGROUND_PID" >/dev/null 2>&1 || true
        wait "$BACKGROUND_PID" >/dev/null 2>&1 || true
    fi
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    rm -f "$BACKGROUND_OUT" "$BACKGROUND_ERR"
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_TRX';")
expect_value "innodb trx table kind" \
    "INNODB_TRX	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_TRX';")
expect_value "innodb trx system table row" \
    "INNODB_TRX	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_TRX	trx_id	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_state	2		NO	varchar	4	13	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(13)	select
INNODB_TRX	trx_started	3		NO	datetime	NULL	NULL	NULL	NULL	NULL	NULL	NULL	datetime	select
INNODB_TRX	trx_requested_lock_id	4		YES	varchar	42	126	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(126)	select
INNODB_TRX	trx_wait_started	5		YES	datetime	NULL	NULL	NULL	NULL	NULL	NULL	NULL	datetime	select
INNODB_TRX	trx_weight	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_mysql_thread_id	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_query	8		YES	varchar	341	1024	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(1024)	select
INNODB_TRX	trx_operation_state	9		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_TRX	trx_tables_in_use	10		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_tables_locked	11		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_lock_structs	12		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_lock_memory_bytes	13		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_rows_locked	14		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_rows_modified	15		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_concurrency_tickets	16		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_isolation_level	17		NO	varchar	5	16	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(16)	select
INNODB_TRX	trx_unique_checks	18		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TRX	trx_foreign_key_checks	19		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TRX	trx_last_foreign_key_error	20		YES	varchar	85	256	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
INNODB_TRX	trx_adaptive_hash_latched	21		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TRX	trx_adaptive_hash_timeout	22		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TRX	trx_is_read_only	23		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TRX	trx_autocommit_non_locking	24		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TRX	trx_schedule_weight	25		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TRX' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb trx columns metadata" "$expected_columns_metadata" "$columns_metadata"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX;")
expect_value "default innodb trx count" "0" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_trx;")
expect_value "case-insensitive innodb trx count" "0" "$case_count"

predicate_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX WHERE trx_state = 'RUNNING';")
expect_value "empty innodb trx predicate count" "0" "$predicate_count"

use_count=$(run_mysql "USE information_schema; SELECT COUNT(*) FROM INNODB_TRX;")
expect_value "unqualified innodb trx count" "0" "$use_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.INNODB_TRX; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb trx status" "0	-1" "$status"

run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE}; "\
"CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE tx_probe (id INT PRIMARY KEY, v INT) ENGINE=InnoDB; "\
"INSERT INTO tx_probe VALUES (1, 1);" >/dev/null

(
    run_mysql \
        "USE ${DATABASE}; "\
"SET autocommit=0; "\
"UPDATE tx_probe SET v = v + 1 WHERE id = 1; "\
"SELECT SLEEP(12); "\
"COMMIT;"
) >"$BACKGROUND_OUT" 2>"$BACKGROUND_ERR" &
BACKGROUND_PID=$!
sleep 2

active_row=$(run_mysql \
    "SELECT TRX_STATE,TRX_TABLES_IN_USE,TRX_TABLES_LOCKED,TRX_ROWS_LOCKED, "\
"TRX_ROWS_MODIFIED,TRX_IS_READ_ONLY,TRX_AUTOCOMMIT_NON_LOCKING, "\
"TRX_SCHEDULE_WEIGHT IS NULL "\
"FROM INFORMATION_SCHEMA.INNODB_TRX "\
"WHERE TRX_MYSQL_THREAD_ID <> CONNECTION_ID() AND TRX_QUERY = 'SELECT SLEEP(12)' "\
"ORDER BY TRX_STARTED DESC LIMIT 1;")
expect_value "active innodb trx dynamic row observation" \
    "RUNNING	0	1	1	1	0	0	1" \
    "$active_row"

if ! wait "$BACKGROUND_PID"; then
    cat "$BACKGROUND_ERR" >&2
    fail "background transaction probe failed"
fi
BACKGROUND_PID=""

printf '%s\n' "mysql_baseline_information_schema_innodb_trx_expectations: ok"
