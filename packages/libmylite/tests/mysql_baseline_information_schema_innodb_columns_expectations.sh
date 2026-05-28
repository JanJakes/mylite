#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_columns_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_columns_expectations: $1" >&2
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
     CREATE TABLE c_sample(
       id INT NOT NULL,
       nullable_int INT,
       big BIGINT UNSIGNED,
       c CHAR(4),
       v VARCHAR(10) NOT NULL,
       b BINARY(3),
       vb VARBINARY(5),
       d DECIMAL(8,2),
       f FLOAT,
       dbl DOUBLE,
       t TEXT,
       bl BLOB,
       js JSON,
       y YEAR,
       da DATE,
       ti TIME,
       dt DATETIME,
       ts TIMESTAMP NULL,
       PRIMARY KEY(id)
     ) ENGINE=InnoDB;
     CREATE TABLE numeric_sample(
       ti TINYINT,
       ti_u TINYINT UNSIGNED NOT NULL,
       si SMALLINT,
       si_u SMALLINT UNSIGNED NOT NULL,
       mi MEDIUMINT,
       mi_u MEDIUMINT UNSIGNED NOT NULL,
       i INT,
       i_u INT UNSIGNED NOT NULL,
       bi BIGINT,
       bi_u BIGINT UNSIGNED NOT NULL,
       de1 DECIMAL(5,0),
       de2 DECIMAL(18,4),
       bit1 BIT(1),
       bit9 BIT(9) NOT NULL
     ) ENGINE=InnoDB;
     CREATE TABLE string_sample(
       c_null CHAR(4),
       c_not_null CHAR(4) NOT NULL,
       vc_null VARCHAR(10),
       vc_not_null VARCHAR(10) NOT NULL,
       nchar_col NCHAR(3),
       nvarchar_col NATIONAL VARCHAR(5),
       txt TINYTEXT,
       medtxt MEDIUMTEXT,
       longtxt LONGTEXT,
       bin_null BINARY(3),
       bin_not_null BINARY(3) NOT NULL,
       vb_null VARBINARY(5),
       vb_not_null VARBINARY(5) NOT NULL,
       tinybl TINYBLOB,
       medbl MEDIUMBLOB,
       longbl LONGBLOB,
       enum_col ENUM('a','bb'),
       enum_not_null ENUM('a','bb') NOT NULL,
       set_col SET('a','bb'),
       set_not_null SET('a','bb') NOT NULL,
       js_null JSON,
       js_not_null JSON NOT NULL,
       geom_null GEOMETRY,
       geom_not_null GEOMETRY NOT NULL
     ) ENGINE=InnoDB;
     CREATE TABLE charset_sample(
       c_ascii CHAR(4) CHARACTER SET ascii,
       vc_ascii VARCHAR(10) CHARACTER SET ascii NOT NULL,
       txt_ascii TEXT CHARACTER SET ascii,
       c_u8bin CHAR(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,
       vc_u8gen VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci,
       c_bin_charset CHAR(4) CHARACTER SET binary,
       vc_bin_charset VARCHAR(10) CHARACTER SET binary,
       txt_bin_charset TEXT CHARACTER SET binary
     ) ENGINE=InnoDB;" >/dev/null

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_COLUMNS';")
expect_value "innodb columns table kind" \
    "INNODB_COLUMNS	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_COLUMNS';")
expect_value "innodb columns system table row" \
    "INNODB_COLUMNS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_COLUMNS	TABLE_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_COLUMNS	NAME	2		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_COLUMNS	POS	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_COLUMNS	MTYPE	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_COLUMNS	PRTYPE	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_COLUMNS	LEN	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_COLUMNS	HAS_DEFAULT	7		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_COLUMNS	DEFAULT_VALUE	8		YES	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	text	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_COLUMNS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb columns columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_c_rows="id	0	6	1283	4	0	1
