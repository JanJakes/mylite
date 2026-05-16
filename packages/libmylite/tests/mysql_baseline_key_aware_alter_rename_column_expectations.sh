#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_key_aware_rename_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_key_aware_alter_rename_column_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE keyed_rename ("\
"id INT NOT NULL, "\
"k VARCHAR(20) NOT NULL, "\
"v INT NULL, "\
"body VARCHAR(100), "\
"PRIMARY KEY (id), "\
"UNIQUE KEY uk_k (k), "\
"KEY idx_v (v), "\
"KEY pref_k (k(10)), "\
"FULLTEXT KEY ft_body (body)"\
") ENGINE=InnoDB; "\
"INSERT INTO keyed_rename VALUES (1, 'alpha', 10, 'hello world'), (2, 'beta', 20, 'beta words');" \
    "$DATABASE" >/dev/null

expect_output \
    "rename primary key column" \
    "0	0	1:alpha:10:hello world,2:beta:20:beta words" \
    "ALTER TABLE keyed_rename RENAME COLUMN id TO pk_id; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(pk_id, ':', k, ':', v, ':', body) ORDER BY pk_id) FROM keyed_rename;" \
    "$DATABASE"

expect_output \
    "primary key metadata follows renamed column" \
    "PRIMARY	0	BTREE	1	pk_id	NULL" \
    "SELECT INDEX_NAME, NON_UNIQUE, INDEX_TYPE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'keyed_rename' AND INDEX_NAME = 'PRIMARY';" \
    "$DATABASE"

expect_output \
    "rename secondary key column" \
    "0	0	1:alpha:10:hello world,2:beta:20:beta words" \
    "ALTER TABLE keyed_rename RENAME COLUMN k TO key_name; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(pk_id, ':', key_name, ':', v, ':', body) ORDER BY pk_id) FROM keyed_rename;" \
    "$DATABASE"

secondary_stats_expected=$(cat <<'EXPECTED'
ft_body	1	FULLTEXT	1	body	NULL
idx_v	1	BTREE	1	v	NULL
pref_k	1	BTREE	1	key_name	10
PRIMARY	0	BTREE	1	pk_id	NULL
uk_k	0	BTREE	1	key_name	NULL
EXPECTED
)
expect_output \
    "secondary key metadata follows renamed column" \
    "$secondary_stats_expected" \
    "SELECT INDEX_NAME, NON_UNIQUE, INDEX_TYPE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'keyed_rename' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_output \
    "rename fulltext column" \
    "0	0" \
    "ALTER TABLE keyed_rename RENAME COLUMN body TO content; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

fulltext_stats_expected=$(cat <<'EXPECTED'
ft_body	1	FULLTEXT	1	content	NULL
idx_v	1	BTREE	1	v	NULL
pref_k	1	BTREE	1	key_name	10
PRIMARY	0	BTREE	1	pk_id	NULL
uk_k	0	BTREE	1	key_name	NULL
EXPECTED
)
expect_output \
    "fulltext metadata follows renamed column" \
    "$fulltext_stats_expected" \
    "SELECT INDEX_NAME, NON_UNIQUE, INDEX_TYPE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'keyed_rename' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE parent (id INT NOT NULL, PRIMARY KEY (id)) ENGINE=InnoDB; "\
"CREATE TABLE child ("\
"id INT NOT NULL, "\
"parent_id INT, "\
"KEY p_idx(parent_id), "\
"CONSTRAINT fk_parent FOREIGN KEY (parent_id) REFERENCES parent(id) "\
"ON DELETE CASCADE ON UPDATE CASCADE"\
") ENGINE=InnoDB;" \
    "$DATABASE" >/dev/null

expect_output \
    "rename referenced parent column" \
    "0	0	parent_id	parent_id" \
    "ALTER TABLE parent RENAME COLUMN id TO parent_id; "\
"SELECT ROW_COUNT(), @@warning_count, COLUMN_NAME, REFERENCED_COLUMN_NAME "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'fk_parent' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "rename child foreign-key column" \
    "0	0	pid	parent_id" \
    "ALTER TABLE child RENAME COLUMN parent_id TO pid; "\
"SELECT ROW_COUNT(), @@warning_count, COLUMN_NAME, REFERENCED_COLUMN_NAME "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'fk_parent' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "foreign-key enforcement after rename" \
    "0	1:1,2:2" \
    "INSERT INTO parent VALUES (1), (2); "\
"INSERT INTO child VALUES (1, 1), (2, 2); "\
"INSERT IGNORE INTO child VALUES (3, 99); "\
"SELECT ROW_COUNT(), GROUP_CONCAT(CONCAT(id, ':', pid) ORDER BY id) FROM child;" \
    "$DATABASE"

expect_output \
    "foreign-key delete cascade after rename" \
    "1	2:2" \
    "DELETE FROM parent WHERE parent_id = 1; "\
"SELECT ROW_COUNT(), GROUP_CONCAT(CONCAT(id, ':', pid) ORDER BY id) FROM child;" \
    "$DATABASE"

expect_output \
    "foreign-key update cascade after rename" \
    "1	2:20" \
    "UPDATE parent SET parent_id = 20 WHERE parent_id = 2; "\
"SELECT ROW_COUNT(), GROUP_CONCAT(CONCAT(id, ':', pid) ORDER BY id) FROM child;" \
    "$DATABASE"

cleanup
