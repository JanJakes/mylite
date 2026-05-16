#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_fulltext_index_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_fulltext_index_metadata_expectations: $1" >&2
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
ft	CREATE TABLE `ft` (
  `id` int DEFAULT NULL,
  `title` varchar(191) DEFAULT NULL,
  `body` text,
  `tiny` tinytext,
  `med` mediumtext,
  `long_body` longtext,
  FULLTEXT KEY `ft_title_body` (`title`,`body`),
  FULLTEXT KEY `ft_texts` (`tiny`,`med`,`long_body`),
  FULLTEXT KEY `body` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft	1	ft_title_body	1	title	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft	1	ft_title_body	2	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft	1	ft_texts	1	tiny	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft	1	ft_texts	2	med	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft	1	ft_texts	3	long_body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft	1	body	1	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
body	1	1	body	NULL	NULL	YES	FULLTEXT	YES	NULL
ft_texts	1	1	tiny	NULL	NULL	YES	FULLTEXT	YES	NULL
ft_texts	1	2	med	NULL	NULL	YES	FULLTEXT	YES	NULL
ft_texts	1	3	long_body	NULL	NULL	YES	FULLTEXT	YES	NULL
ft_title_body	1	1	title	NULL	NULL	YES	FULLTEXT	YES	NULL
ft_title_body	1	2	body	NULL	NULL	YES	FULLTEXT	YES	NULL
id	
title	MUL
body	MUL
tiny	MUL
med	
long_body	
0
EXPECTED
)
expect_output \
    "create-table fulltext metadata" \
    "$metadata_expected" \
    "CREATE TABLE ft ("\
"id INT, title VARCHAR(191), body TEXT, tiny TINYTEXT, med MEDIUMTEXT, "\
"long_body LONGTEXT, FULLTEXT KEY ft_title_body (title, body), "\
"FULLTEXT INDEX ft_texts (tiny, med, long_body), FULLTEXT (body(10))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE ft; "\
"SHOW INDEX FROM ft; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ft' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ft' ORDER BY ORDINAL_POSITION; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ft';" \
    "$DATABASE"

