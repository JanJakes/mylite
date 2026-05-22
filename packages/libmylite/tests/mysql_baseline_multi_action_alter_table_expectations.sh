#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_action_alter_table_$$"

fail() {
    printf '%s\n' "mysql_baseline_multi_action_alter_table_expectations: $1" >&2
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

add_columns_expected=$(cat <<\EXPECTED
0	0
id	int	NO	PRI	NULL
v	int	YES		NULL
a	int	YES		7
b	int	NO		8
1:10:7:8,2:20:7:8
EXPECTED
)
expect_output \
    "add two columns in one alter" \
    "$add_columns_expected" \
    "CREATE TABLE add_cols (id INT NOT NULL, v INT, PRIMARY KEY(id)); "\
"INSERT INTO add_cols VALUES (1,10),(2,20); "\
"ALTER TABLE add_cols ADD COLUMN a INT DEFAULT 7, ADD COLUMN b INT NOT NULL DEFAULT 8; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, "\
"COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_cols' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', a, ':', b) ORDER BY id) FROM add_cols;" \
    "$DATABASE"

add_column_index_expected=$(cat <<\EXPECTED
0	0
dep	CREATE TABLE `dep` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `k_c` (`c`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "add index over column added earlier in same statement" \
    "$add_column_index_expected" \
    "CREATE TABLE dep (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE dep ADD COLUMN c INT, ADD INDEX k_c (c); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE dep;" \
    "$DATABASE"

drop_add_index_expected=$(cat <<\EXPECTED
0	0
t	1	k_v2	1	v	A
t	0	PRIMARY	1	id	A
EXPECTED
)
expect_output \
    "drop and add secondary indexes atomically" \
    "$drop_add_index_expected" \
    "CREATE TABLE t (id INT NOT NULL, v INT, PRIMARY KEY(id), KEY k_v(v)); "\
"ALTER TABLE t DROP INDEX k_v, ADD INDEX k_v2 (v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

rollback_expected=$(cat <<\EXPECTED
id	int	NO	PRI	NULL
v	int	YES		NULL
1
EXPECTED
)
expect_error \
    "duplicate unique action fails" \
    1062 \
    23000 \
    "Duplicate entry '7' for key 'uniq_fail.u_v'" \
    "CREATE TABLE uniq_fail (id INT PRIMARY KEY, v INT); "\
"INSERT INTO uniq_fail VALUES (1,7),(2,7); "\
"ALTER TABLE uniq_fail ADD COLUMN ok_col INT, ADD UNIQUE INDEX u_v(v);" \
    "$DATABASE"

expect_output \
    "failed later action rolls back earlier added column and index" \
    "$rollback_expected" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, "\
"COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'uniq_fail' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'uniq_fail';" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action alter with trailing online ddl options" \
    "CREATE TABLE option_tail (id INT PRIMARY KEY); "\
"ALTER TABLE option_tail ADD COLUMN a INT, ADD COLUMN b INT, ALGORITHM=INSTANT, LOCK=DEFAULT;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action alter with default changes" \
    "CREATE TABLE default_actions (id INT PRIMARY KEY, a INT, b INT); "\
"ALTER TABLE default_actions ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts primary key inside multi-action alter" \
    "CREATE TABLE primary_action (id INT NOT NULL, v INT); "\
"ALTER TABLE primary_action ADD COLUMN extra_col INT, ADD PRIMARY KEY (id);" \
    "$DATABASE"
