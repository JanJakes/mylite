#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_add_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_index_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

metadata_expected=$(cat <<\EXPECTED
0	0
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL,
  `c` char(3) DEFAULT NULL,
  `amount` decimal(5,2) DEFAULT NULL,
  `d` date DEFAULT NULL,
  `dt` datetime DEFAULT NULL,
  `ts` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `k_v` (`v`),
  KEY `name` (`name`),
  KEY `name_2` (`name`),
  KEY `k_c` (`c`),
  KEY `k_amount` (`amount`),
  KEY `k_d` (`d`),
  KEY `k_dt` (`dt`),
  KEY `k_ts` (`ts`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
k_amount	1	1	amount	YES
k_c	1	1	c	YES
k_d	1	1	d	YES
k_dt	1	1	dt	YES
k_ts	1	1	ts	YES
k_v	1	1	v	YES
name	1	1	name	YES
name_2	1	1	name	YES
EXPECTED
)
expect_output \
    "add index metadata for supported descriptor families" \
    "$metadata_expected" \
    "CREATE TABLE t ("\
"id INT PRIMARY KEY, v INT, name VARCHAR(10), c CHAR(3), amount DECIMAL(5,2), "\
"d DATE, dt DATETIME, ts TIMESTAMP NULL"\
"); "\
"INSERT INTO t VALUES "\
"(1,10,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03','2024-01-01 01:02:03'),"\
"(2,NULL,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL); "\
"ALTER TABLE t ADD INDEX k_v (v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD KEY (name); "\
"ALTER TABLE t ADD INDEX (name); "\
"ALTER TABLE t ADD INDEX k_c (c); "\
"ALTER TABLE t ADD INDEX k_amount (amount); "\
"ALTER TABLE t ADD INDEX k_d (d); "\
"ALTER TABLE t ADD INDEX k_dt (dt); "\
"ALTER TABLE t ADD INDEX k_ts (ts); "\
"SHOW CREATE TABLE t; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

auto_increment_expected=$(cat <<\EXPECTED
1:10,2:20
EXPECTED
)
expect_output \
    "added secondary index keeps auto increment keyed for later primary drop" \
    "$auto_increment_expected" \
    "CREATE TABLE ai (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE ai ADD KEY id_idx (id); "\
"ALTER TABLE ai DROP PRIMARY KEY; "\
"INSERT INTO ai (v) VALUES (10),(20); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM ai;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default ADD INDEX k_v (v);"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing ADD INDEX k_v (v);"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ADD INDEX k_v (v);" \
    "$DATABASE"

expect_error \
    "duplicate explicit index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'k_v'" \
    "CREATE TABLE duplicate_name (id INT, v INT, KEY k_v (v)); "\
"ALTER TABLE duplicate_name ADD INDEX k_v (id);" \
    "$DATABASE"

expect_error \
    "quoted primary index name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE quoted_primary (id INT, v INT); "\
"ALTER TABLE quoted_primary ADD INDEX \`PRIMARY\` (v);" \
    "$DATABASE"

expect_error \
    "unknown key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE unknown_column (id INT, v INT); "\
"ALTER TABLE unknown_column ADD INDEX k_missing (missing);" \
    "$DATABASE"

expect_error \
    "text without prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'txt' used in key specification without a key length" \
    "CREATE TABLE text_key (id INT, txt TEXT); "\
"ALTER TABLE text_key ADD INDEX k_txt (txt);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action add index" \
    "CREATE TABLE deferred_multi_action (id INT, v INT); "\
"ALTER TABLE deferred_multi_action ADD INDEX k_id (id), ADD INDEX k_v (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-column add index" \
    "CREATE TABLE deferred_multi_part (id INT, v INT); "\
"ALTER TABLE deferred_multi_part ADD INDEX k_multi (id, v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts add unique" \
    "CREATE TABLE deferred_unique (id INT, v INT); "\
"ALTER TABLE deferred_unique ADD UNIQUE u_v (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts standalone create index" \
    "CREATE TABLE deferred_create_index (id INT, v INT); "\
"CREATE INDEX k_v ON deferred_create_index (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts index options" \
    "CREATE TABLE deferred_options (id INT, v INT, name VARCHAR(10)); "\
"ALTER TABLE deferred_options ADD INDEX k_name USING BTREE (name(2)) COMMENT 'hello' VISIBLE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_add_index_expectations: ok"
