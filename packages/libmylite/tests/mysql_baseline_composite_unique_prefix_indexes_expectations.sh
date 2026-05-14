#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_composite_unique_prefix_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_composite_unique_prefix_indexes_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
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

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
cup	CREATE TABLE `cup` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` varchar(10) DEFAULT NULL,
  `n` int DEFAULT NULL,
  UNIQUE KEY `u_ab` (`a`(3),`b`(2)),
  UNIQUE KEY `u_full_prefix` (`a`,`c`(2)),
  UNIQUE KEY `u_alt` (`b`(2),`c`(3)),
  UNIQUE KEY `u_created` (`a`(4),`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
cup	0	u_ab	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
cup	0	u_ab	2	b	A	0	2	NULL	YES	BTREE			YES	NULL
cup	0	u_full_prefix	1	a	A	0	NULL	NULL	YES	BTREE			YES	NULL
cup	0	u_full_prefix	2	c	A	0	2	NULL	YES	BTREE			YES	NULL
cup	0	u_alt	1	b	A	0	2	NULL	YES	BTREE			YES	NULL
cup	0	u_alt	2	c	A	0	3	NULL	YES	BTREE			YES	NULL
cup	0	u_created	1	a	A	0	4	NULL	YES	BTREE			YES	NULL
cup	0	u_created	2	b	A	0	NULL	NULL	YES	BTREE			YES	NULL
u_ab	0	1	a	3	YES
u_ab	0	2	b	2	YES
u_alt	0	1	b	2	YES
u_alt	0	2	c	3	YES
u_created	0	1	a	4	YES
u_created	0	2	b	NULL	YES
u_full_prefix	0	1	a	NULL	YES
u_full_prefix	0	2	c	2	YES
EXPECTED
)
expect_output \
    "composite unique prefix metadata across create alter and create index" \
    "$metadata_expected" \
    "CREATE TABLE cup (a VARCHAR(10), b VARCHAR(10), c VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2)), UNIQUE KEY u_full_prefix (a, c(2))); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE cup ADD UNIQUE KEY u_alt (b(2), c(3)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_created ON cup (a(4), b); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE cup; SHOW INDEX FROM cup; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'cup' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

null_ignore_expected=$(cat <<\EXPECTED
6	0
2	2
abcdef	xyzz	keep	1
abc123	zzzz	keep	2
NULL	xyqq	keep	3
abcdef	NULL	keep	4
NULL	xyqq	keep	5
abcdef	NULL	keep	6
def000	xy11	keep	8
NULL	xy11	keep	9
EXPECTED
)
expect_output \
    "composite unique prefix permits null tuples and insert ignore skips duplicates" \
    "$null_ignore_expected" \
    "CREATE TABLE null_ignore (a VARCHAR(10), b VARCHAR(10), c VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2))); "\
"INSERT INTO null_ignore VALUES "\
"('abcdef','xyzz','keep',1),('abc123','zzzz','keep',2),(NULL,'xyqq','keep',3),"\
"('abcdef',NULL,'keep',4),(NULL,'xyqq','keep',5),('abcdef',NULL,'keep',6); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT IGNORE INTO null_ignore VALUES "\
"('abc999','xy11','keep',7),('def000','xy11','keep',8),(NULL,'xy11','keep',9),"\
"('def000','xy22','keep',10); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT a,b,c,n FROM null_ignore ORDER BY n;" \
    "$DATABASE"

expect_output \
    "insert ignore composite prefix warnings" \
    "Warning	1062	Duplicate entry 'abc-xy' for key 'ignore_warning.u_ab'
Warning	1062	Duplicate entry 'def-xy' for key 'ignore_warning.u_ab'" \
    "CREATE TABLE ignore_warning (a VARCHAR(10), b VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2))); "\
"INSERT INTO ignore_warning VALUES ('abcdef','xyzz',1),('def000','xy11',2); "\
"INSERT IGNORE INTO ignore_warning VALUES "\
"('abc999','xy11',3),('def000','xy22',4),('ghi000','xy22',5); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "insert duplicate composite prefix tuple" \
    1062 \
    23000 \
    "Duplicate entry 'abc-xy' for key 'insert_dup.u_ab'" \
    "CREATE TABLE insert_dup (a VARCHAR(10), b VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2))); "\
"INSERT INTO insert_dup VALUES ('abcdef','xyzz',1); "\
"INSERT INTO insert_dup VALUES ('abc999','xy11',2);" \
    "$DATABASE"

expect_error \
    "update duplicate composite prefix tuple" \
    1062 \
    23000 \
    "Duplicate entry 'abc-xy' for key 'update_dup.u_ab'" \
    "CREATE TABLE update_dup (a VARCHAR(10), b VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2))); "\
"INSERT INTO update_dup VALUES ('abcdef','xyzz',1),('def000','xy11',2); "\
"UPDATE update_dup SET a = 'abc999' WHERE n = 2;" \
    "$DATABASE"

expect_error \
    "update internal duplicate composite prefix tuple" \
    1062 \
    23000 \
    "Duplicate entry 'qqq-uv' for key 'update_internal.u_ab'" \
    "CREATE TABLE update_internal (a VARCHAR(10), b VARCHAR(10), n INT, "\
"UNIQUE KEY u_ab (a(3), b(2))); "\
"INSERT INTO update_internal VALUES ('def111','uv11',1),('ghi222','uv22',2); "\
"UPDATE update_internal SET a = 'qqq999';" \
    "$DATABASE"

expect_error \
    "alter add unique validates existing composite prefix duplicates" \
    1062 \
    23000 \
    "Duplicate entry 'abc-xy' for key 'alter_dup.u_ab'" \
    "CREATE TABLE alter_dup (a VARCHAR(10), b VARCHAR(10)); "\
"INSERT INTO alter_dup VALUES ('abcdef','xyzz'),('abc999','xy11'); "\
"ALTER TABLE alter_dup ADD UNIQUE KEY u_ab (a(3), b(2));" \
    "$DATABASE"

expect_error \
    "create unique index validates existing composite prefix duplicates" \
    1062 \
    23000 \
    "Duplicate entry 'abc-xy' for key 'create_dup.u_ab'" \
    "CREATE TABLE create_dup (a VARCHAR(10), b VARCHAR(10)); "\
"INSERT INTO create_dup VALUES ('abcdef','xyzz'),('abc999','xy11'); "\
"CREATE UNIQUE INDEX u_ab ON create_dup (a(3), b(2));" \
    "$DATABASE"
