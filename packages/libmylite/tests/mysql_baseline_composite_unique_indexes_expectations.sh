#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_composite_unique_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_composite_unique_indexes_expectations: $1" >&2
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

metadata_expected=$(cat <<\EXPECTED
cu	CREATE TABLE `cu` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  `d` int NOT NULL,
  `e` int NOT NULL,
  UNIQUE KEY `u_de` (`d`,`e`),
  UNIQUE KEY `u_ab` (`a`,`b`),
  UNIQUE KEY `u_ba` (`b`,`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	int	YES	MUL	NULL	
b	int	YES	MUL	NULL	
c	int	YES		NULL	
d	int	NO	PRI	NULL	
e	int	NO	PRI	NULL	
u_ab	0	1	a	A	NULL	YES	BTREE
u_ab	0	2	b	A	NULL	YES	BTREE
u_ba	0	1	b	A	NULL	YES	BTREE
u_ba	0	2	a	A	NULL	YES	BTREE
u_de	0	1	d	A	NULL		BTREE
u_de	0	2	e	A	NULL		BTREE
u_ab	UNIQUE	YES
u_ba	UNIQUE	YES
u_de	UNIQUE	YES
u_ab	a	1	NULL
u_ab	b	2	NULL
u_ba	b	1	NULL
u_ba	a	2	NULL
u_de	d	1	NULL
u_de	e	2	NULL
EXPECTED
)
expect_output \
    "create-table composite unique metadata" \
    "$metadata_expected" \
    "CREATE TABLE cu (a INT, b INT, c INT, d INT NOT NULL, e INT NOT NULL, "\
"UNIQUE KEY u_ab (a,b), UNIQUE KEY u_ba (b,a), UNIQUE KEY u_de (d,e)); "\
"SHOW CREATE TABLE cu; "\
"SHOW COLUMNS FROM cu; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "\
"SUB_PART, NULLABLE, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cu' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "\
"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cu' "\
"ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, POSITION_IN_UNIQUE_CONSTRAINT "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cu' "\
"ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

nullable_expected=$(cat <<\EXPECTED
7	0
1	2	10
1	NULL	11
1	NULL	12
NULL	2	13
NULL	2	14
NULL	NULL	15
NULL	NULL	16
EXPECTED
)
expect_output \
    "composite unique allows duplicate tuples with null parts" \
    "$nullable_expected" \
    "CREATE TABLE nullable_tuple (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO nullable_tuple VALUES "\
"(1,2,10),(1,NULL,11),(1,NULL,12),(NULL,2,13),(NULL,2,14),(NULL,NULL,15),(NULL,NULL,16); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT a,b,c FROM nullable_tuple ORDER BY c;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
1	1
1	2	10
3	4	30
EXPECTED
)
expect_output \
    "insert ignore skips duplicate composite tuple" \
    "$ignore_expected" \
"CREATE TABLE ignore_tuple (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO ignore_tuple VALUES (1,2,10); "\
"INSERT IGNORE INTO ignore_tuple VALUES (1,2,20),(3,4,30); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT a,b,c FROM ignore_tuple ORDER BY c;" \
    "$DATABASE"

expect_output \
    "insert ignore stores composite duplicate warning" \
    "Warning	1062	Duplicate entry '1-2' for key 'ignore_warning.u_ab'" \
    "CREATE TABLE ignore_warning (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO ignore_warning VALUES (1,2,10); "\
"INSERT IGNORE INTO ignore_warning VALUES (1,2,20); SHOW WARNINGS;" \
    "$DATABASE"

update_null_expected=$(cat <<\EXPECTED
2	0
1	2	10
NULL	4	20
NULL	6	30
EXPECTED
)
expect_output \
    "update permits duplicate composite tuples with null parts" \
    "$update_null_expected" \
    "CREATE TABLE update_null_tuple (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO update_null_tuple VALUES (1,2,10),(3,4,20),(5,6,30); "\
"UPDATE update_null_tuple SET a = NULL WHERE c IN (20,30); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT a,b,c FROM update_null_tuple ORDER BY c;" \
    "$DATABASE"

create_index_expected=$(cat <<\EXPECTED
0	0
ci	CREATE TABLE `ci` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  UNIQUE KEY `u_ab` (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
u_ab	0	1	a	A	NULL	YES	BTREE
u_ab	0	2	b	A	NULL	YES	BTREE
EXPECTED
)
expect_output \
    "create unique index composite metadata" \
    "$create_index_expected" \
    "CREATE TABLE ci (a INT, b INT, c INT); "\
"INSERT INTO ci VALUES (1,2,10),(3,4,20); "\
"CREATE UNIQUE INDEX u_ab ON ci (a,b); "\
"SELECT ROW_COUNT(), @@warning_count; SHOW CREATE TABLE ci; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'ci' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
0	0
ai	CREATE TABLE `ai` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  UNIQUE KEY `u_ab` (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
u_ab	0	1	a	A	NULL	YES	BTREE
u_ab	0	2	b	A	NULL	YES	BTREE
EXPECTED
)
expect_output \
    "alter add unique composite metadata" \
    "$alter_expected" \
    "CREATE TABLE ai (a INT, b INT, c INT); "\
"INSERT INTO ai VALUES (1,2,10),(3,4,20); "\
"ALTER TABLE ai ADD UNIQUE KEY u_ab (a,b); "\
"SELECT ROW_COUNT(), @@warning_count; SHOW CREATE TABLE ai; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'ai' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

string_expected=$(printf '%b' '2\t1')
expect_output \
    "composite unique string keys use default case-insensitive collation" \
    "$string_expected" \
    "CREATE TABLE string_tuple (a VARCHAR(10), b CHAR(10), UNIQUE KEY u_ab (a,b)); "\
"INSERT IGNORE INTO string_tuple VALUES ('abc','x'),('ABC','x'),('abc','y'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "duplicate insert fails with tuple diagnostic" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'dup_insert.u_ab'" \
    "CREATE TABLE dup_insert (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO dup_insert VALUES (1,2,10); INSERT INTO dup_insert VALUES (1,2,20);" \
    "$DATABASE"

expect_error \
    "duplicate update fails with tuple diagnostic" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'dup_update.u_ab'" \
    "CREATE TABLE dup_update (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO dup_update VALUES (1,2,10),(3,2,20); "\
"UPDATE dup_update SET a = 1 WHERE c = 20;" \
    "$DATABASE"

expect_error \
    "multi-row update creating internal duplicate fails" \
    1062 \
    23000 \
    "Duplicate entry '9-2' for key 'dup_update_all.u_ab'" \
    "CREATE TABLE dup_update_all (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO dup_update_all VALUES (1,2,10),(3,2,20); "\
"UPDATE dup_update_all SET a = 9;" \
    "$DATABASE"

expect_error \
    "create unique index validates existing composite duplicates" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'create_dup.u_ab'" \
    "CREATE TABLE create_dup (a INT, b INT); "\
"INSERT INTO create_dup VALUES (1,2),(1,2); CREATE UNIQUE INDEX u_ab ON create_dup (a,b);" \
    "$DATABASE"

expect_error \
    "alter add unique validates existing composite duplicates" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'alter_dup.u_ab'" \
    "CREATE TABLE alter_dup (a INT, b INT); "\
"INSERT INTO alter_dup VALUES (1,2),(1,2); ALTER TABLE alter_dup ADD UNIQUE KEY u_ab (a,b);" \
    "$DATABASE"

expect_error \
    "duplicate key part fails" \
    1060 \
    "42S21" \
    "Duplicate column name 'a'" \
    "CREATE TABLE duplicate_part (a INT, UNIQUE KEY u_ab (a,a));" \
    "$DATABASE"

expect_error \
    "duplicate implicit name fails" \
    1061 \
    "42000" \
    "Duplicate key name 'a'" \
    "CREATE TABLE duplicate_name (a INT, b INT, UNIQUE (a,b), KEY a (b), UNIQUE (a,b));" \
    "$DATABASE"

expect_error \
    "missing default schema fails for create index" \
    1046 \
    "3D000" \
    "No database selected" \
    "CREATE UNIQUE INDEX u_ab ON no_default (a,b);"

expect_error \
    "unknown schema fails for create index" \
    1049 \
    "42000" \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE UNIQUE INDEX u_ab ON ${MISSING_DATABASE}.missing (a,b);"

expect_error \
    "unknown table fails for alter add unique" \
    1146 \
    "42S02" \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ADD UNIQUE KEY u_ab (a,b);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite unique prefix keys" \
    "CREATE TABLE deferred_prefix (a VARCHAR(255), b VARCHAR(255), UNIQUE KEY u_ab (a(3),b(4)));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite unique ODKU" \
    "CREATE TABLE deferred_odku (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b)); "\
"INSERT INTO deferred_odku VALUES (1,2,10); "\
"INSERT INTO deferred_odku VALUES (1,2,20) ON DUPLICATE KEY UPDATE c = VALUES(c);" \
    "$DATABASE"

printf '%s\n' "baseline composite unique index MySQL 8.4.9 expectations verified"
