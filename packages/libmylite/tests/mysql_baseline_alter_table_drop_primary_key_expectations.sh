#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_drop_pk_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_drop_primary_key_expectations: $1" >&2
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

drop_columns=$(printf '%b' 'id\tint\tNO\t\tNULL\t\nv\tint\tYES\tMUL\tNULL\t')
drop_rest=$(cat <<\EXPECTED
drop_pk	1	k_v	1	v	A	2	NULL	NULL	YES	BTREE			YES	NULL
drop_pk	CREATE TABLE `drop_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id		NO	NULL
v	MUL	YES	NULL
k_v	1	1	v	YES
0
0
1:10,2:20,1:30
EXPECTED
)
drop_expected=$(cat <<EXPECTED
2	0
$drop_columns
$drop_rest
EXPECTED
)
expect_output \
    "drop primary key metadata and secondary index preservation" \
    "$drop_expected" \
    "CREATE TABLE drop_pk (id INT PRIMARY KEY, v INT, KEY k_v (v)); "\
"INSERT INTO drop_pk VALUES (1,10),(2,20); "\
"ALTER TABLE drop_pk DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM drop_pk; "\
"SHOW INDEX FROM drop_pk; "\
"SHOW CREATE TABLE drop_pk; "\
"SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_pk' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, IF(NULLABLE = '', 'NO', NULLABLE) "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_pk' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_pk'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_pk'; "\
"INSERT INTO drop_pk VALUES (1,30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v) FROM drop_pk;" \
    "$DATABASE"

added_expected=$(cat <<\EXPECTED
2	0
id	NO		NULL
v	YES		NULL
added_pk	CREATE TABLE `added_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:10,2:20,1:30
EXPECTED
)
expect_output \
    "drop primary key added by alter" \
    "$added_expected" \
    "CREATE TABLE added_pk (id INT, v INT); "\
"INSERT INTO added_pk VALUES (1,10),(2,20); "\
"ALTER TABLE added_pk ADD PRIMARY KEY (id); "\
"ALTER TABLE added_pk DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'added_pk' "\
"ORDER BY ORDINAL_POSITION; "\
"SHOW CREATE TABLE added_pk; "\
"INSERT INTO added_pk VALUES (1,30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v) FROM added_pk;" \
    "$DATABASE"

composite_expected=$(cat <<\EXPECTED
2	0
a	NO		NULL
b	NO		NULL
v	YES		NULL
comp_pk	CREATE TABLE `comp_pk` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop composite primary key" \
    "$composite_expected" \
    "CREATE TABLE comp_pk (a INT, b INT, v INT, PRIMARY KEY (a,b)); "\
"INSERT INTO comp_pk VALUES (1,1,10),(2,2,20); "\
"ALTER TABLE comp_pk DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'comp_pk' "\
"ORDER BY ORDINAL_POSITION; "\
"SHOW CREATE TABLE comp_pk;" \
    "$DATABASE"

empty_expected=$(cat <<\EXPECTED
0	0
EXPECTED
)
expect_output \
    "drop primary key affected rows on empty table" \
    "$empty_expected" \
    "CREATE TABLE empty_pk (id INT PRIMARY KEY); "\
"ALTER TABLE empty_pk DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

auto_key_expected=$(cat <<\EXPECTED
0	0
id	int	NO	MUL	NULL	auto_increment
v	int	YES		NULL	
ai_key	1	id_idx	1	id	A	0	NULL	NULL		BTREE			YES	NULL
1:30
EXPECTED
)
expect_output \
    "drop primary key preserves auto increment when another key remains" \
    "$auto_key_expected" \
    "CREATE TABLE ai_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT, KEY id_idx (id)); "\
"ALTER TABLE ai_key DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ai_key; "\
"SHOW INDEX FROM ai_key; "\
"CREATE TABLE ai_clone LIKE ai_key; "\
"INSERT INTO ai_clone (v) VALUES (30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM ai_clone;" \
    "$DATABASE"

expect_error \
    "drop only auto increment key fails" \
    1075 \
    42000 \
    "Incorrect table definition; there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE ai_only (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE ai_only DROP PRIMARY KEY;" \
    "$DATABASE"

expect_error \
    "no primary key fails" \
    1091 \
    42000 \
    "Can't DROP 'PRIMARY'; check that column/key exists" \
    "CREATE TABLE no_pk (id INT); ALTER TABLE no_pk DROP PRIMARY KEY;" \
    "$DATABASE"

qualified_expected=$(cat <<\EXPECTED
0	0
qualified_pk	CREATE TABLE `qualified_pk` (
  `id` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified drop primary key succeeds" \
    "$qualified_expected" \
    "CREATE TABLE qualified_pk (id INT PRIMARY KEY); "\
"ALTER TABLE ${DATABASE}.qualified_pk DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE qualified_pk;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default DROP PRIMARY KEY;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing DROP PRIMARY KEY;"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table DROP PRIMARY KEY;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action drop primary key" \
    "CREATE TABLE deferred_multi (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE deferred_multi DROP PRIMARY KEY, ADD KEY k_v (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts drop constraint primary when unambiguous" \
    "CREATE TABLE deferred_constraint (id INT PRIMARY KEY); "\
"ALTER TABLE deferred_constraint DROP CONSTRAINT \`PRIMARY\`;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_drop_primary_key_expectations: ok"
