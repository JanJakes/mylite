#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_action_alter_defaults_$$"

fail() {
    printf '%s\n' "mysql_baseline_multi_action_alter_defaults_expectations: $1" >&2
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

default_actions_expected=$(cat <<\EXPECTED
0	0
id	NULL
a	9
b	NULL
c	NULL
EXPECTED
)
expect_output \
    "set and drop defaults in one alter" \
    "$default_actions_expected" \
    "CREATE TABLE defaults_t (id INT PRIMARY KEY, a INT DEFAULT 1, b INT DEFAULT 2, c INT); "\
"ALTER TABLE defaults_t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'defaults_t' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "dropped default makes omitted insert fail" \
    1364 \
    HY000 \
    "Field 'b' doesn't have a default value" \
    "INSERT INTO defaults_t (id, c) VALUES (1, 5);" \
    "$DATABASE"

mixed_actions_expected=$(cat <<\EXPECTED
0	0
add_after	CREATE TABLE `add_after` (
  `id` int NOT NULL,
  `a` int DEFAULT '3',
  `c` int DEFAULT '4',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "default change can mix with add column over existing target" \
    "$mixed_actions_expected" \
    "CREATE TABLE add_after (id INT PRIMARY KEY, a INT DEFAULT 1); "\
"ALTER TABLE add_after ALTER a SET DEFAULT 3, ADD COLUMN c INT DEFAULT 4; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_after;" \
    "$DATABASE"

add_then_alter_existing_expected=$(cat <<\EXPECTED
0	0
add_then_alter	CREATE TABLE `add_then_alter` (
  `id` int NOT NULL,
  `a` int DEFAULT '3',
  `c` int DEFAULT '4',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "default change after add column can target an existing column" \
    "$add_then_alter_existing_expected" \
    "CREATE TABLE add_then_alter (id INT PRIMARY KEY, a INT DEFAULT 1); "\
"ALTER TABLE add_then_alter ADD COLUMN c INT DEFAULT 4, ALTER a SET DEFAULT 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_then_alter;" \
    "$DATABASE"

expect_error \
    "default action cannot target column added earlier in same statement" \
    1054 \
    42S22 \
    "Unknown column 'c' in 'later'" \
    "CREATE TABLE later (id INT PRIMARY KEY); "\
"ALTER TABLE later ADD COLUMN c INT, ALTER c SET DEFAULT 7;" \
    "$DATABASE"

rollback_expected=$(cat <<\EXPECTED
a	1
b	2
EXPECTED
)
expect_error \
    "unknown later default action rolls back earlier default change" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'rollback_defaults'" \
    "CREATE TABLE rollback_defaults (id INT PRIMARY KEY, a INT DEFAULT 1, b INT DEFAULT 2); "\
"ALTER TABLE rollback_defaults ALTER a SET DEFAULT 9, ALTER missing SET DEFAULT 2;" \
    "$DATABASE"

expect_output \
    "rolled back default metadata after failed later action" \
    "$rollback_expected" \
    "SELECT COLUMN_NAME, COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'rollback_defaults' "\
"AND COLUMN_NAME IN ('a', 'b') "\
"ORDER BY COLUMN_NAME;" \
    "$DATABASE"

physical_rollback_expected=$(cat <<\EXPECTED
0
EXPECTED
)
expect_error \
    "unknown later default action rolls back earlier index action" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'physical_rollback'" \
    "CREATE TABLE physical_rollback (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE physical_rollback ADD INDEX k_v(v), ALTER missing SET DEFAULT 2;" \
    "$DATABASE"

expect_output \
    "rolled back physical index after failed later default action" \
    "$physical_rollback_expected" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'physical_rollback' "\
"AND INDEX_NAME = 'k_v';" \
    "$DATABASE"
