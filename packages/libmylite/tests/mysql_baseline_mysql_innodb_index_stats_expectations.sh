#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_mysql_innodb_index_stats_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_mysql_innodb_index_stats_expectations: $1" >&2
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
       w INT,
       KEY ix_v(v),
       KEY ix_v_id(v,id),
       UNIQUE KEY uq_w(w)
     ) ENGINE=InnoDB;
     CREATE TABLE t_generated(
       a INT,
       b INT,
       KEY ix_b(b)
     ) ENGINE=InnoDB;
     CREATE TABLE t_prefix(
       id INT PRIMARY KEY,
       name VARCHAR(10),
       raw VARBINARY(10),
       KEY ix_name(name(1)),
       KEY ix_raw(raw(1))
     ) ENGINE=InnoDB;
     INSERT INTO t_primary VALUES (1,10,100),(2,20,200),(3,20,300);
     INSERT INTO t_generated VALUES (1,10),(2,10),(3,20);
     INSERT INTO t_prefix VALUES
       (1,'aa',X'0101'),(2,'ab',X'0102'),(3,'ba',X'0201'),(4,'bb',X'0202');
     ANALYZE TABLE t_primary, t_generated, t_prefix;" >/dev/null

show_full_expected="innodb_index_stats	BASE TABLE"
expect_output \
    "show full tables mysql innodb index stats" \
    "$show_full_expected" \
    "SHOW FULL TABLES FROM mysql LIKE 'innodb_index_stats';"

table_row_expected="innodb_index_stats	BASE TABLE	InnoDB	10	Dynamic	utf8mb3_bin	row_format=DYNAMIC stats_persistent=0"
expect_output \
    "information schema tables row" \
    "$table_row_expected" \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,
            TABLE_COLLATION,CREATE_OPTIONS
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME = 'innodb_index_stats';"

columns_expected=$(
    printf '%b' \
        'database_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'table_name\tvarchar(199)\tNO\tPRI\tNULL\t\n' \
        'index_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'last_update\ttimestamp\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n' \
        'stat_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'stat_value\tbigint unsigned\tNO\t\tNULL\t\n' \
        'sample_size\tbigint unsigned\tYES\t\tNULL\t\n' \
        'stat_description\tvarchar(1024)\tNO\t\tNULL\t'
)
expect_output \
    "show columns shape" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.innodb_index_stats;"

information_schema_columns_expected="mysql	innodb_index_stats	database_name	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	PRI		select,insert,update,references
mysql	innodb_index_stats	table_name	2	NULL	NO	varchar	199	597	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(199)	PRI		select,insert,update,references
mysql	innodb_index_stats	index_name	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	PRI		select,insert,update,references
mysql	innodb_index_stats	last_update	4	CURRENT_TIMESTAMP	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp		DEFAULT_GENERATED on update CURRENT_TIMESTAMP	select,insert,update,references
mysql	innodb_index_stats	stat_name	5	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	PRI		select,insert,update,references
mysql	innodb_index_stats	stat_value	6	NULL	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select,insert,update,references
mysql	innodb_index_stats	sample_size	7	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select,insert,update,references
mysql	innodb_index_stats	stat_description	8	NULL	NO	varchar	1024	3072	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(1024)			select,insert,update,references"
expect_output \
    "information schema columns metadata" \
    "$information_schema_columns_expected" \
    "SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,
            IS_NULLABLE,DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,
            COLLATION_NAME,COLUMN_TYPE,COLUMN_KEY,EXTRA,PRIVILEGES
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME = 'innodb_index_stats'
      ORDER BY ORDINAL_POSITION;"

builtin_expected="mysql	component	PRIMARY	n_diff_pfx01	0	1	component_id
mysql	component	PRIMARY	n_leaf_pages	1	NULL	Number of leaf pages in the index
mysql	component	PRIMARY	size	1	NULL	Number of pages in the index
sys	sys_config	PRIMARY	n_diff_pfx01	6	1	variable
sys	sys_config	PRIMARY	n_leaf_pages	1	NULL	Number of leaf pages in the index
sys	sys_config	PRIMARY	size	1	NULL	Number of pages in the index"
expect_output \
    "built-in statistics rows" \
    "$builtin_expected" \
    "SELECT database_name, table_name, index_name, stat_name, stat_value,
            sample_size, stat_description
       FROM mysql.innodb_index_stats
      WHERE database_name IN ('mysql','sys')
      ORDER BY database_name, table_name, index_name, stat_name;"

