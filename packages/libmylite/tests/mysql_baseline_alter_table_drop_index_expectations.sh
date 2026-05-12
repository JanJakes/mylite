#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_drop_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_drop_index_expectations: $1" >&2
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

drop_secondary_expected=$(cat <<\EXPECTED
0	0
0	0
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `u` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id	PRI
v	
u	
0
0
1:10,2:20,3:10
EXPECTED
)
expect_output \
    "drop nonunique and unique secondary indexes" \
    "$drop_secondary_expected" \
    "CREATE TABLE t (id INT PRIMARY KEY, v INT, u INT, UNIQUE KEY u_v (v), KEY k_u (u)); "\
"INSERT INTO t VALUES (1,10,100),(2,20,200); "\
"ALTER TABLE t DROP INDEX k_u; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t DROP KEY u_v; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' ORDER BY ORDINAL_POSITION; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND CONSTRAINT_TYPE = 'UNIQUE'; "\
"INSERT INTO t VALUES (3,10,300); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

drop_added_expected=$(cat <<\EXPECTED
0	0
added	CREATE TABLE `added` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop index added by alter" \
    "$drop_added_expected" \
    "CREATE TABLE added (id INT, v INT); "\
"ALTER TABLE added ADD INDEX k_v (v); "\
"ALTER TABLE added DROP INDEX k_v; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE added;" \
    "$DATABASE"

case_expected=$(cat <<\EXPECTED
0	0
case_idx	CREATE TABLE `case_idx` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop index names case-insensitively" \
    "$case_expected" \
    "CREATE TABLE case_idx (id INT, v INT, KEY k_v (v)); "\
"ALTER TABLE case_idx DROP INDEX K_V; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE case_idx;" \
    "$DATABASE"

auto_primary_expected=$(cat <<\EXPECTED
0	0
id	int	NO	PRI	NULL	auto_increment
v	int	YES		NULL	
EXPECTED
)
expect_output \
    "drop auto increment secondary index while primary remains" \
    "$auto_primary_expected" \
    "CREATE TABLE ai_primary (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id)); "\
"ALTER TABLE ai_primary DROP INDEX id_idx; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ai_primary;" \
    "$DATABASE"

expect_error \
    "dropping last auto increment key fails" \
    1075 \
    42000 \
    "Incorrect table definition; there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE ai_last (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id)); "\
"ALTER TABLE ai_last DROP PRIMARY KEY; "\
"ALTER TABLE ai_last DROP INDEX id_idx;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default DROP INDEX k_v;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing DROP INDEX k_v;" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table DROP INDEX k_v;" \
    "$DATABASE"

expect_error \
    "unknown index fails" \
    1091 \
    42000 \
    "Can't DROP 'missing_idx'; check that column/key exists" \
    "CREATE TABLE unknown_index (id INT, v INT, KEY k_v (v)); "\
"ALTER TABLE unknown_index DROP INDEX missing_idx;" \
    "$DATABASE"

expect_error \
    "unquoted primary drop index is syntax error" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE primary_syntax (id INT PRIMARY KEY); "\
"ALTER TABLE primary_syntax DROP INDEX PRIMARY;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts standalone drop index" \
    "CREATE TABLE deferred_standalone (id INT, v INT, KEY k_v (v)); "\
"DROP INDEX k_v ON deferred_standalone;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts quoted primary drop index" \
    "CREATE TABLE deferred_primary (id INT PRIMARY KEY); "\
"ALTER TABLE deferred_primary DROP INDEX \`PRIMARY\`;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action drop index" \
    "CREATE TABLE deferred_multi (id INT, v INT, KEY k_id (id)); "\
"ALTER TABLE deferred_multi DROP INDEX k_id, ADD INDEX k_v (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts algorithm and lock options" \
    "CREATE TABLE deferred_options (id INT, v INT, KEY k_v (v)); "\
"ALTER TABLE deferred_options DROP INDEX k_v, ALGORITHM=INPLACE, LOCK=NONE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_drop_index_expectations: ok"
