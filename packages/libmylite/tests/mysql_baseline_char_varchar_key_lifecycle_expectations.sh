#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_char_varchar_key_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_char_varchar_key_lifecycle_expectations: $1" >&2
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

metadata_expected=$(cat <<\EXPECTED
pk_inline	CREATE TABLE `pk_inline` (
  `c` char(3) NOT NULL,
  `v` varchar(3) DEFAULT NULL,
  `n` varchar(3) NOT NULL DEFAULT 'x',
  PRIMARY KEY (`c`),
  UNIQUE KEY `n` (`n`),
  UNIQUE KEY `v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
c	char(3)	NO	PRI	NULL	
v	varchar(3)	YES	UNI	NULL	
n	varchar(3)	NO	UNI	x	
pk_inline	0	PRIMARY	1	c	A	0	NULL	NULL		BTREE			YES	NULL
pk_inline	0	n	1	n	A	0	NULL	NULL		BTREE			YES	NULL
pk_inline	0	v	1	v	A	0	NULL	NULL	YES	BTREE			YES	NULL
pk_inline	0	n	1	n	A	NULL		BTREE	YES	NULL
pk_inline	0	PRIMARY	1	c	A	NULL		BTREE	YES	NULL
pk_inline	0	v	1	v	A	NULL	YES	BTREE	YES	NULL
n	UNIQUE	YES
PRIMARY	PRIMARY KEY	YES
v	UNIQUE	YES
n	n	1	NULL	NULL
PRIMARY	c	1	NULL	NULL
v	v	1	NULL	NULL
EXPECTED
)
expect_output \
    "string key metadata" \
    "$metadata_expected" \
    "CREATE TABLE pk_inline (c CHAR(3) PRIMARY KEY, v VARCHAR(3) UNIQUE, "\
"n VARCHAR(3) UNIQUE NOT NULL DEFAULT 'x'); "\
"SHOW CREATE TABLE pk_inline; SHOW COLUMNS FROM pk_inline; SHOW INDEX FROM pk_inline; "\
"SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "\
"SUB_PART, NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'pk_inline' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'pk_inline' ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, POSITION_IN_UNIQUE_CONSTRAINT, "\
"REFERENCED_TABLE_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'pk_inline' "\
"ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "varchar trailing spaces are distinct under default NO PAD collation" \
    "$(printf '%b' 'a\t1\t61\na \t2\t6120')" \
    "CREATE TABLE varchar_spaces (v VARCHAR(5) UNIQUE); "\
"INSERT INTO varchar_spaces VALUES ('a'),('a '); "\
"SELECT v, LENGTH(v), HEX(v) FROM varchar_spaces ORDER BY v;" \
    "$DATABASE"

expect_output \
    "nullable unique string keys allow duplicate NULL" \
    "$(printf '%b' '1\t<NULL>\n2\t<NULL>\n3\ta')" \
    "CREATE TABLE nullable_unique (id INT, v VARCHAR(5), UNIQUE KEY u_v (v)); "\
"INSERT INTO nullable_unique VALUES (1,NULL),(2,NULL),(3,'a'); "\
"SELECT id, IFNULL(v, '<NULL>') FROM nullable_unique ORDER BY id;" \
    "$DATABASE"

expect_output \
    "alter add varchar primary key accepts trailing-space-distinct rows" \
    "$(cat <<\EXPECTED
add_pk_v_space	CREATE TABLE `add_pk_v_space` (
  `v` varchar(10) NOT NULL,
  PRIMARY KEY (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)" \
    "CREATE TABLE add_pk_v_space (v VARCHAR(10) NOT NULL); "\
"INSERT INTO add_pk_v_space VALUES ('a'),('a '); "\
"ALTER TABLE add_pk_v_space ADD PRIMARY KEY (v); "\
"SHOW CREATE TABLE add_pk_v_space;" \
    "$DATABASE"

expect_output \
    "insert ignore duplicate string keys skips duplicates and keeps valid rows" \
    "$(printf '%b' '1\t4\n1\ta\ta\n2\té\té\n3\t\t\n7\t<NULL>\t<NULL>')" \
    "CREATE TABLE ignore_count (id INT PRIMARY KEY, c CHAR(5) UNIQUE, v VARCHAR(5) UNIQUE); "\
"INSERT INTO ignore_count VALUES (1,'a','a'),(2,'é','é'),(3,'',''); "\
"INSERT IGNORE INTO ignore_count VALUES "\
"(4,'A','A'),(5,'a ','a '),(6,'e','e'),(7,NULL,NULL),(8,'',''); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IFNULL(c,'<NULL>'), IFNULL(v,'<NULL>') FROM ignore_count ORDER BY id;" \
    "$DATABASE"

expect_output \
    "insert ignore duplicate string key warnings" \
    "$(cat <<\EXPECTED
Warning	1062	Duplicate entry 'A' for key 'ignore_warnings.c'
Warning	1062	Duplicate entry 'a' for key 'ignore_warnings.c'
Warning	1062	Duplicate entry 'e' for key 'ignore_warnings.c'
Warning	1062	Duplicate entry '' for key 'ignore_warnings.c'
EXPECTED
)" \
    "CREATE TABLE ignore_warnings (id INT PRIMARY KEY, c CHAR(5) UNIQUE, v VARCHAR(5) UNIQUE); "\
"INSERT INTO ignore_warnings VALUES (1,'a','a'),(2,'é','é'),(3,'',''); "\
"INSERT IGNORE INTO ignore_warnings VALUES "\
"(4,'A','A'),(5,'a ','a '),(6,'e','e'),(7,NULL,NULL),(8,'',''); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "case-insensitive primary duplicate" \
    1062 \
    "23000" \
    "Duplicate entry 'ABC' for key 'pkdup.PRIMARY'" \
    "CREATE TABLE pkdup (c CHAR(3) PRIMARY KEY); "\
"INSERT INTO pkdup VALUES('abc'); INSERT INTO pkdup VALUES('ABC');" \
    "$DATABASE"

expect_error \
    "char trailing-space duplicate" \
    1062 \
    "23000" \
    "Duplicate entry 'a' for key 'char_spaces.c'" \
    "CREATE TABLE char_spaces (c CHAR(5) UNIQUE); "\
"INSERT INTO char_spaces VALUES('a'); INSERT INTO char_spaces VALUES('a ');" \
    "$DATABASE"

expect_error \
    "accent-insensitive duplicate is broader than MyLite first slice" \
    1062 \
    "23000" \
    "Duplicate entry 'e' for key 'accent_unique.v'" \
    "CREATE TABLE accent_unique (v VARCHAR(3) UNIQUE); "\
"INSERT INTO accent_unique VALUES(_utf8mb4'é'); INSERT INTO accent_unique VALUES(_utf8mb4'e');" \
    "$DATABASE"

expect_error \
    "duplicate update uses attempted value" \
    1062 \
    "23000" \
    "Duplicate entry 'ABC' for key 'update_unique.v'" \
    "CREATE TABLE update_unique (id INT PRIMARY KEY, v VARCHAR(3) UNIQUE); "\
"INSERT INTO update_unique VALUES(1,'abc'),(2,'def'); "\
"UPDATE update_unique SET v='ABC' WHERE id=2;" \
    "$DATABASE"

expect_error \
    "primary key rejects null insert" \
    1048 \
    "23000" \
    "Column 'v' cannot be null" \
    "CREATE TABLE pknull (v VARCHAR(3) PRIMARY KEY); INSERT INTO pknull VALUES(NULL);" \
    "$DATABASE"

expect_error \
    "explicit nullable primary key rejected" \
    1171 \
    "42000" \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE bad_nullable (v VARCHAR(10) NULL PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "inline default null primary key rejected" \
    1067 \
    "42000" \
    "Invalid default value for 'v'" \
    "CREATE TABLE bad_default_null (v VARCHAR(10) DEFAULT NULL PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "varchar zero key rejected by storage engine" \
    1167 \
    "42000" \
    "The used storage engine can't index column 'v'" \
    "CREATE TABLE varchar_zero_key (v VARCHAR(0) PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "char zero key rejected by storage engine" \
    1167 \
    "42000" \
    "The used storage engine can't index column 'c'" \
    "CREATE TABLE char_zero_key (c CHAR(0) UNIQUE);" \
    "$DATABASE"

expect_error \
    "alter add varchar primary key rejects case duplicate existing rows" \
    1062 \
    "23000" \
    "Duplicate entry 'a' for key 'add_pk_v.PRIMARY'" \
    "CREATE TABLE add_pk_v (v VARCHAR(10) NOT NULL); "\
"INSERT INTO add_pk_v VALUES ('a'),('A'); "\
"ALTER TABLE add_pk_v ADD PRIMARY KEY (v);" \
    "$DATABASE"

expect_error \
    "alter add char primary key rejects trailing-space duplicate existing rows" \
    1062 \
    "23000" \
    "Duplicate entry 'a' for key 'add_pk_c.PRIMARY'" \
    "CREATE TABLE add_pk_c (c CHAR(3) NOT NULL); "\
"INSERT INTO add_pk_c VALUES ('a'),('a '); "\
"ALTER TABLE add_pk_c ADD PRIMARY KEY (c);" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite string primary keys" \
    "CREATE TABLE deferred_composite_string_pk (a VARCHAR(5), b INT, PRIMARY KEY (a, b));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred string prefix unique indexes" \
    "CREATE TABLE deferred_prefix_unique (v VARCHAR(20), UNIQUE KEY u_v (v(3)));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_char_varchar_key_lifecycle_expectations: ok"
