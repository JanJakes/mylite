#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_index_options_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_index_options_metadata_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

create_table_expected=$(cat <<\EXPECTED
0	0
t	CREATE TABLE `t` (
  `id` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  `body` text,
  UNIQUE KEY `uk` (`id`) COMMENT 'uniq',
  KEY `k_pre` (`v`(3)) USING BTREE COMMENT 'pre' /*!80000 INVISIBLE */,
  FULLTEXT KEY `ft` (`body`) COMMENT 'full' /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft	FULLTEXT		full	NO	1	body	NULL	NULL
k_pre	BTREE		pre	NO	1	v	3	A
uk	BTREE		uniq	YES	1	id	NULL	A
t	0	uk	1	id	A	0	NULL	NULL	YES	BTREE		uniq	YES	NULL
t	1	k_pre	1	v	A	0	3	NULL	YES	BTREE		pre	NO	NULL
t	1	ft	1	body	NULL	0	NULL	NULL	YES	FULLTEXT		full	NO	NULL
EXPECTED
)
expect_output \
    "create-table index options metadata" \
    "$create_table_expected" \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), body TEXT, "\
"KEY k_pre USING BTREE (v(3)) COMMENT 'pre' INVISIBLE, "\
"UNIQUE KEY uk (id) COMMENT 'uniq' VISIBLE, "\
"FULLTEXT KEY ft (body) COMMENT 'full' INVISIBLE"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SELECT INDEX_NAME, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE, SEQ_IN_INDEX, "\
"COLUMN_NAME, SUB_PART, COLLATION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SHOW INDEX FROM t;" \
    "$DATABASE"

alter_create_expected=$(cat <<\EXPECTED
0	1
0	0
0	1
a	CREATE TABLE `a` (
  `id` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  `w` varchar(20) DEFAULT NULL,
  `body` text,
  KEY `k_hash` (`v`) COMMENT 'hash',
  KEY `k_create` (`w`(4)) USING BTREE COMMENT 'created' /*!80000 INVISIBLE */,
  FULLTEXT KEY `ft_body` (`body`) COMMENT 'alterfull'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft_body	FULLTEXT		alterfull	YES	1	body	NULL	NULL
k_create	BTREE		created	NO	1	w	4	A
k_hash	BTREE		hash	YES	1	v	NULL	A
EXPECTED
)
expect_output \
    "alter and standalone create index options metadata" \
    "$alter_create_expected" \
"CREATE TABLE a (id INT, v VARCHAR(20), w VARCHAR(20), body TEXT); "\
"ALTER TABLE a ADD INDEX k_hash (v) USING HASH COMMENT 'hash'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE INDEX k_create USING BTREE ON a (w(4)) COMMENT 'created' INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE a ADD FULLTEXT INDEX ft_body (body) COMMENT 'alterfull'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE a; "\
"SELECT INDEX_NAME, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE, SEQ_IN_INDEX, "\
"COLUMN_NAME, SUB_PART, COLLATION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='a' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

hash_warning_expected=$(cat <<\EXPECTED
Note	3502	This storage engine does not support the HASH index algorithm, storage engine default was used instead.
EXPECTED
)
expect_output \
    "hash fallback warning text" \
    "$hash_warning_expected" \
    "CREATE TABLE hash_warning (v VARCHAR(20)); "\
"ALTER TABLE hash_warning ADD INDEX k_hash (v) USING HASH; "\
"SHOW WARNINGS;" \
    "$DATABASE"

fulltext_warning_expected=$(cat <<\EXPECTED
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
EXPECTED
)
expect_output \
    "alter fulltext warning text" \
    "$fulltext_warning_expected" \
    "CREATE TABLE fulltext_warning (body TEXT); "\
"ALTER TABLE fulltext_warning ADD FULLTEXT INDEX ft (body); "\
"SHOW WARNINGS;" \
    "$DATABASE"

repeated_expected=$(cat <<\EXPECTED
repeated	CREATE TABLE `repeated` (
  `id` int DEFAULT NULL,
  KEY `k` (`id`) COMMENT 'second' /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
second	NO
EXPECTED
)
expect_output \
    "repeated comment and visibility use final value" \
    "$repeated_expected" \
    "CREATE TABLE repeated ("\
"id INT, KEY k (id) COMMENT 'first' COMMENT 'second' VISIBLE INVISIBLE"\
"); "\
"SHOW CREATE TABLE repeated; "\
"SELECT INDEX_COMMENT, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='repeated' AND INDEX_NAME='k';" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
t_clone	CREATE TABLE `t_clone` (
  `id` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  `body` text,
  UNIQUE KEY `uk` (`id`) COMMENT 'uniq',
  KEY `k_pre` (`v`(3)) USING BTREE COMMENT 'pre' /*!80000 INVISIBLE */,
  FULLTEXT KEY `ft` (`body`) COMMENT 'full' /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft	full	NO
k_pre	pre	NO
uk	uniq	YES
t_clone	CREATE TABLE `t_clone` (
  `id` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  `body` text,
  KEY `renamed_k` (`v`(3)) USING BTREE COMMENT 'pre' /*!80000 INVISIBLE */,
  FULLTEXT KEY `ft` (`body`) COMMENT 'full' /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create table like and rename/drop preserve option metadata" \
    "$clone_expected" \
    "CREATE TABLE t_clone LIKE t; "\
"SHOW CREATE TABLE t_clone; "\
"SELECT INDEX_NAME, INDEX_COMMENT, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t_clone' ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"ALTER TABLE t_clone RENAME INDEX k_pre TO renamed_k; "\
"ALTER TABLE t_clone DROP INDEX uk; "\
"SHOW CREATE TABLE t_clone;" \
    "$DATABASE"

expect_error \
    "hash index rejects explicit key-part order" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE bad_hash_order (id INT, KEY k USING HASH (id DESC));" \
    "$DATABASE"

expect_error \
    "fulltext using btree is syntax error" \
    1064 \
    42000 \
    "near 'USING BTREE)' at line 1" \
    "CREATE TABLE bad_fulltext_using (body TEXT, FULLTEXT KEY ft (body) USING BTREE);" \
    "$DATABASE"

expect_error \
    "index comment length is capped at 1024 characters" \
    1688 \
    HY000 \
    "Comment for index 'k' is too long (max = 1024)" \
    "SET @s = REPEAT('a', 1025); "\
"SET @sql = CONCAT(\"CREATE TABLE too_long_comment (id INT, KEY k (id) COMMENT '\", @s, \"')\"); "\
"PREPARE stmt FROM @sql; "\
"EXECUTE stmt;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_index_options_metadata_expectations: ok"
