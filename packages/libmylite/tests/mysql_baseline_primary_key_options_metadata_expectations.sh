#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_primary_key_options_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_primary_key_options_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
pk_options	CREATE TABLE `pk_options` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE COMMENT 'pk comment'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PRIMARY	BTREE		pk comment	YES	1	id	NULL	A
pk_options	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE		pk comment	YES	NULL
EXPECTED
)
expect_output \
    "create-table primary-key options metadata" \
    "$create_table_expected" \
    "CREATE TABLE pk_options ("\
"id INT NOT NULL, v INT, PRIMARY KEY USING BTREE (id) COMMENT 'pk comment' VISIBLE"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE pk_options; "\
"SELECT INDEX_NAME, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE, SEQ_IN_INDEX, "\
"COLUMN_NAME, SUB_PART, COLLATION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='pk_options' AND INDEX_NAME='PRIMARY'; "\
"SHOW INDEX FROM pk_options;" \
    "$DATABASE"

hash_expected=$(cat <<\EXPECTED
Note	3502	This storage engine does not support the HASH index algorithm, storage engine default was used instead.
pk_hash	CREATE TABLE `pk_hash` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`) COMMENT 'hash pk'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PRIMARY	BTREE	hash pk	YES
EXPECTED
)
expect_output \
    "primary-key hash fallback metadata" \
    "$hash_expected" \
    "CREATE TABLE pk_hash ("\
"id INT NOT NULL, PRIMARY KEY USING HASH (id) COMMENT 'hash pk'"\
"); "\
"SHOW WARNINGS; "\
"SHOW CREATE TABLE pk_hash; "\
"SELECT INDEX_NAME, INDEX_TYPE, INDEX_COMMENT, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='pk_hash' AND INDEX_NAME='PRIMARY';" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
0	0
0	1
pk_alter	CREATE TABLE `pk_alter` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE COMMENT 'alter pk'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
pk_alter_hash	CREATE TABLE `pk_alter_hash` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`) COMMENT 'alter hash'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter primary-key options metadata" \
    "$alter_expected" \
    "CREATE TABLE pk_alter (id INT NOT NULL, v INT); "\
"ALTER TABLE pk_alter ADD PRIMARY KEY USING BTREE (id) COMMENT 'alter pk'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE TABLE pk_alter_hash (id INT NOT NULL); "\
"ALTER TABLE pk_alter_hash ADD PRIMARY KEY USING HASH (id) COMMENT 'alter hash'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE pk_alter; "\
"SHOW CREATE TABLE pk_alter_hash;" \
    "$DATABASE"

repeated_expected=$(cat <<\EXPECTED
pk_repeated	CREATE TABLE `pk_repeated` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`) USING BTREE COMMENT 'second'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
second	YES
0
pk_repeated_hash	CREATE TABLE `pk_repeated_hash` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`) COMMENT 'h'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "repeated primary-key options use final value" \
    "$repeated_expected" \
    "CREATE TABLE pk_repeated ("\
"id INT NOT NULL, PRIMARY KEY USING HASH (id) USING BTREE COMMENT 'first' COMMENT 'second' "\
"INVISIBLE VISIBLE"\
"); "\
"SHOW CREATE TABLE pk_repeated; "\
"SELECT INDEX_COMMENT, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='pk_repeated' AND INDEX_NAME='PRIMARY'; "\
"SELECT @@warning_count; "\
"CREATE TABLE pk_repeated_hash ("\
"id INT NOT NULL, PRIMARY KEY USING BTREE (id) USING HASH COMMENT 'h'"\
"); "\
"SHOW CREATE TABLE pk_repeated_hash;" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
pk_clone	CREATE TABLE `pk_clone` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE COMMENT 'pk comment'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
pk comment	YES
EXPECTED
)
expect_output \
    "create table like preserves primary-key options" \
    "$clone_expected" \
    "CREATE TABLE pk_clone LIKE pk_options; "\
"SHOW CREATE TABLE pk_clone; "\
"SELECT INDEX_COMMENT, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='pk_clone' AND INDEX_NAME='PRIMARY';" \
    "$DATABASE"

expect_error \
    "primary-key invisible rejected" \
    3522 \
    HY000 \
    "A primary key index cannot be invisible" \
    "CREATE TABLE pk_invisible (id INT NOT NULL, PRIMARY KEY (id) INVISIBLE);" \
    "$DATABASE"

expect_error \
    "primary-key RTREE rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "CREATE TABLE pk_rtree (id INT NOT NULL, PRIMARY KEY USING RTREE (id));" \
    "$DATABASE"

expect_error \
    "primary-key hash explicit order rejected" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "CREATE TABLE pk_hash_desc (id INT NOT NULL, PRIMARY KEY USING HASH (id DESC));" \
    "$DATABASE"

expect_error \
    "alter primary-key RTREE rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "CREATE TABLE pk_alter_rtree (id INT NOT NULL); "\
"ALTER TABLE pk_alter_rtree ADD PRIMARY KEY USING RTREE (id);" \
    "$DATABASE"

long_comment=$(printf '%01025d' 0 | tr '0' 'a')
expect_error \
    "primary-key comment too long" \
    1688 \
    HY000 \
    "Comment for index 'PRIMARY' is too long (max = 1024)" \
    "CREATE TABLE pk_too_long (id INT NOT NULL, PRIMARY KEY (id) COMMENT '${long_comment}');" \
    "$DATABASE"
