#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_add_primary_key_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_primary_key_expectations: $1" >&2
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

metadata_columns=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\tMUL\tNULL\t')
metadata_rest=$(cat <<\EXPECTED
add_pk	0	PRIMARY	1	id	A	2	NULL	NULL		BTREE			YES	NULL
add_pk	1	k_v	1	v	A	2	NULL	NULL	YES	BTREE			YES	NULL
add_pk	CREATE TABLE `add_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id	PRI	NO	NULL
v	MUL	YES	NULL
PRIMARY	PRIMARY KEY	YES
PRIMARY	id	1	NULL
PRIMARY	0	1	id	NO
k_v	1	1	v	YES
EXPECTED
)
metadata_expected=$(cat <<EXPECTED
0	0
$metadata_columns
$metadata_rest
EXPECTED
)
expect_output \
    "alter add primary key metadata and secondary index preservation" \
    "$metadata_expected" \
    "CREATE TABLE add_pk (id INT, v INT, KEY k_v (v)); "\
"INSERT INTO add_pk VALUES (2,20),(1,10); "\
"ALTER TABLE add_pk ADD PRIMARY KEY (id); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_pk; "\
"SHOW INDEX FROM add_pk; "\
"SHOW CREATE TABLE add_pk; "\
"SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_pk' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_pk' ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, IFNULL(REFERENCED_TABLE_NAME, 'NULL') "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'add_pk' ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, IF(NULLABLE = '', 'NO', NULLABLE) "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_pk' "\
"ORDER BY NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_error \
    "omitted insert after no-default primary key fails" \
    1364 \
    HY000 \
    "Field 'id' doesn't have a default value" \
    "INSERT INTO add_pk (v) VALUES (30);" \
    "$DATABASE"

default_columns=$(printf '%b' 'id\tint\tNO\tPRI\t7\t\nv\tint\tYES\t\tNULL\t')
default_rest=$(cat <<\EXPECTED
default_pk	CREATE TABLE `default_pk` (
  `id` int NOT NULL DEFAULT '7',
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
7	10
EXPECTED
)
default_expected=$(cat <<EXPECTED
$default_columns
$default_rest
EXPECTED
)
expect_output \
    "alter add primary key preserves non-null default" \
    "$default_expected" \
    "CREATE TABLE default_pk (id INT DEFAULT 7, v INT); "\
"INSERT INTO default_pk (v) VALUES (10); "\
"ALTER TABLE default_pk ADD PRIMARY KEY (id); "\
"SHOW COLUMNS FROM default_pk; "\
"SHOW CREATE TABLE default_pk; "\
"SELECT * FROM default_pk;" \
    "$DATABASE"

expect_error \
    "omitted insert after preserved default can duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '7' for key 'default_pk.PRIMARY'" \
    "INSERT INTO default_pk (v) VALUES (20);" \
    "$DATABASE"

explicit_null_columns=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\t\tNULL\t')
explicit_null_create=$(cat <<\EXPECTED
explicit_null_pk	CREATE TABLE `explicit_null_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
explicit_null_expected=$(cat <<EXPECTED
$explicit_null_columns
$explicit_null_create
EXPECTED
)
expect_output \
    "alter add primary key removes explicit default null" \
    "$explicit_null_expected" \
    "CREATE TABLE explicit_null_pk (id INT DEFAULT NULL, v INT); "\
"INSERT INTO explicit_null_pk VALUES (1, 10); "\
"ALTER TABLE explicit_null_pk ADD PRIMARY KEY (id); "\
"SHOW COLUMNS FROM explicit_null_pk; "\
"SHOW CREATE TABLE explicit_null_pk;" \
    "$DATABASE"

expect_error \
    "existing duplicates fail" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'dup.PRIMARY'" \
    "CREATE TABLE dup (id INT); INSERT INTO dup VALUES (1),(1); ALTER TABLE dup ADD PRIMARY KEY (id);" \
    "$DATABASE"

expect_error \
    "existing null values fail" \
    1138 \
    22004 \
    "Invalid use of NULL value" \
    "CREATE TABLE has_null (id INT); INSERT INTO has_null VALUES (NULL); "\
"ALTER TABLE has_null ADD PRIMARY KEY (id);" \
    "$DATABASE"

expect_error \
    "missing key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE missing_col (id INT); ALTER TABLE missing_col ADD PRIMARY KEY (missing);" \
    "$DATABASE"

expect_error \
    "existing primary key fails" \
    1068 \
    42000 \
    "Multiple primary key defined" \
    "CREATE TABLE existing_pk (id INT PRIMARY KEY); ALTER TABLE existing_pk ADD PRIMARY KEY (id);" \
    "$DATABASE"

qualified_expected=$(cat <<\EXPECTED
qualified_pk	CREATE TABLE `qualified_pk` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified alter add primary key succeeds" \
    "$qualified_expected" \
    "CREATE TABLE qualified_pk (id INT); ALTER TABLE ${DATABASE}.qualified_pk ADD PRIMARY KEY (id); "\
"SHOW CREATE TABLE qualified_pk;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default ADD PRIMARY KEY (id);"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing ADD PRIMARY KEY (id);" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ADD PRIMARY KEY (id);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred varchar primary key" \
    "CREATE TABLE deferred_varchar_pk (id VARCHAR(10)); "\
"ALTER TABLE deferred_varchar_pk ADD PRIMARY KEY (id);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite primary key" \
    "CREATE TABLE deferred_composite_pk (a INT, b INT); "\
"ALTER TABLE deferred_composite_pk ADD PRIMARY KEY (a, b);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred named primary key constraint" \
    "CREATE TABLE deferred_named_pk (id INT); "\
"ALTER TABLE deferred_named_pk ADD CONSTRAINT named_pk PRIMARY KEY (id);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred add primary key with using clause" \
    "CREATE TABLE deferred_using_pk (id INT); "\
"ALTER TABLE deferred_using_pk ADD PRIMARY KEY USING BTREE (id);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred multi-action add primary key" \
    "CREATE TABLE deferred_multi_pk (id INT, v INT); "\
"ALTER TABLE deferred_multi_pk ADD PRIMARY KEY (id), ADD KEY k_v (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred drop primary key" \
    "CREATE TABLE deferred_drop_pk (id INT PRIMARY KEY); "\
"ALTER TABLE deferred_drop_pk DROP PRIMARY KEY;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_add_primary_key_expectations: ok"
