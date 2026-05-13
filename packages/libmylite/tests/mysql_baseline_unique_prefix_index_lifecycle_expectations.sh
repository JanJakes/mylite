#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_unique_prefix_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_unique_prefix_index_lifecycle_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
long_prefix=$(printf '%*s' 191 '' | tr ' ' a)
long_duplicate_display=$(printf '%*s' 64 '' | tr ' ' a)
long_first="${long_prefix}x"
long_duplicate="${long_prefix}y"
long_other=$(printf '%*s' 191 '' | tr ' ' b)
long_other="${long_other}z"
tab=$(printf '\t')

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
prefix_unique	CREATE TABLE `prefix_unique` (
  `id` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  `c` char(10) DEFAULT NULL,
  `body` text,
  UNIQUE KEY `u_v` (`v`(3)),
  UNIQUE KEY `u_c` (`c`(2)),
  UNIQUE KEY `u_body` (`body`(4)),
  UNIQUE KEY `u_alt` (`v`(5)),
  UNIQUE KEY `u_created` (`body`(6))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
prefix_unique	0	u_v	1	v	A	0	3	NULL	YES	BTREE			YES	NULL
prefix_unique	0	u_c	1	c	A	0	2	NULL	YES	BTREE			YES	NULL
prefix_unique	0	u_body	1	body	A	0	4	NULL	YES	BTREE			YES	NULL
prefix_unique	0	u_alt	1	v	A	0	5	NULL	YES	BTREE			YES	NULL
prefix_unique	0	u_created	1	body	A	0	6	NULL	YES	BTREE			YES	NULL
u_alt	0	1	v	5	YES	BTREE	YES	NULL
u_body	0	1	body	4	YES	BTREE	YES	NULL
u_c	0	1	c	2	YES	BTREE	YES	NULL
u_created	0	1	body	6	YES	BTREE	YES	NULL
u_v	0	1	v	3	YES	BTREE	YES	NULL
EXPECTED
)
expect_output \
    "unique prefix metadata across create alter and create index" \
    "$metadata_expected" \
    "CREATE TABLE prefix_unique ("\
"id INT, v VARCHAR(20), c CHAR(10), body TEXT, "\
"UNIQUE KEY u_v (v(3)), UNIQUE KEY u_c (c(2)), UNIQUE KEY u_body (body(4))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE prefix_unique ADD UNIQUE KEY u_alt (v(5)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_created ON prefix_unique (body(6)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE prefix_unique; "\
"SHOW INDEX FROM prefix_unique; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "\
"INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'prefix_unique' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