nullable_int	1	6	1027	4	0	1
big	2	6	1544	8	0	1
c	3	13	16711934	16	0	1
v	4	12	16711951	40	0	1
b	5	3	4130046	3	0	1
vb	6	4	4129807	5	0	1
d	7	3	525558	4	0	1
f	8	9	1028	4	0	1
dbl	9	10	1029	8	0	1
t	10	5	16711932	10	0	1
bl	11	5	4130044	10	0	1
js	12	5	3015925	12	0	1
y	13	6	1549	1	0	1
da	14	6	1034	3	0	1
ti	15	3	525323	3	0	1
dt	16	3	525324	5	0	1
ts	17	3	525319	4	0	1"
c_rows=$(run_mysql \
    "SELECT c.NAME,c.POS,c.MTYPE,c.PRTYPE,c.LEN,c.HAS_DEFAULT,c.DEFAULT_VALUE IS NULL "\
"FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/c_sample' ORDER BY c.POS;")
expect_value "innodb columns c_sample rows" "$expected_c_rows" "$c_rows"

expected_numeric_rows="ti	0	6	1025	1
ti_u	1	6	1793	1
si	2	6	1026	2
si_u	3	6	1794	2
mi	4	6	1033	3
mi_u	5	6	1801	3
i	6	6	1027	4
i_u	7	6	1795	4
bi	8	6	1032	8
bi_u	9	6	1800	8
de1	10	3	525558	3
de2	11	3	525558	9
bit1	12	3	4130320	1
bit9	13	3	4130576	2"
numeric_rows=$(run_mysql \
    "SELECT c.NAME,c.POS,c.MTYPE,c.PRTYPE,c.LEN "\
"FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/numeric_sample' ORDER BY c.POS;")
expect_value "innodb columns numeric rows" "$expected_numeric_rows" "$numeric_rows"

expected_string_rows="c_null	0	13	16711934	16
c_not_null	1	13	16712190	16
vc_null	2	12	16711695	40
vc_not_null	3	12	16711951	40
nchar_col	4	13	2162942	9
nvarchar_col	5	12	2162703	15
txt	6	5	16711932	9
medtxt	7	5	16711932	11
longtxt	8	5	16711932	12
bin_null	9	3	4130046	3
bin_not_null	10	3	4130302	3
vb_null	11	4	4129807	5
vb_not_null	12	4	4130063	5
tinybl	13	5	4130044	9
medbl	14	5	4130044	11
longbl	15	5	4130044	12
enum_col	16	6	766	1
enum_not_null	17	6	1022	1
set_col	18	6	766	1
set_not_null	19	6	1022	1
js_null	20	5	3015925	12
js_not_null	21	5	3016181	12
geom_null	22	14	1279	12
geom_not_null	23	14	1535	12"
string_rows=$(run_mysql \
    "SELECT c.NAME,c.POS,c.MTYPE,c.PRTYPE,c.LEN "\
"FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/string_sample' ORDER BY c.POS;")
expect_value "innodb columns string rows" "$expected_string_rows" "$string_rows"

expected_charset_rows="c_ascii	0	13	721150	4
vc_ascii	1	12	721167	10
txt_ascii	2	5	721148	10
c_u8bin	3	13	3014910	16
vc_u8gen	4	12	2949135	40
c_bin_charset	5	3	4130046	4
vc_bin_charset	6	4	4129807	10
txt_bin_charset	7	5	4130044	10"
charset_rows=$(run_mysql \
    "SELECT c.NAME,c.POS,c.MTYPE,c.PRTYPE,c.LEN "\
"FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/charset_sample' ORDER BY c.POS;")
expect_value "innodb columns charset rows" "$expected_charset_rows" "$charset_rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_columns c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME LIKE '${DATABASE}/%';")
expect_value "case-insensitive innodb columns table name count" "64" "$case_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_COLUMNS c "\
"JOIN INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID WHERE t.NAME LIKE '${DATABASE}/%';")
expect_value "unqualified innodb columns count" "64" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME LIKE '${DATABASE}/%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb columns status" "0	-1" "$status"

rename_before=$(run_mysql \
    "SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES "\
"WHERE NAME = '${DATABASE}/c_sample';")
run_mysql "USE ${DATABASE}; RENAME TABLE c_sample TO renamed_sample;" >/dev/null
rename_after=$(run_mysql \
    "SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES "\
"WHERE NAME = '${DATABASE}/renamed_sample';")
expect_value "innodb columns rename table id" "$rename_before" "$rename_after"

rename_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS c "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = c.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/renamed_sample';")
expect_value "innodb columns renamed table count" "18" "$rename_count"

printf '%s\n' "mysql_baseline_information_schema_innodb_columns_expectations: ok"
