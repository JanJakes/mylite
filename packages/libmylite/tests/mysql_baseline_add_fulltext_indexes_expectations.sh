#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_add_fulltext_indexes_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_add_fulltext_indexes_expectations: $1" >&2
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

alter_metadata_expected=$(cat <<\EXPECTED
0	1
0	0
0	0
0	0
0	0
0	0
posts	CREATE TABLE `posts` (
  `id` int DEFAULT NULL,
  `title` varchar(40) DEFAULT NULL,
  `body` text,
  `other` varchar(20) DEFAULT NULL,
  FULLTEXT KEY `ft_title_body` (`title`,`body`),
  FULLTEXT KEY `body` (`body`),
  FULLTEXT KEY `ft_other` (`other`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
posts	1	ft_title_body	1	title	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
posts	1	ft_title_body	2	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
posts	1	body	1	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
posts	1	ft_other	1	other	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
body	1	body	NULL	NULL	FULLTEXT
ft_other	1	other	NULL	NULL	FULLTEXT
ft_title_body	1	title	NULL	NULL	FULLTEXT
ft_title_body	2	body	NULL	NULL	FULLTEXT
id	<empty>
title	MUL
body	MUL
other	MUL
EXPECTED
)
expect_output \
    "alter add fulltext metadata and warnings" \
    "$alter_metadata_expected" \
    "CREATE TABLE posts (id INT, title VARCHAR(40), body TEXT, other VARCHAR(20)); "\
"ALTER TABLE posts ADD FULLTEXT KEY ft_title_body (title, body(10)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE posts ADD FULLTEXT (body); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE posts ADD FULLTEXT INDEX ft_other (other(3)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE posts DROP INDEX ft_title_body; "\
"ALTER TABLE posts DROP INDEX body; "\
"ALTER TABLE posts DROP INDEX ft_other; "\
"ALTER TABLE posts ADD FULLTEXT KEY ft_title_body (title, body(10)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE posts ADD FULLTEXT (body); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE posts ADD FULLTEXT INDEX ft_other (other(3)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE posts; "\
"SHOW INDEX FROM posts; "\
"SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, INDEX_TYPE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'posts' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT COLUMN_NAME, IF(COLUMN_KEY = '', '<empty>', COLUMN_KEY) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'posts' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

fulltext_warning_expected=$(cat <<\EXPECTED
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
EXPECTED
)
expect_output \
    "first alter add fulltext warning is available via show warnings" \
    "$fulltext_warning_expected" \
    "CREATE TABLE warning_probe (body TEXT); "\
"ALTER TABLE warning_probe ADD FULLTEXT KEY ft_body (body); "\
"SHOW WARNINGS;" \
    "$DATABASE"

create_fulltext_expected=$(cat <<\EXPECTED
0	1
created	CREATE TABLE `created` (
  `body` text,
  `title` varchar(20) DEFAULT NULL,
  FULLTEXT KEY `ft_body` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
created	1	ft_body	1	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
EXPECTED
)
expect_output \
    "standalone create fulltext index metadata and warning" \
    "$create_fulltext_expected" \
    "CREATE TABLE created (body TEXT, title VARCHAR(20)); "\
"CREATE FULLTEXT INDEX ft_body ON created (body(12)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE created; "\
"SHOW INDEX FROM created;" \
    "$DATABASE"

create_time_readd_expected=$(cat <<\EXPECTED
0	0
0	0
EXPECTED
)
expect_output \
    "create-time fulltext drop and re-add has no warning" \
    "$create_time_readd_expected" \
    "CREATE TABLE created_inline (body TEXT, FULLTEXT KEY ft_body (body)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE created_inline DROP INDEX ft_body; "\
"ALTER TABLE created_inline ADD FULLTEXT KEY ft_body (body); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

generated_names_expected=$(cat <<\EXPECTED
generated_names	CREATE TABLE `generated_names` (
  `a` text,
  FULLTEXT KEY `a` (`a`),
  FULLTEXT KEY `a_2` (`a`),
  FULLTEXT KEY `a_3` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter add fulltext generated names" \
    "$generated_names_expected" \
    "CREATE TABLE generated_names (a TEXT); "\
"ALTER TABLE generated_names ADD FULLTEXT (a); "\
"ALTER TABLE generated_names ADD FULLTEXT (a); "\
"ALTER TABLE generated_names ADD FULLTEXT (a); "\
"SHOW CREATE TABLE generated_names;" \
    "$DATABASE"

clone_drop_rename_expected=$(cat <<\EXPECTED
ft_clone	CREATE TABLE `ft_clone` (
  `id` int DEFAULT NULL,
  `body` text,
  `title` varchar(20) DEFAULT NULL,
  FULLTEXT KEY `ft_text` (`body`),
  FULLTEXT KEY `ft_title` (`title`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft_clone	CREATE TABLE `ft_clone` (
  `id` int DEFAULT NULL,
  `body` text,
  `title` varchar(20) DEFAULT NULL,
  FULLTEXT KEY `ft_text` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "added fulltext descriptors clone drop and rename" \
    "$clone_drop_rename_expected" \
    "CREATE TABLE ft_source (id INT, body TEXT, title VARCHAR(20)); "\
"ALTER TABLE ft_source ADD FULLTEXT KEY ft_body (body); "\
"ALTER TABLE ft_source ADD FULLTEXT KEY ft_title (title); "\
"CREATE TABLE ft_clone LIKE ft_source; "\
"ALTER TABLE ft_clone RENAME INDEX ft_body TO ft_text; "\
"SHOW CREATE TABLE ft_clone; "\
"DROP INDEX ft_title ON ft_clone; "\
"SHOW CREATE TABLE ft_clone;" \
    "$DATABASE"

expect_error \
    "non-character fulltext key part fails" \
    1283 \
    HY000 \
    "Column 'id' cannot be part of FULLTEXT index" \
    "CREATE TABLE bad_int (id INT); ALTER TABLE bad_int ADD FULLTEXT KEY ft_id (id);" \
    "$DATABASE"

expect_error \
    "binary fulltext key part fails" \
    1283 \
    HY000 \
    "Column 'payload' cannot be part of FULLTEXT index" \
    "CREATE TABLE bad_blob (payload BLOB); CREATE FULLTEXT INDEX ft_payload ON bad_blob (payload);" \
    "$DATABASE"

expect_error \
    "zero char fulltext key part fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'c'" \
    "CREATE TABLE bad_char_zero (c CHAR(0)); ALTER TABLE bad_char_zero ADD FULLTEXT KEY ft_c (c);" \
    "$DATABASE"

expect_error \
    "zero varchar fulltext key part fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'v'" \
    "CREATE TABLE bad_varchar_zero (v VARCHAR(0)); CREATE FULLTEXT INDEX ft_v ON bad_varchar_zero (v);" \
    "$DATABASE"

expect_error \
    "explicit asc fulltext key part fails" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE bad_asc (body TEXT); ALTER TABLE bad_asc ADD FULLTEXT KEY ft_body (body ASC);" \
    "$DATABASE"

expect_error \
    "explicit desc fulltext key part fails" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE bad_desc (body TEXT); CREATE FULLTEXT INDEX ft_body ON bad_desc (body DESC);" \
    "$DATABASE"

expect_error \
    "zero prefix fulltext key part fails" \
    1391 \
    HY000 \
    "Key part 'body' length cannot be 0" \
    "CREATE TABLE bad_zero_prefix (body TEXT); ALTER TABLE bad_zero_prefix ADD FULLTEXT KEY ft_body (body(0));" \
    "$DATABASE"

expect_error \
    "unknown key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE bad_missing (body TEXT); ALTER TABLE bad_missing ADD FULLTEXT KEY ft_missing (missing);" \
    "$DATABASE"

expect_error \
    "duplicate fulltext index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'ft_body'" \
    "CREATE TABLE bad_duplicate (body TEXT, title TEXT, FULLTEXT KEY ft_body (body)); "\
"ALTER TABLE bad_duplicate ADD FULLTEXT KEY ft_body (title);" \
    "$DATABASE"

expect_error \
    "primary fulltext index name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE bad_primary (body TEXT); ALTER TABLE bad_primary ADD FULLTEXT KEY \`PRIMARY\` (body);" \
    "$DATABASE"

expect_error \
    "constraint fulltext form remains unsupported syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_constraint (body TEXT); ALTER TABLE bad_constraint ADD CONSTRAINT c FULLTEXT KEY ft_body (body);" \
    "$DATABASE"

expect_error \
    "temporary fulltext index fails" \
    1796 \
    HY000 \
    "Cannot create FULLTEXT index on temporary InnoDB table" \
    "CREATE TEMPORARY TABLE bad_temp (body TEXT); ALTER TABLE bad_temp ADD FULLTEXT KEY ft_body (body);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_add_fulltext_indexes_expectations: ok"
