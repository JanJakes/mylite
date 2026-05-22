#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_action_alter_primary_key_$$"

fail() {
    printf '%s\n' "mysql_baseline_multi_action_alter_primary_key_expectations: $1" >&2
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

expect_upstream_warning() {
    label=$1
    message=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$message"*) ;;
        *) fail "$label: expected warning output containing [$message], got [$output]" ;;
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

add_pk_expected=$(cat <<\EXPECTED
0	0
add_pk	CREATE TABLE `add_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
add_pk	1	k_v	1	v	A
add_pk	0	PRIMARY	1	id	A
EXPECTED
)
expect_output \
    "add primary key and secondary index" \
    "$add_pk_expected" \
    "CREATE TABLE add_pk (id INT NOT NULL, v INT); "\
"INSERT INTO add_pk VALUES (1,10),(2,20); "\
"ALTER TABLE add_pk ADD PRIMARY KEY(id), ADD KEY k_v(v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_pk; "\
"SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_pk' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

swap_pk_expected=$(cat <<\EXPECTED
0	0
swap_pk	CREATE TABLE `swap_pk` (
  `id` int NOT NULL,
  `v` int NOT NULL,
  PRIMARY KEY (`v`),
  KEY `k_id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
swap_pk	1	k_id	1	id	A
swap_pk	0	PRIMARY	1	v	A
EXPECTED
)
expect_output \
    "drop and add primary key in one statement" \
    "$swap_pk_expected" \
    "CREATE TABLE swap_pk (id INT PRIMARY KEY, v INT NOT NULL, KEY k_id(id)); "\
"INSERT INTO swap_pk VALUES (1,10),(2,20); "\
"ALTER TABLE swap_pk DROP PRIMARY KEY, ADD PRIMARY KEY(v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE swap_pk; "\
"SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'swap_pk' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

add_column_pk_expected=$(cat <<\EXPECTED
0	0
add_then_pk_empty	CREATE TABLE `add_then_pk_empty` (
  `v` int DEFAULT NULL,
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0	0
add_then_pk_default	CREATE TABLE `add_then_pk_default` (
  `v` int DEFAULT NULL,
  `id` int NOT NULL DEFAULT '7',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "add column then primary key on empty tables" \
    "$add_column_pk_expected" \
    "CREATE TABLE add_then_pk_empty (v INT); "\
"ALTER TABLE add_then_pk_empty ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_then_pk_empty; "\
"CREATE TABLE add_then_pk_default (v INT); "\
"ALTER TABLE add_then_pk_default ADD COLUMN id INT NOT NULL DEFAULT 7, ADD PRIMARY KEY(id); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_then_pk_default;" \
    "$DATABASE"

auto_increment_expected=$(cat <<\EXPECTED
0	0
ai_later_key	CREATE TABLE `ai_later_key` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  KEY `k_id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0	0
ai_earlier_key	CREATE TABLE `ai_earlier_key` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  KEY `k_id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "auto increment remains indexed by same-statement secondary key" \
    "$auto_increment_expected" \
    "CREATE TABLE ai_later_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE ai_later_key DROP PRIMARY KEY, ADD KEY k_id(id); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE ai_later_key; "\
"CREATE TABLE ai_earlier_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE ai_earlier_key ADD KEY k_id(id), DROP PRIMARY KEY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE ai_earlier_key;" \
    "$DATABASE"

expect_error \
    "duplicate add primary key rolls back earlier index" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'dup_pk.PRIMARY'" \
    "CREATE TABLE dup_pk (id INT, v INT); "\
"INSERT INTO dup_pk VALUES (1,10),(1,20); "\
"ALTER TABLE dup_pk ADD KEY k_v(v), ADD PRIMARY KEY(id);" \
    "$DATABASE"

expect_output \
    "duplicate add primary key rollback metadata" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'dup_pk';" \
    "$DATABASE"

expect_error \
    "duplicate replacement primary key rolls back original key" \
    1062 \
    23000 \
    "Duplicate entry '10' for key 'swap_dup.PRIMARY'" \
    "CREATE TABLE swap_dup (id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO swap_dup VALUES (1,10),(2,10); "\
"ALTER TABLE swap_dup DROP PRIMARY KEY, ADD PRIMARY KEY(v);" \
    "$DATABASE"

expect_output \
    "replacement primary key rollback metadata" \
    "PRIMARY	id" \
    "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'swap_dup' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_error \
    "null add primary key rolls back earlier index" \
    1138 \
    22004 \
    "Invalid use of NULL value" \
    "CREATE TABLE null_pk (id INT, v INT); "\
"INSERT INTO null_pk VALUES (NULL,10),(2,20); "\
"ALTER TABLE null_pk ADD KEY k_v(v), ADD PRIMARY KEY(id);" \
    "$DATABASE"

expect_output \
    "null add primary key rollback metadata" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'null_pk';" \
    "$DATABASE"

expect_error \
    "existing primary key rolls back later index" \
    1068 \
    42000 \
    "Multiple primary key defined" \
    "CREATE TABLE existing_pk (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE existing_pk ADD PRIMARY KEY(v), ADD KEY k_v(v);" \
    "$DATABASE"

expect_output \
    "existing primary key rollback metadata" \
    "PRIMARY	id" \
    "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'existing_pk' "\
"ORDER BY INDEX_NAME;" \
    "$DATABASE"

expect_error \
    "missing primary key rolls back later index" \
    1091 \
    42000 \
    "Can't DROP 'PRIMARY'; check that column/key exists" \
    "CREATE TABLE no_pk (id INT, v INT); "\
"ALTER TABLE no_pk DROP PRIMARY KEY, ADD KEY k_id(id);" \
    "$DATABASE"

expect_output \
    "missing primary key rollback metadata" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'no_pk';" \
    "$DATABASE"

expect_error \
    "same-statement add column duplicate default rolls back column" \
    1062 \
    23000 \
    "Duplicate entry '0' for key 'add_dup.PRIMARY'" \
    "CREATE TABLE add_dup (v INT); "\
"INSERT INTO add_dup VALUES (1),(2); "\
"ALTER TABLE add_dup ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id);" \
    "$DATABASE"

add_column_rollback_expected=$(cat <<\EXPECTED
v		YES	NULL
0
EXPECTED
)
expect_output \
    "same-statement add column rollback metadata" \
    "$add_column_rollback_expected" \
    "SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_dup' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'add_dup';" \
    "$DATABASE"

expect_error \
    "auto increment final key validation" \
    1075 \
    42000 \
    "Incorrect table definition; there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE ai_bad (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE ai_bad DROP PRIMARY KEY, ADD KEY k_v(v);" \
    "$DATABASE"

expect_output \
    "auto increment final key validation rollback" \
    "PRIMARY	id" \
    "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ai_bad' "\
"ORDER BY INDEX_NAME;" \
    "$DATABASE"

expect_upstream_warning \
    "mysql accepts warning-producing primary key hash in multi-action alter" \
    "This storage engine does not support the HASH index algorithm" \
    "CREATE TABLE hash_pk (id INT NOT NULL, v INT); "\
"ALTER TABLE hash_pk ADD PRIMARY KEY USING HASH(id), ADD KEY k_v(v); "\
"SHOW WARNINGS;" \
    "$DATABASE"