column_key_expected=$(cat <<\EXPECTED
nullable_prefix	CREATE TABLE `nullable_prefix` (
  `v` varchar(20) DEFAULT NULL,
  UNIQUE KEY `u_v` (`v`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
v	varchar(20)	YES	UNI	NULL__TAB__
v	UNI
notnull_prefix	CREATE TABLE `notnull_prefix` (
  `v` varchar(20) NOT NULL,
  UNIQUE KEY `u_v` (`v`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
v	varchar(20)	NO	UNI	NULL__TAB__
v	UNI
EXPECTED
)
column_key_expected=$(printf '%s' "$column_key_expected" | sed "s/__TAB__/${tab}/g")
expect_output \
    "prefix unique columns report UNI not PRI" \
    "$column_key_expected" \
    "CREATE TABLE nullable_prefix (v VARCHAR(20), UNIQUE KEY u_v (v(3))); "\
"SHOW CREATE TABLE nullable_prefix; SHOW COLUMNS FROM nullable_prefix; "\
"SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'nullable_prefix'; "\
"CREATE TABLE notnull_prefix (v VARCHAR(20) NOT NULL, UNIQUE KEY u_v (v(3))); "\
"SHOW CREATE TABLE notnull_prefix; SHOW COLUMNS FROM notnull_prefix; "\
"SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'notnull_prefix';" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
3	1
abcdef	xyzz	body000
defxyz	uvqq	text222
NULL	NULL	NULL
NULL	NULL	NULL
EXPECTED
)
expect_output \
    "insert ignore skips duplicate prefix values and keeps duplicate nulls" \
    "$dml_expected" \
    "CREATE TABLE dml_prefix (v VARCHAR(20), c CHAR(10), body TEXT, "\
"UNIQUE KEY u_v (v(3)), UNIQUE KEY u_c (c(2)), UNIQUE KEY u_body (body(4))); "\
"INSERT INTO dml_prefix VALUES ('abcdef','xyzz','body000'); "\
"INSERT IGNORE INTO dml_prefix VALUES "\
"('abcxyz','xyqq','body111'),('defxyz','uvqq','text222'),(NULL,NULL,NULL),(NULL,NULL,NULL); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT v, c, body FROM dml_prefix ORDER BY v IS NULL, v, c;" \
    "$DATABASE"

odku_expected=$(cat <<\EXPECTED
2	1
1	abcdef	2
EXPECTED
)
expect_output \
    "odku resolves conflict through unique prefix" \
    "$odku_expected" \
    "CREATE TABLE odku_prefix (id INT, v VARCHAR(20), n INT, UNIQUE KEY u_v (v(3))); "\
"INSERT INTO odku_prefix VALUES (1,'abcdef',1); "\
"INSERT INTO odku_prefix VALUES (2,'abcxyz',2) ON DUPLICATE KEY UPDATE n=VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM odku_prefix;" \
    "$DATABASE"

expect_error \
    "long varchar insert duplicate prefix fails with truncated entry text" \
    1062 \
    23000 \
    "Duplicate entry '${long_duplicate_display}' for key 'long_prefix_insert.u_v'" \
    "CREATE TABLE long_prefix_insert (v VARCHAR(255), UNIQUE KEY u_v (v(191))); "\
"INSERT INTO long_prefix_insert VALUES ('${long_first}'); "\
"INSERT INTO long_prefix_insert VALUES ('${long_duplicate}');" \
    "$DATABASE"

long_ignore_expected=$(cat <<EXPECTED
0	1
1
EXPECTED
)
expect_output \
    "long varchar insert ignore duplicate prefix warns and skips row" \
    "$long_ignore_expected" \
    "CREATE TABLE long_prefix_ignore (v VARCHAR(255), UNIQUE KEY u_v (v(191))); "\
"INSERT INTO long_prefix_ignore VALUES ('${long_first}'); "\
"INSERT IGNORE INTO long_prefix_ignore VALUES ('${long_duplicate}'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM long_prefix_ignore;" \
    "$DATABASE"

long_odku_expected=$(cat <<EXPECTED
2	1
2
EXPECTED
)
expect_output \
    "long varchar odku duplicate prefix updates row" \
    "$long_odku_expected" \
    "CREATE TABLE long_prefix_odku (v VARCHAR(255), n INT, UNIQUE KEY u_v (v(191))); "\
"INSERT INTO long_prefix_odku VALUES ('${long_first}', 1); "\
"INSERT INTO long_prefix_odku VALUES ('${long_duplicate}', 2) "\
"ON DUPLICATE KEY UPDATE n = VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT n FROM long_prefix_odku;" \
    "$DATABASE"

expect_error \
    "long varchar update duplicate prefix fails with truncated entry text" \
    1062 \
    23000 \
    "Duplicate entry '${long_duplicate_display}' for key 'long_prefix_update.u_v'" \
    "CREATE TABLE long_prefix_update (id INT, v VARCHAR(255), UNIQUE KEY u_v (v(191))); "\
"INSERT INTO long_prefix_update VALUES (1, '${long_first}'), (2, '${long_other}'); "\
"UPDATE long_prefix_update SET v = '${long_duplicate}' WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "varchar insert duplicate prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'dup_v.u_v'" \
    "CREATE TABLE dup_v (v VARCHAR(20), UNIQUE KEY u_v (v(3))); "\
"INSERT INTO dup_v VALUES ('abcdef'); INSERT INTO dup_v VALUES ('abcxyz');" \
    "$DATABASE"

expect_error \
    "char insert duplicate prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'ABC' for key 'dup_c.u_c'" \
    "CREATE TABLE dup_c (c CHAR(5), UNIQUE KEY u_c (c(3))); "\
"INSERT INTO dup_c VALUES ('abcde'); INSERT INTO dup_c VALUES ('ABCzz');" \
    "$DATABASE"

expect_error \
    "update duplicate prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'update_prefix.u_v'" \
    "CREATE TABLE update_prefix (id INT, v VARCHAR(20), UNIQUE KEY u_v (v(3))); "\
"INSERT INTO update_prefix VALUES (1,'abcdef'),(2,'defghi'); "\
"UPDATE update_prefix SET v='abcxyz' WHERE id=2;" \
    "$DATABASE"

expect_error \
    "alter add unique prefix validates existing duplicates" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'alter_dup.u_v'" \
    "CREATE TABLE alter_dup (v VARCHAR(20)); "\
"INSERT INTO alter_dup VALUES ('abcdef'),('abcxyz'); "\
"ALTER TABLE alter_dup ADD UNIQUE KEY u_v (v(3));" \
    "$DATABASE"

expect_error \
    "create unique index prefix validates existing duplicates" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'create_dup.u_v'" \
    "CREATE TABLE create_dup (v VARCHAR(20)); "\
"INSERT INTO create_dup VALUES ('abcdef'),('abcxyz'); "\
"CREATE UNIQUE INDEX u_v ON create_dup (v(3));" \
    "$DATABASE"

expect_error \
    "integer unique prefix fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_int_prefix (id INT, UNIQUE KEY u_id (id(3)));" \
    "$DATABASE"

expect_error \
    "zero unique prefix fails" \
    1391 \
    HY000 \
    "Key part 'v' length cannot be 0" \
    "CREATE TABLE bad_zero_prefix (v VARCHAR(10), UNIQUE KEY u_v (v(0)));" \
    "$DATABASE"

expect_error \
    "oversized bounded unique prefix fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE bad_varchar_prefix (v VARCHAR(10), UNIQUE KEY u_v (v(11)));" \
    "$DATABASE"

expect_error \
    "unique prefix over 3072 byte key limit fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "CREATE TABLE bad_key_length (v VARCHAR(1000), UNIQUE KEY u_v (v(769)));" \
    "$DATABASE"

expect_error \
    "text unique without prefix still fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'body' used in key specification without a key length" \
    "CREATE TABLE bad_text_unique (body TEXT, UNIQUE KEY u_body (body));" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE UNIQUE INDEX u_no_default ON no_default (v(3));"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE UNIQUE INDEX u_missing_schema ON ${MISSING_DATABASE}.missing (v(3));"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "CREATE UNIQUE INDEX u_missing_table ON missing_table (v(3));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite unique prefix keys" \
    "CREATE TABLE deferred_composite_unique_prefix ("\
"a VARCHAR(20), b VARCHAR(20), UNIQUE KEY u_ab (a(3), b(2)));" \
    "$DATABASE"

printf '%s\n' "baseline unique prefix index MySQL 8.4.9 expectations verified"