generated_names_expected=$(cat <<\EXPECTED
generated_names	CREATE TABLE `generated_names` (
  `a` text,
  FULLTEXT KEY `a` (`a`),
  FULLTEXT KEY `a_2` (`a`),
  FULLTEXT KEY `a_3` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a
a_2
a_3
EXPECTED
)
expect_output \
    "unnamed fulltext indexes use first column suffixes" \
    "$generated_names_expected" \
    "CREATE TABLE generated_names (a TEXT, FULLTEXT (a), FULLTEXT (a), FULLTEXT (a)); "\
"SHOW CREATE TABLE generated_names; "\
"SELECT INDEX_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'generated_names' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

mixed_index_order_expected=$(cat <<\EXPECTED
mixed_order	CREATE TABLE `mixed_order` (
  `body` text,
  `k` int DEFAULT NULL,
  KEY `k_body` (`k`),
  FULLTEXT KEY `ft_body` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create orders ordinary keys before fulltext keys" \
    "$mixed_index_order_expected" \
    "CREATE TABLE mixed_order (body TEXT, k INT, FULLTEXT KEY ft_body (body), KEY k_body (k)); "\
"SHOW CREATE TABLE mixed_order;" \
    "$DATABASE"

char_varchar_expected=$(cat <<\EXPECTED
0	0
char_varchar	CREATE TABLE `char_varchar` (
  `c` char(1) DEFAULT NULL,
  `v` varchar(1) DEFAULT NULL,
  FULLTEXT KEY `ft_c` (`c`),
  FULLTEXT KEY `ft_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "char and varchar fulltext key parts are accepted above zero length" \
    "$char_varchar_expected" \
    "CREATE TABLE char_varchar (c CHAR(1), v VARCHAR(1), "\
"FULLTEXT KEY ft_c (c), FULLTEXT KEY ft_v (v)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE char_varchar;" \
    "$DATABASE"

clone_drop_rename_expected=$(cat <<\EXPECTED
ft_clone	CREATE TABLE `ft_clone` (
  `id` int DEFAULT NULL,
  `title` varchar(191) DEFAULT NULL,
  `body` text,
  `tiny` tinytext,
  `med` mediumtext,
  `long_body` longtext,
  FULLTEXT KEY `ft_all_texts` (`tiny`,`med`,`long_body`),
  FULLTEXT KEY `body` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft_clone	1	ft_all_texts	1	tiny	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft_clone	1	ft_all_texts	2	med	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft_clone	1	ft_all_texts	3	long_body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft_clone	1	body	1	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
ft_clone	CREATE TABLE `ft_clone` (
  `id` int DEFAULT NULL,
  `title` varchar(191) DEFAULT NULL,
  `body` text,
  `tiny` tinytext,
  `med` mediumtext,
  `long_body` longtext,
  FULLTEXT KEY `body` (`body`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ft_clone	1	body	1	body	NULL	0	NULL	NULL	YES	FULLTEXT			YES	NULL
EXPECTED
)
expect_output \
    "create table like clones fulltext and drop rename update metadata" \
    "$clone_drop_rename_expected" \
    "CREATE TABLE ft_clone LIKE ft; "\
"ALTER TABLE ft_clone RENAME INDEX ft_texts TO ft_all_texts; "\
"ALTER TABLE ft_clone DROP INDEX ft_title_body; "\
"SHOW CREATE TABLE ft_clone; "\
"SHOW INDEX FROM ft_clone; "\
"DROP INDEX ft_all_texts ON ft_clone; "\
"SHOW CREATE TABLE ft_clone; "\
"SHOW INDEX FROM ft_clone;" \
    "$DATABASE"

expect_error \
    "integer fulltext key part fails" \
    1283 \
    HY000 \
    "Column 'i' cannot be part of FULLTEXT index" \
    "CREATE TABLE bad_int (i INT, FULLTEXT KEY ft_i (i));" \
    "$DATABASE"

expect_error \
    "blob fulltext key part fails" \
    1283 \
    HY000 \
    "Column 'b' cannot be part of FULLTEXT index" \
    "CREATE TABLE bad_blob (b BLOB, FULLTEXT KEY ft_b (b));" \
    "$DATABASE"

expect_error \
    "binary fulltext key part fails" \
    1283 \
    HY000 \
    "Column 'b' cannot be part of FULLTEXT index" \
    "CREATE TABLE bad_binary (b BINARY(4), FULLTEXT KEY ft_b (b));" \
    "$DATABASE"

expect_error \
    "zero-length char fulltext key part fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'c'" \
    "CREATE TABLE bad_char_zero (c CHAR(0), FULLTEXT KEY ft_c (c));" \
    "$DATABASE"

expect_error \
    "zero-length varchar fulltext key part fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'v'" \
    "CREATE TABLE bad_varchar_zero (v VARCHAR(0), FULLTEXT KEY ft_v (v));" \
    "$DATABASE"

expect_error \
    "explicit asc fulltext key part fails" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE bad_asc (v VARCHAR(20), FULLTEXT KEY ft_v (v ASC));" \
    "$DATABASE"

expect_error \
    "explicit desc fulltext key part fails" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE bad_desc (v VARCHAR(20), FULLTEXT KEY ft_v (v DESC));" \
    "$DATABASE"

expect_error \
    "zero fulltext prefix still fails" \
    1391 \
    HY000 \
    "Key part 'v' length cannot be 0" \
    "CREATE TABLE bad_zero (v VARCHAR(20), FULLTEXT KEY ft_v (v(0)));" \
    "$DATABASE"

expect_error \
    "constraint fulltext syntax is rejected by MySQL" \
    1064 \
    42000 \
    "near 'FULLTEXT KEY ft_v (v))'" \
    "CREATE TABLE bad_constraint (v VARCHAR(20), CONSTRAINT c FULLTEXT KEY ft_v (v));" \
    "$DATABASE"

expect_error \
    "temporary fulltext table is rejected by MySQL" \
    1796 \
    HY000 \
    "Cannot create FULLTEXT index on temporary InnoDB table" \
    "CREATE TEMPORARY TABLE bad_temp (v VARCHAR(20), FULLTEXT KEY ft_v (v));" \
    "$DATABASE"

cleanup