stats_expected="${DATABASE}	t_generated	GEN_CLUST_INDEX	n_diff_pfx01	3	1	DB_ROW_ID
${DATABASE}	t_generated	GEN_CLUST_INDEX	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_generated	GEN_CLUST_INDEX	size	1	NULL	Number of pages in the index
${DATABASE}	t_generated	ix_b	n_diff_pfx01	2	1	b
${DATABASE}	t_generated	ix_b	n_diff_pfx02	3	1	b,DB_ROW_ID
${DATABASE}	t_generated	ix_b	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_generated	ix_b	size	1	NULL	Number of pages in the index
${DATABASE}	t_primary	PRIMARY	n_diff_pfx01	3	1	id
${DATABASE}	t_primary	PRIMARY	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_primary	PRIMARY	size	1	NULL	Number of pages in the index
${DATABASE}	t_primary	ix_v	n_diff_pfx01	2	1	v
${DATABASE}	t_primary	ix_v	n_diff_pfx02	3	1	v,id
${DATABASE}	t_primary	ix_v	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_primary	ix_v	size	1	NULL	Number of pages in the index
${DATABASE}	t_primary	ix_v_id	n_diff_pfx01	2	1	v
${DATABASE}	t_primary	ix_v_id	n_diff_pfx02	3	1	v,id
${DATABASE}	t_primary	ix_v_id	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_primary	ix_v_id	size	1	NULL	Number of pages in the index
${DATABASE}	t_primary	uq_w	n_diff_pfx01	3	1	w
${DATABASE}	t_primary	uq_w	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_primary	uq_w	size	1	NULL	Number of pages in the index"
expect_output \
    "descriptor statistics rows" \
    "$stats_expected" \
    "SELECT database_name, table_name, index_name, stat_name, stat_value,
            sample_size, stat_description
       FROM mysql.innodb_index_stats
      WHERE database_name = '${DATABASE}'
        AND table_name IN ('t_generated','t_primary')
      ORDER BY table_name, index_name, stat_name;"

prefix_stats_expected="${DATABASE}	t_prefix	PRIMARY	n_diff_pfx01	4	1	id
${DATABASE}	t_prefix	PRIMARY	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_prefix	PRIMARY	size	1	NULL	Number of pages in the index
${DATABASE}	t_prefix	ix_name	n_diff_pfx01	2	1	name
${DATABASE}	t_prefix	ix_name	n_diff_pfx02	4	1	name,id
${DATABASE}	t_prefix	ix_name	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_prefix	ix_name	size	1	NULL	Number of pages in the index
${DATABASE}	t_prefix	ix_raw	n_diff_pfx01	2	1	raw
${DATABASE}	t_prefix	ix_raw	n_diff_pfx02	4	1	raw,id
${DATABASE}	t_prefix	ix_raw	n_leaf_pages	1	NULL	Number of leaf pages in the index
${DATABASE}	t_prefix	ix_raw	size	1	NULL	Number of pages in the index"
expect_output \
    "prefix statistics rows" \
    "$prefix_stats_expected" \
    "SELECT database_name, table_name, index_name, stat_name, stat_value,
            sample_size, stat_description
       FROM mysql.innodb_index_stats
      WHERE database_name = '${DATABASE}'
        AND table_name = 't_prefix'
      ORDER BY table_name, index_name, stat_name;"

expect_output \
    "last update not null" \
    "32" \
    "SELECT COUNT(*)
       FROM mysql.innodb_index_stats
      WHERE database_name = '${DATABASE}'
        AND last_update IS NOT NULL;"

expect_output \
    "last update ignores SET timestamp" \
    "0" \
    "SET time_zone = '+00:00';
     SET timestamp = 1700000000;
     SELECT COUNT(*)
       FROM mysql.innodb_index_stats
      WHERE database_name = '${DATABASE}'
        AND last_update = '2023-11-14 22:13:20';"

expect_output \
    "unqualified selected mysql schema read" \
    "32" \
    "USE mysql;
     SELECT COUNT(*)
       FROM innodb_index_stats
      WHERE database_name = '${DATABASE}';"

status=$(run_mysql \
    "SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE database_name = '${DATABASE}';
     SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "read status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_mysql_innodb_index_stats_expectations: ok"
