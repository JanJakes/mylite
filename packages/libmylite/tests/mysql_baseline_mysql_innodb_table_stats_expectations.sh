#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_mysql_innodb_table_stats_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_mysql_innodb_table_stats_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE t_primary(
       id INT PRIMARY KEY,
       v INT,
       KEY ix_v(v),
       KEY ix_v_id(v,id)
     ) ENGINE=InnoDB;
     CREATE TABLE t_plain(
       a INT,
       b INT
     ) ENGINE=InnoDB;
     INSERT INTO t_primary VALUES (1,10),(2,20),(3,20);
     INSERT INTO t_plain VALUES (1,2),(3,4);
     ANALYZE TABLE t_primary, t_plain;" >/dev/null

show_full_expected="innodb_table_stats	BASE TABLE"
expect_output \
    "show full tables mysql innodb table stats" \
    "$show_full_expected" \
    "SHOW FULL TABLES FROM mysql LIKE 'innodb_table_stats';"

table_row_expected="innodb_table_stats	BASE TABLE	InnoDB	10	Dynamic	16384	utf8mb3_bin	row_format=DYNAMIC stats_persistent=0"
expect_output \
    "information schema tables row" \
    "$table_row_expected" \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,DATA_LENGTH,
            TABLE_COLLATION,CREATE_OPTIONS
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME = 'innodb_table_stats';"

columns_expected=$(
    printf '%b' \
        'database_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'table_name\tvarchar(199)\tNO\tPRI\tNULL\t\n' \
        'last_update\ttimestamp\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n' \
        'n_rows\tbigint unsigned\tNO\t\tNULL\t\n' \
        'clustered_index_size\tbigint unsigned\tNO\t\tNULL\t\n' \
        'sum_of_other_index_sizes\tbigint unsigned\tNO\t\tNULL\t'
)
expect_output \
    "show columns shape" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.innodb_table_stats;"

information_schema_columns_expected="mysql	innodb_table_stats	database_name	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	PRI		select,insert,update,references
mysql	innodb_table_stats	table_name	2	NULL	NO	varchar	199	597	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(199)	PRI		select,insert,update,references
mysql	innodb_table_stats	last_update	3	CURRENT_TIMESTAMP	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp		DEFAULT_GENERATED on update CURRENT_TIMESTAMP	select,insert,update,references
mysql	innodb_table_stats	n_rows	4	NULL	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select,insert,update,references
mysql	innodb_table_stats	clustered_index_size	5	NULL	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select,insert,update,references
mysql	innodb_table_stats	sum_of_other_index_sizes	6	NULL	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select,insert,update,references"
expect_output \
    "information schema columns metadata" \
    "$information_schema_columns_expected" \
    "SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,
            IS_NULLABLE,DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,
            COLLATION_NAME,COLUMN_TYPE,COLUMN_KEY,EXTRA,PRIVILEGES
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME = 'innodb_table_stats'
      ORDER BY ORDINAL_POSITION;"

stats_expected="${DATABASE}	t_plain	2	1	0
${DATABASE}	t_primary	3	1	2"
expect_output \
    "descriptor statistics rows" \
    "$stats_expected" \
    "SELECT database_name, table_name, n_rows, clustered_index_size,
            sum_of_other_index_sizes
       FROM mysql.innodb_table_stats
      WHERE database_name = '${DATABASE}'
      ORDER BY table_name;"

expect_output \
    "last update not null" \
    "2" \
    "SELECT COUNT(*)
       FROM mysql.innodb_table_stats
      WHERE database_name = '${DATABASE}'
        AND last_update IS NOT NULL;"

expect_output \
    "last update ignores SET timestamp" \
    "0" \
    "SET time_zone = '+00:00';
     SET timestamp = 1700000000;
     SELECT COUNT(*)
       FROM mysql.innodb_table_stats
      WHERE database_name = '${DATABASE}'
        AND last_update = '2023-11-14 22:13:20';"

expect_output \
    "unqualified selected mysql schema read" \
    "2" \
    "USE mysql;
     SELECT COUNT(*)
       FROM innodb_table_stats
      WHERE database_name = '${DATABASE}';"

status=$(run_mysql \
    "SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE database_name = '${DATABASE}';
     SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "read status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_mysql_innodb_table_stats_expectations: ok"
