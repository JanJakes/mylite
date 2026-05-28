#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_indexes_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_indexes_expectations: $1" >&2
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
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE idx_sample(
       id INT NOT NULL,
       a INT NOT NULL,
       b INT NOT NULL,
       body TEXT,
       p POINT NOT NULL,
       PRIMARY KEY(id),
       UNIQUE KEY uq_ab(a,b),
       KEY ix_b_desc(b DESC),
       FULLTEXT KEY ft_body(body),
       SPATIAL KEY sp_p(p)
     ) ENGINE=InnoDB;
     CREATE TABLE unique_clustered(
       a INT NOT NULL,
       b INT NOT NULL,
       UNIQUE KEY uq_a(a),
       KEY ix_b(b)
     ) ENGINE=InnoDB;
     CREATE TABLE generated_clustered(
       a INT,
       b INT,
       UNIQUE KEY uq_a(a),
       KEY ix_b(b)
     ) ENGINE=InnoDB;
     CREATE TABLE no_index(
       a INT,
       b INT
     ) ENGINE=InnoDB;" >/dev/null

table_kinds=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA "\
"IN ('INNODB_INDEXES', 'INNODB_FIELDS');")
expect_value "innodb index table kinds" \
    "INNODB_FIELDS	SYSTEM VIEW
INNODB_INDEXES	SYSTEM VIEW" \
    "$table_kinds"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_INDEXES', 'INNODB_FIELDS') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb index system table rows" \
    "INNODB_FIELDS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
INNODB_INDEXES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_FIELDS	INDEX_ID	1	NULL	YES	varbinary	256	256	NULL	NULL	NULL	NULL	NULL	varbinary(256)	select
INNODB_FIELDS	NAME	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
INNODB_FIELDS	POS	3	0	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned	select
INNODB_INDEXES	INDEX_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_INDEXES	NAME	2		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_INDEXES	TABLE_ID	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_INDEXES	TYPE	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_INDEXES	N_FIELDS	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_INDEXES	PAGE_NO	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_INDEXES	SPACE	7		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_INDEXES	MERGE_THRESHOLD	8		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_INDEXES', 'INNODB_FIELDS') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb index columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_index_rows="ft_body	32
ix_b_desc	0
PRIMARY	3
sp_p	64
uq_ab	2"
index_rows=$(run_mysql \
    "SELECT i.NAME, i.TYPE FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p') "\
"ORDER BY i.NAME;")
expect_value "innodb index rows" "$expected_index_rows" "$index_rows"

expected_field_rows="ft_body	body	0
ix_b_desc	b	0
PRIMARY	id	0
sp_p	p	0
uq_ab	a	0
uq_ab	b	1"
field_rows=$(run_mysql \
    "SELECT i.NAME, f.NAME, f.POS FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_FIELDS f ON f.INDEX_ID = i.INDEX_ID "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p') "\
"ORDER BY i.NAME, f.POS;")
expect_value "innodb field rows" "$expected_field_rows" "$field_rows"

counts=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p'); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FIELDS f "\
"JOIN INFORMATION_SCHEMA.INNODB_INDEXES i ON i.INDEX_ID = f.INDEX_ID "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p');")
expect_value "innodb index counts" "5
6" "$counts"

expected_clustered_rows="generated_clustered	GEN_CLUST_INDEX	1	5
generated_clustered	ix_b	0	2
generated_clustered	uq_a	2	2
no_index	GEN_CLUST_INDEX	1	5
unique_clustered	ix_b	0	2
unique_clustered	uq_a	3	4"
clustered_rows=$(run_mysql \
    "SELECT SUBSTRING_INDEX(t.NAME, '/', -1), i.NAME, i.TYPE, i.N_FIELDS "\
"FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME IN ('${DATABASE}/unique_clustered', "\
"'${DATABASE}/generated_clustered', '${DATABASE}/no_index') "\
"ORDER BY t.NAME, i.NAME;")
expect_value "innodb clustered fallback index rows" "$expected_clustered_rows" "$clustered_rows"

expected_clustered_field_rows="generated_clustered	ix_b	b	0
generated_clustered	uq_a	a	0
unique_clustered	ix_b	b	0
unique_clustered	uq_a	a	0"
clustered_field_rows=$(run_mysql \
    "SELECT SUBSTRING_INDEX(t.NAME, '/', -1), i.NAME, f.NAME, f.POS "\
"FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_FIELDS f ON f.INDEX_ID = i.INDEX_ID "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME IN ('${DATABASE}/unique_clustered', "\
"'${DATABASE}/generated_clustered', '${DATABASE}/no_index') "\
"ORDER BY t.NAME, i.NAME, f.POS;")
expect_value \
    "innodb clustered fallback field rows" \
    "$expected_clustered_field_rows" \
    "$clustered_field_rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_indexes i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p');")
expect_value "case-insensitive innodb indexes table name count" "5" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_INDEXES i "\
"JOIN INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p'); "\
"SELECT COUNT(*) FROM INNODB_FIELDS f "\
"JOIN INNODB_INDEXES i ON i.INDEX_ID = f.INDEX_ID "\
"JOIN INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p');")
expect_value "unqualified innodb index counts" "5
6" "$use_counts"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb index status" "0	-1" "$status"

rename_drop_rows=$(run_mysql \
    "USE ${DATABASE}; ALTER TABLE idx_sample RENAME INDEX ix_b_desc TO ix_b2; "\
"DROP INDEX uq_ab ON idx_sample; "\
"SELECT i.NAME, i.TYPE FROM INFORMATION_SCHEMA.INNODB_INDEXES i "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/idx_sample' "\
"AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ix_b2','ft_body','sp_p') "\
"ORDER BY i.NAME;")
expect_value "innodb index rows after rename drop" \
    "ft_body	32
ix_b2	0
PRIMARY	3
sp_p	64" \
    "$rename_drop_rows"

printf '%s\n' "mysql_baseline_information_schema_innodb_indexes_expectations: ok"
