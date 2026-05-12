#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_index_prefix_key_parts_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_index_prefix_key_parts_expectations: $1" >&2
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

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
prefix_t	CREATE TABLE `prefix_t` (
  `id` int DEFAULT NULL,
  `meta_key` varchar(255) DEFAULT NULL,
  `body` text,
  `a` varchar(20) DEFAULT NULL,
  `b` varchar(20) DEFAULT NULL,
  `full_name` varchar(20) DEFAULT NULL,
  KEY `meta_key_prefix` (`meta_key`(191)),
  KEY `body_prefix` (`body`(20)),
  KEY `k_ab` (`a`(3),`b`(4)),
  KEY `k_mix` (`full_name`,`meta_key`(5)),
  KEY `k_alt` (`meta_key`(32)),
  KEY `k_created` (`body`(16))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
prefix_t	1	meta_key_prefix	1	meta_key	A	0	191	NULL	YES	BTREE			YES	NULL
prefix_t	1	body_prefix	1	body	A	0	20	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_ab	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_ab	2	b	A	0	4	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_mix	1	full_name	A	0	NULL	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_mix	2	meta_key	A	0	5	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_alt	1	meta_key	A	0	32	NULL	YES	BTREE			YES	NULL
prefix_t	1	k_created	1	body	A	0	16	NULL	YES	BTREE			YES	NULL
body_prefix	1	1	body	20	YES	BTREE	YES	NULL
k_ab	1	1	a	3	YES	BTREE	YES	NULL
k_ab	1	2	b	4	YES	BTREE	YES	NULL
k_alt	1	1	meta_key	32	YES	BTREE	YES	NULL
k_created	1	1	body	16	YES	BTREE	YES	NULL
k_mix	1	1	full_name	NULL	YES	BTREE	YES	NULL
k_mix	1	2	meta_key	5	YES	BTREE	YES	NULL
meta_key_prefix	1	1	meta_key	191	YES	BTREE	YES	NULL
EXPECTED
)
expect_output \
    "prefix metadata across create table alter add key and create index" \
    "$metadata_expected" \
    "CREATE TABLE prefix_t ("\
"id INT, meta_key VARCHAR(255), body TEXT, a VARCHAR(20), b VARCHAR(20), "\
"full_name VARCHAR(20), KEY meta_key_prefix (meta_key(191)), "\
"KEY body_prefix (body(20)), KEY k_ab (a(3), b(4)), "\
"KEY k_mix (full_name, meta_key(5))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE prefix_t ADD KEY k_alt (meta_key(32)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE INDEX k_created ON prefix_t (body(16)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE prefix_t; "\
"SHOW INDEX FROM prefix_t; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "\
"INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'prefix_t' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

generated_names_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
generated_names	CREATE TABLE `generated_names` (
  `a` varchar(20) DEFAULT NULL,
  `b` text,
  KEY `a` (`a`(2)),
  KEY `a_2` (`a`(3)),
  KEY `b` (`b`(4)),
  KEY `a_3` (`a`(5)),
  KEY `b_2` (`b`(6))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	1	a	2
a_2	1	a	3
a_3	1	a	5
b	1	b	4
b_2	1	b	6
EXPECTED
)
expect_output \
    "generated prefix index names use first key part and suffix collisions" \
    "$generated_names_expected" \
    "CREATE TABLE generated_names ("\
"a VARCHAR(20), b TEXT, KEY (a(2)), INDEX (a(3)), KEY (b(4))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE generated_names ADD KEY (a(5)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE generated_names ADD INDEX (b(6)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE generated_names; "\
"SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'generated_names' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

maximum_key_length_expected=$(cat <<\EXPECTED
0	0
long_prefix_ok	CREATE TABLE `long_prefix_ok` (
  `v` varchar(1000) DEFAULT NULL,
  KEY `k` (`v`(768))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
k	v	768
EXPECTED
)
expect_output \
    "varchar prefix at 3072 byte utf8mb4 key limit succeeds" \
    "$maximum_key_length_expected" \
    "CREATE TABLE long_prefix_ok (v VARCHAR(1000), KEY k (v(768))); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE long_prefix_ok; "\
"SELECT INDEX_NAME, COLUMN_NAME, SUB_PART FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'long_prefix_ok';" \
    "$DATABASE"

expect_error \
    "integer prefix fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_int_prefix (id INT, KEY k (id(3)));" \
    "$DATABASE"

expect_error \
    "zero prefix fails" \
    1391 \
    HY000 \
    "Key part 'v' length cannot be 0" \
    "CREATE TABLE bad_zero_prefix (v VARCHAR(10), KEY k (v(0)));" \
    "$DATABASE"

expect_error \
    "oversized bounded varchar prefix fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE bad_varchar_prefix (v VARCHAR(10), KEY k (v(11)));" \
    "$DATABASE"

expect_error \
    "varchar prefix over 3072 byte utf8mb4 key limit fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "CREATE TABLE bad_key_length (v VARCHAR(1000), KEY k (v(769)));" \
    "$DATABASE"

expect_error \
    "text without prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'body' used in key specification without a key length" \
    "CREATE TABLE bad_text_key (body TEXT, KEY k (body));" \
    "$DATABASE"

expect_error \
    "duplicate prefix key part fails" \
    1060 \
    42S21 \
    "Duplicate column name 'a'" \
    "CREATE TABLE bad_duplicate_part (a VARCHAR(20), KEY k (a(3), a(5)));" \
    "$DATABASE"

expect_error \
    "duplicate prefix index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'k'" \
    "CREATE TABLE bad_duplicate_name (a VARCHAR(20), KEY k (a(3)), KEY k (a(4)));" \
    "$DATABASE"

expect_error \
    "quoted primary prefix index name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE bad_primary_name (a VARCHAR(20), KEY \`PRIMARY\` (a(3)));" \
    "$DATABASE"

expect_error \
    "unknown prefix key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE bad_unknown (a VARCHAR(20), KEY k (missing(3)));" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE INDEX k_no_default ON no_default (a(3));"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE INDEX k_missing_schema ON ${MISSING_DATABASE}.missing (a(3));"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "CREATE INDEX k_missing_table ON missing_table (a(3));" \
    "$DATABASE"

expect_error \
    "unique prefix index rejects later duplicate prefix value" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'unique_prefix.u_v'" \
    "CREATE TABLE unique_prefix (v VARCHAR(20), UNIQUE KEY u_v (v(3))); "\
"INSERT INTO unique_prefix VALUES ('abcdef'); "\
"INSERT INTO unique_prefix VALUES ('abcxyz');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_index_prefix_key_parts_expectations: ok"
