#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_create_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_create_index_lifecycle_expectations: $1" >&2
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
0	0
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `u` bigint unsigned DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL,
  `c` char(3) DEFAULT NULL,
  `amount` decimal(5,2) DEFAULT NULL,
  `d` date DEFAULT NULL,
  `dt` datetime DEFAULT NULL,
  `ts` timestamp NULL DEFAULT NULL,
  `txt` text,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_name` (`name`),
  KEY `k_v` (`v`),
  KEY `k_u` (`u`),
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
k_u	1	1	u	YES
k_v	1	1	v	YES
u_name	0	1	name	YES
EXPECTED
)
expect_output \
    "create index metadata for supported descriptor families" \
    "$metadata_expected" \
    "CREATE TABLE t ("\
"id INT PRIMARY KEY, v INT, u BIGINT UNSIGNED, name VARCHAR(10), c CHAR(3), "\
"amount DECIMAL(5,2), d DATE, dt DATETIME, ts TIMESTAMP NULL, txt TEXT"\
"); "\
"INSERT INTO t VALUES "\
"(1,10,100,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03',"\
"'2024-01-01 01:02:03','hello'),"\
"(2,NULL,200,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL,'body'); "\
"CREATE INDEX k_v ON t (v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_name ON t (name); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE INDEX k_u ON t (u); "\
"CREATE INDEX k_c ON t (c); "\
"CREATE INDEX k_amount ON t (amount); "\
"CREATE INDEX k_d ON t (d); "\
"CREATE INDEX k_dt ON t (dt); "\
"CREATE INDEX k_ts ON t (ts); "\
"SHOW CREATE TABLE t; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

schema_expected=$(cat <<\EXPECTED
q	CREATE TABLE `q` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified create index succeeds without selected schema" \
    "$schema_expected" \
    "CREATE TABLE ${DATABASE}.q (id INT, v INT); "\
"CREATE INDEX k_v ON ${DATABASE}.q (v); "\
"SHOW CREATE TABLE ${DATABASE}.q;"

duplicate_null_expected=$(cat <<\EXPECTED
0	0
duplicate_null	CREATE TABLE `duplicate_null` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  UNIQUE KEY `u_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create unique index permits duplicate nulls" \
    "$duplicate_null_expected" \
    "CREATE TABLE duplicate_null (id INT, v INT); "\
"INSERT INTO duplicate_null VALUES (1,NULL),(2,NULL),(3,10); "\
"CREATE UNIQUE INDEX u_v ON duplicate_null (v); "\
"SELECT ROW_COUNT(), @@warning_count; SHOW CREATE TABLE duplicate_null;" \
    "$DATABASE"

expect_error \
    "create unique index rejects duplicate populated values" \
    1062 \
    23000 \
    "Duplicate entry '10' for key 'duplicate_values.u_v'" \
    "CREATE TABLE duplicate_values (id INT, v INT); "\
"INSERT INTO duplicate_values VALUES (1,10),(2,10); "\
"CREATE UNIQUE INDEX u_v ON duplicate_values (v);" \
    "$DATABASE"

expect_error \
    "create unique varchar index is case-insensitive" \
    1062 \
    23000 \
    "Duplicate entry 'a' for key 'varchar_case.u_name'" \
    "CREATE TABLE varchar_case (id INT, name VARCHAR(10)); "\
"INSERT INTO varchar_case VALUES (1,'a'),(2,'A'); "\
"CREATE UNIQUE INDEX u_name ON varchar_case (name);" \
    "$DATABASE"

expect_error \
    "create unique char index observes stored char canonicalization" \
    1062 \
    23000 \
    "Duplicate entry 'a' for key 'char_space.u_name'" \
    "CREATE TABLE char_space (id INT, name CHAR(10)); "\
"INSERT INTO char_space VALUES (1,'a'),(2,'a '); "\
"CREATE UNIQUE INDEX u_name ON char_space (name);" \
    "$DATABASE"

expect_output \
    "create unique varchar index preserves no-pad trailing spaces" \
    "0	0" \
    "CREATE TABLE varchar_space (id INT, name VARCHAR(10)); "\
"INSERT INTO varchar_space VALUES (1,'a'),(2,'a '); "\
"CREATE UNIQUE INDEX u_name ON varchar_space (name); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE INDEX k_v ON no_default (v);"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE INDEX k_v ON ${MISSING_DATABASE}.missing (v);"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "CREATE INDEX k_v ON missing_table (v);" \
    "$DATABASE"

expect_error \
    "duplicate explicit index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'k_v'" \
    "CREATE TABLE duplicate_name (id INT, v INT, KEY k_v (v)); "\
"CREATE INDEX k_v ON duplicate_name (id);" \
    "$DATABASE"

expect_error \
    "quoted primary index name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE quoted_primary (id INT, v INT); "\
"CREATE INDEX \`PRIMARY\` ON quoted_primary (v);" \
    "$DATABASE"

expect_error \
    "unknown key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE unknown_column (id INT, v INT); "\
"CREATE INDEX k_missing ON unknown_column (missing);" \
    "$DATABASE"

expect_error \
    "text without prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'txt' used in key specification without a key length" \
    "CREATE TABLE text_key (id INT, txt TEXT); "\
"CREATE INDEX k_txt ON text_key (txt);" \
    "$DATABASE"

expect_error \
    "char zero length index fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'c'" \
    "CREATE TABLE char_zero (c CHAR(0)); CREATE INDEX k_c ON char_zero (c);" \
    "$DATABASE"

expect_error \
    "varchar zero length index fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'v'" \
    "CREATE TABLE varchar_zero (v VARCHAR(0)); CREATE UNIQUE INDEX u_v ON varchar_zero (v);" \
    "$DATABASE"

expect_error \
    "missing index name is syntax error" \
    1064 \
    42000 \
    "near 'ON no_name (v)'" \
    "CREATE TABLE no_name (id INT, v INT); CREATE INDEX ON no_name (v);" \
    "$DATABASE"

expect_error \
    "if not exists is syntax error" \
    1064 \
    42000 \
    "near 'IF NOT EXISTS k_v ON ine (v)'" \
    "CREATE TABLE ine (id INT, v INT); CREATE INDEX IF NOT EXISTS k_v ON ine (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-column create index" \
    "CREATE TABLE deferred_multi_part (id INT, v INT); "\
"CREATE INDEX k_multi ON deferred_multi_part (id, v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts prefix create index" \
    "CREATE TABLE deferred_prefix (id INT, name VARCHAR(10)); "\
"CREATE INDEX k_prefix ON deferred_prefix (name(2));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts descending create index" \
    "CREATE TABLE deferred_desc (id INT, v INT); "\
"CREATE INDEX k_desc ON deferred_desc (v DESC);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts functional create index" \
    "CREATE TABLE deferred_func (id INT, v INT); "\
"CREATE INDEX k_func ON deferred_func ((v + 1));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts create index options" \
    "CREATE TABLE deferred_options (id INT, name VARCHAR(10)); "\
"CREATE INDEX k_name USING BTREE ON deferred_options (name) COMMENT 'hello' VISIBLE;" \
    "$DATABASE"
