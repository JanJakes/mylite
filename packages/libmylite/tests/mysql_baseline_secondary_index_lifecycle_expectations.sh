#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_secondary_index_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_secondary_index_lifecycle_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE idx_t ("\
"id INT NOT NULL, v INT, amount DECIMAL(5,2), d DATE, c CHAR(3), "\
"name VARCHAR(20), body TEXT, PRIMARY KEY (id), KEY k_v (v), "\
"INDEX k_amount (amount), KEY k_date (d), KEY k_char (c), KEY k_name (name));" \
    "$DATABASE" >/dev/null

show_create_expected=$(cat <<\EXPECTED
idx_t	CREATE TABLE `idx_t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `amount` decimal(5,2) DEFAULT NULL,
  `d` date DEFAULT NULL,
  `c` char(3) DEFAULT NULL,
  `name` varchar(20) DEFAULT NULL,
  `body` text,
  PRIMARY KEY (`id`),
  KEY `k_v` (`v`),
  KEY `k_amount` (`amount`),
  KEY `k_date` (`d`),
  KEY `k_char` (`c`),
  KEY `k_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders secondary indexes" \
    "$show_create_expected" \
    "SHOW CREATE TABLE idx_t;" \
    "$DATABASE"

show_index_expected=$(cat <<\EXPECTED
idx_t	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
idx_t	1	k_v	1	v	A	0	NULL	NULL	YES	BTREE			YES	NULL
idx_t	1	k_amount	1	amount	A	0	NULL	NULL	YES	BTREE			YES	NULL
idx_t	1	k_date	1	d	A	0	NULL	NULL	YES	BTREE			YES	NULL
idx_t	1	k_char	1	c	A	0	NULL	NULL	YES	BTREE			YES	NULL
idx_t	1	k_name	1	name	A	0	NULL	NULL	YES	BTREE			YES	NULL
EXPECTED
)
expect_output \
    "show index renders secondary indexes" \
    "$show_index_expected" \
    "SHOW INDEX FROM idx_t;" \
    "$DATABASE"

statistics_expected=$(cat <<\EXPECTED
idx_t	1	k_amount	1	amount	A	NULL	YES	BTREE	YES	NULL
idx_t	1	k_char	1	c	A	NULL	YES	BTREE	YES	NULL
idx_t	1	k_date	1	d	A	NULL	YES	BTREE	YES	NULL
idx_t	1	k_name	1	name	A	NULL	YES	BTREE	YES	NULL
idx_t	1	k_v	1	v	A	NULL	YES	BTREE	YES	NULL
idx_t	0	PRIMARY	1	id	A	NULL		BTREE	YES	NULL
EXPECTED
)
expect_output \
    "information schema statistics renders indexes" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "\
"SUB_PART, NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'idx_t' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

unnamed_expected=$(cat <<\EXPECTED
unnamed_idx	CREATE TABLE `unnamed_idx` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  KEY `v` (`v`),
  KEY `v_2` (`v`),
  KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "unnamed indexes derive names and suffixes" \
    "$unnamed_expected" \
    "CREATE TABLE unnamed_idx (id INT, v INT, KEY (v), INDEX (v), KEY (id)); "\
"SHOW CREATE TABLE unnamed_idx;" \
    "$DATABASE"

expect_error \
    "unknown key column fails" \
    1072 \
    "42000" \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE bad_unknown (id INT, KEY k_missing (missing));" \
    "$DATABASE"

expect_error \
    "duplicate explicit key name fails" \
    1061 \
    "42000" \
    "Duplicate key name 'k'" \
    "CREATE TABLE bad_duplicate (id INT, KEY k (id), KEY k (id));" \
    "$DATABASE"

expect_error \
    "primary secondary key name fails" \
    1280 \
    "42000" \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE bad_primary_name (id INT, KEY \`PRIMARY\` (id));" \
    "$DATABASE"

expect_error \
    "text key without prefix fails" \
    1170 \
    "42000" \
    "BLOB/TEXT column 'body' used in key specification without a key length" \
    "CREATE TABLE bad_text_key (body TEXT, KEY k (body));" \
    "$DATABASE"

expect_error \
    "non-string prefix fails" \
    1089 \
    "HY000" \
    "Incorrect prefix key" \
    "CREATE TABLE bad_int_prefix (id INT, KEY k (id(3)));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-column indexes deferred by MyLite" \
    "CREATE TABLE upstream_multi (id INT, v INT, KEY k (id, v));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts descending indexes deferred by MyLite" \
    "CREATE TABLE upstream_desc (id INT, KEY k (id DESC));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts using clauses deferred by MyLite" \
    "CREATE TABLE upstream_using (id INT, KEY k USING BTREE (id));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts varchar prefix indexes deferred by MyLite" \
    "CREATE TABLE upstream_prefix (name VARCHAR(10), KEY k (name(3)));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts unique indexes covered by the unique-index baseline" \
    "CREATE TABLE upstream_unique (v INT, UNIQUE KEY u_v (v));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts standalone create index deferred by MyLite" \
    "CREATE TABLE upstream_create_index (v INT); CREATE INDEX k_v ON upstream_create_index(v);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_secondary_index_lifecycle_expectations: ok"
