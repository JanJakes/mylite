#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_add_comp_pk_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_composite_primary_key_expectations: $1" >&2
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

metadata_columns=$(printf '%b' 'a\tint\tNO\tPRI\tNULL\t\nb\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\tMUL\tNULL\t')
metadata_rest=$(cat <<\EXPECTED
add_comp	0	PRIMARY	1	a	A	2	NULL	NULL		BTREE			YES	NULL
add_comp	0	PRIMARY	2	b	A	2	NULL	NULL		BTREE			YES	NULL
add_comp	1	k_v	1	v	A	2	NULL	NULL	YES	BTREE			YES	NULL
add_comp	CREATE TABLE `add_comp` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	PRI	NO	NULL
b	PRI	NO	NULL
v	MUL	YES	NULL
PRIMARY	PRIMARY KEY	YES
PRIMARY	a	1	NULL
PRIMARY	b	2	NULL
PRIMARY	0	1	a	NO
PRIMARY	0	2	b	NO
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
    "alter add composite primary key metadata and secondary index preservation" \
    "$metadata_expected" \
    "CREATE TABLE add_comp (a INT, b INT, v INT, KEY k_v (v)); "\
"INSERT INTO add_comp VALUES (2,1,20),(1,2,10); "\
"ALTER TABLE add_comp ADD PRIMARY KEY (a,b); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_comp; "\
"SHOW INDEX FROM add_comp; "\
"SHOW CREATE TABLE add_comp; "\
"SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_comp' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_comp' ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, IFNULL(REFERENCED_TABLE_NAME, 'NULL') "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'add_comp' ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, IF(NULLABLE = '', 'NO', NULLABLE) "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_comp' "\
"ORDER BY NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

default_columns=$(printf '%b' 'a\tint\tNO\tPRI\t7\t\nb\tint\tNO\tPRI\t8\t\nv\tint\tYES\t\tNULL\t')
default_rest=$(cat <<\EXPECTED
default_comp	CREATE TABLE `default_comp` (
  `a` int NOT NULL DEFAULT '7',
  `b` int NOT NULL DEFAULT '8',
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
7	8	10
EXPECTED
)
default_expected=$(cat <<EXPECTED
$default_columns
$default_rest
EXPECTED
)
expect_output \
    "alter add composite primary key preserves non-null defaults" \
    "$default_expected" \
    "CREATE TABLE default_comp (a INT DEFAULT 7, b INT DEFAULT 8, v INT); "\
"INSERT INTO default_comp (v) VALUES (10); "\
"ALTER TABLE default_comp ADD PRIMARY KEY (a,b); "\
"SHOW COLUMNS FROM default_comp; "\
"SHOW CREATE TABLE default_comp; "\
"SELECT * FROM default_comp;" \
    "$DATABASE"

expect_error \
    "omitted insert after preserved defaults can duplicate composite primary key" \
    1062 \
    23000 \
    "Duplicate entry '7-8' for key 'default_comp.PRIMARY'" \
    "INSERT INTO default_comp (v) VALUES (20);" \
    "$DATABASE"

explicit_null_columns=$(printf '%b' 'a\tint\tNO\tPRI\tNULL\t\nb\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\t\tNULL\t')
explicit_null_create=$(cat <<\EXPECTED
explicit_null_comp	CREATE TABLE `explicit_null_comp` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
explicit_null_expected=$(cat <<EXPECTED
$explicit_null_columns
$explicit_null_create
EXPECTED
)
expect_output \
    "alter add composite primary key removes explicit default null" \
    "$explicit_null_expected" \
    "CREATE TABLE explicit_null_comp (a INT DEFAULT NULL, b INT DEFAULT NULL, v INT); "\
"INSERT INTO explicit_null_comp VALUES (1, 2, 10); "\
"ALTER TABLE explicit_null_comp ADD PRIMARY KEY (a,b); "\
"SHOW COLUMNS FROM explicit_null_comp; "\
"SHOW CREATE TABLE explicit_null_comp;" \
    "$DATABASE"

expect_error \
    "existing duplicate tuples fail" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'dup.PRIMARY'" \
    "CREATE TABLE dup (a INT, b INT); INSERT INTO dup VALUES (1,2),(1,2); "\
"ALTER TABLE dup ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_error \
    "existing null values fail" \
    1138 \
    22004 \
    "Invalid use of NULL value" \
    "CREATE TABLE has_null (a INT, b INT); INSERT INTO has_null VALUES (1,NULL); "\
"ALTER TABLE has_null ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_error \
    "duplicate key parts fail" \
    1060 \
    42S21 \
    "Duplicate column name 'a'" \
    "CREATE TABLE dup_part (a INT, b INT); ALTER TABLE dup_part ADD PRIMARY KEY (a,a);" \
    "$DATABASE"

expect_error \
    "missing key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE missing_col (a INT, b INT); ALTER TABLE missing_col ADD PRIMARY KEY (a, missing);" \
    "$DATABASE"

expect_error \
    "existing primary key fails" \
    1068 \
    42000 \
    "Multiple primary key defined" \
    "CREATE TABLE existing_pk (a INT PRIMARY KEY, b INT); ALTER TABLE existing_pk ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

qualified_expected=$(cat <<\EXPECTED
qualified_comp	CREATE TABLE `qualified_comp` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified alter add composite primary key succeeds" \
    "$qualified_expected" \
    "CREATE TABLE qualified_comp (a INT, b INT); "\
"ALTER TABLE ${DATABASE}.qualified_comp ADD PRIMARY KEY (a,b); "\
"SHOW CREATE TABLE qualified_comp;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default ADD PRIMARY KEY (a,b);"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred varchar composite primary key" \
    "CREATE TABLE deferred_varchar_comp (a VARCHAR(10), b VARCHAR(10)); "\
"ALTER TABLE deferred_varchar_comp ADD PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred named composite primary key constraint" \
    "CREATE TABLE deferred_named_comp (a INT, b INT); "\
"ALTER TABLE deferred_named_comp ADD CONSTRAINT named_pk PRIMARY KEY (a,b);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred add composite primary key with using clause" \
    "CREATE TABLE deferred_using_comp (a INT, b INT); "\
"ALTER TABLE deferred_using_comp ADD PRIMARY KEY USING BTREE (a,b);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_add_composite_primary_key_expectations: ok"
