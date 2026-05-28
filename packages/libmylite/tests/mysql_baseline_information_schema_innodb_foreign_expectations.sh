#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_foreign_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_foreign_expectations: $1" >&2
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
     CREATE TABLE parent(
       id INT NOT NULL PRIMARY KEY,
       b INT NOT NULL,
       UNIQUE KEY ub(id,b)
     ) ENGINE=InnoDB;
     CREATE TABLE child_default(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       FOREIGN KEY(pid) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_cascade(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_cascade FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE CASCADE ON UPDATE CASCADE
     ) ENGINE=InnoDB;
     CREATE TABLE child_delete_cascade_update_restrict(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_delete_cascade_update_restrict FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE CASCADE ON UPDATE RESTRICT
     ) ENGINE=InnoDB;
     CREATE TABLE child_delete_restrict_update_cascade(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_delete_restrict_update_cascade FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE RESTRICT ON UPDATE CASCADE
     ) ENGINE=InnoDB;
     CREATE TABLE child_delete_setnull_update_noaction(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_delete_setnull_update_noaction FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE SET NULL ON UPDATE NO ACTION
     ) ENGINE=InnoDB;
     CREATE TABLE child_delete_noaction_update_setnull(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_delete_noaction_update_setnull FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE NO ACTION ON UPDATE SET NULL
     ) ENGINE=InnoDB;
     CREATE TABLE child_setnull(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       pb INT,
       CONSTRAINT fk_setnull FOREIGN KEY(pid,pb) REFERENCES parent(id,b)
         ON DELETE SET NULL ON UPDATE SET NULL
     ) ENGINE=InnoDB;
     CREATE TABLE child_noaction(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_noaction FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE NO ACTION ON UPDATE NO ACTION
     ) ENGINE=InnoDB;
     CREATE TABLE child_restrict(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_restrict FOREIGN KEY(pid) REFERENCES parent(id)
         ON DELETE RESTRICT ON UPDATE RESTRICT
     ) ENGINE=InnoDB;" >/dev/null

table_kinds=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA "\
"IN ('INNODB_FOREIGN', 'INNODB_FOREIGN_COLS');")
expect_value "innodb foreign table kinds" \
    "INNODB_FOREIGN	SYSTEM VIEW
INNODB_FOREIGN_COLS	SYSTEM VIEW" \
    "$table_kinds"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FOREIGN', 'INNODB_FOREIGN_COLS') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb foreign system table rows" \
    "INNODB_FOREIGN	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
INNODB_FOREIGN_COLS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_FOREIGN	ID	1	NULL	YES	varchar	129	387	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(129)	select
INNODB_FOREIGN	FOR_NAME	2	NULL	YES	varchar	129	387	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(129)	select
INNODB_FOREIGN	REF_NAME	3	NULL	YES	varchar	129	387	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(129)	select
INNODB_FOREIGN	N_COLS	4	0	NO	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
INNODB_FOREIGN	TYPE	5	0	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned	select
INNODB_FOREIGN_COLS	ID	1	NULL	YES	varchar	129	387	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(129)	select
INNODB_FOREIGN_COLS	FOR_COL_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
INNODB_FOREIGN_COLS	REF_COL_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
INNODB_FOREIGN_COLS	POS	4	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FOREIGN', 'INNODB_FOREIGN_COLS') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb foreign columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_foreign_rows="${DATABASE}/child_default_ibfk_1	${DATABASE}/child_default	${DATABASE}/parent	1	48
${DATABASE}/fk_cascade	${DATABASE}/child_cascade	${DATABASE}/parent	1	5
${DATABASE}/fk_delete_cascade_update_restrict	${DATABASE}/child_delete_cascade_update_restrict	${DATABASE}/parent	1	1
${DATABASE}/fk_delete_noaction_update_setnull	${DATABASE}/child_delete_noaction_update_setnull	${DATABASE}/parent	1	24
${DATABASE}/fk_delete_restrict_update_cascade	${DATABASE}/child_delete_restrict_update_cascade	${DATABASE}/parent	1	4
${DATABASE}/fk_delete_setnull_update_noaction	${DATABASE}/child_delete_setnull_update_noaction	${DATABASE}/parent	1	34
${DATABASE}/fk_noaction	${DATABASE}/child_noaction	${DATABASE}/parent	1	48
${DATABASE}/fk_restrict	${DATABASE}/child_restrict	${DATABASE}/parent	1	0
${DATABASE}/fk_setnull	${DATABASE}/child_setnull	${DATABASE}/parent	2	10"
foreign_rows=$(run_mysql \
    "SELECT ID, FOR_NAME, REF_NAME, N_COLS, TYPE "\
"FROM INFORMATION_SCHEMA.INNODB_FOREIGN "\
"WHERE ID LIKE '${DATABASE}/%' ORDER BY ID;")
expect_value "innodb foreign rows" "$expected_foreign_rows" "$foreign_rows"

expected_foreign_cols_rows="${DATABASE}/child_default_ibfk_1	pid	id	1
${DATABASE}/fk_cascade	pid	id	1
${DATABASE}/fk_delete_cascade_update_restrict	pid	id	1
${DATABASE}/fk_delete_noaction_update_setnull	pid	id	1
${DATABASE}/fk_delete_restrict_update_cascade	pid	id	1
${DATABASE}/fk_delete_setnull_update_noaction	pid	id	1
${DATABASE}/fk_noaction	pid	id	1
${DATABASE}/fk_restrict	pid	id	1
${DATABASE}/fk_setnull	pid	id	1
${DATABASE}/fk_setnull	pb	b	2"
foreign_cols_rows=$(run_mysql \
    "SELECT ID, FOR_COL_NAME, REF_COL_NAME, POS "\
"FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS "\
"WHERE ID LIKE '${DATABASE}/%' ORDER BY ID, POS;")
expect_value "innodb foreign cols rows" "$expected_foreign_cols_rows" "$foreign_cols_rows"

counts=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN WHERE ID LIKE '${DATABASE}/%'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS WHERE ID LIKE '${DATABASE}/%';")
expect_value "innodb foreign counts" "9
10" "$counts"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_foreign WHERE ID LIKE '${DATABASE}/%';")
expect_value "case-insensitive innodb foreign table name count" "9" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_FOREIGN WHERE ID LIKE '${DATABASE}/%'; "\
"SELECT COUNT(*) FROM INNODB_FOREIGN_COLS WHERE ID LIKE '${DATABASE}/%';")
expect_value "unqualified innodb foreign counts" "9
10" "$use_counts"

alias_rows=$(run_mysql \
    "SELECT f.ID, f.TYPE FROM INFORMATION_SCHEMA.INNODB_FOREIGN AS f "\
"WHERE f.ID LIKE '${DATABASE}/%' AND f.TYPE IN (0, 10, 48) ORDER BY f.ID;")
expect_value "innodb foreign alias rows" \
    "${DATABASE}/child_default_ibfk_1	48
${DATABASE}/fk_noaction	48
${DATABASE}/fk_restrict	0
${DATABASE}/fk_setnull	10" \
    "$alias_rows"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN WHERE ID LIKE '${DATABASE}/%'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb foreign status" "0	-1" "$status"

drop_counts=$(run_mysql \
    "USE ${DATABASE}; ALTER TABLE child_restrict DROP FOREIGN KEY fk_restrict; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN WHERE ID LIKE '${DATABASE}/%'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS WHERE ID LIKE '${DATABASE}/%';")
expect_value "innodb foreign counts after drop" "8
9" "$drop_counts"

printf '%s\n' "mysql_baseline_information_schema_innodb_foreign_expectations: ok"
