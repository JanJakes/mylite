#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_scalar_subquery_assignment_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_scalar_subquery_assignment_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE target (
         id INT PRIMARY KEY,
         option_name VARCHAR(40),
         option_value VARCHAR(40) NULL,
         n INT NULL,
         nn INT NOT NULL DEFAULT 7
     );
     CREATE TABLE source (
         id INT PRIMARY KEY,
         option_name VARCHAR(40),
         option_value VARCHAR(40) NULL,
         n INT NULL
     );
     INSERT INTO target VALUES
         (1, 'User 0000018', 'old-18', 10, 7),
         (2, 'User 0000019', 'old-19', 20, 7),
         (3, 'User 0000020', NULL, 30, 7);
     INSERT INTO source VALUES
         (10, 'User 0000018', 'source-18', 100),
         (11, 'User 0000019', 'source-19', 200),
         (12, 'User 0000020', NULL, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "string scalar subquery assignment" \
    "1	0	1:old-18,2:source-19,3:NULL" \
    "UPDATE target
         SET option_value = (
             SELECT option_value FROM source WHERE option_name = 'User 0000019'
         )
         WHERE option_name = 'User 0000019';
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(option_value, 'NULL')) ORDER BY id)
     FROM target;" \
    "$DATABASE"

expect_output \
    "integer scalar subquery assignment" \
    "1	0	200" \
    "UPDATE target SET n = (SELECT n FROM source WHERE id = 11) WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, n FROM target WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "no-op scalar subquery changed rows" \
    "0	0" \
    "UPDATE target SET n = (SELECT n FROM source WHERE id = 11) WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "empty scalar subquery assigns null" \
    "1	0	NULL" \
    "UPDATE target SET option_value = (SELECT option_value FROM source WHERE id = 999)
         WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, IFNULL(option_value, 'NULL') FROM target WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "empty scalar subquery into not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "UPDATE target SET nn = (SELECT n FROM source WHERE id = 999) WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "multi-row scalar subquery" \
    1242 \
    21000 \
    "Subquery returns more than 1 row" \
    "UPDATE target SET option_value = (SELECT option_value FROM source) WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "no-match skips multi-row scalar evaluation" \
    "0	0" \
    "UPDATE target SET option_value = (SELECT option_value FROM source) WHERE id = 999;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "multi-column scalar subquery" \
    1241 \
    21000 \
    "Operand should contain 1 column(s)" \
    "UPDATE target SET option_value = (SELECT option_value, n FROM source WHERE id = 11)
         WHERE id = 999;" \
    "$DATABASE"

expect_error \
    "same-table scalar source" \
    1093 \
    HY000 \
    "You can't specify target table 'target' for update in FROM clause" \
    "UPDATE target SET option_value = (SELECT option_value FROM target WHERE id = 2)
         WHERE id = 999;" \
    "$DATABASE"

expect_output \
    "ordered limited scalar subquery" \
    "1	0	source-18" \
    "UPDATE target
         SET option_value = (SELECT option_value FROM source ORDER BY id ASC LIMIT 1)
         WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count, option_value FROM target WHERE id = 2;" \
    "$DATABASE"

expect_output \
    "offset limited scalar subquery" \
    "1	0	source-19" \
    "UPDATE target
         SET option_value = (SELECT option_value FROM source ORDER BY id ASC LIMIT 1, 1)
         WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count, option_value FROM target WHERE id = 2;" \
    "$DATABASE"

expect_output \
    "limit zero scalar subquery assigns null" \
    "1	0	NULL" \
    "UPDATE target
         SET option_value = (SELECT option_value FROM source ORDER BY id ASC LIMIT 0)
         WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count, IFNULL(option_value, 'NULL') FROM target WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "unknown source column" \
    1054 \
    42S22 \
    "Unknown column 'missing_value' in 'field list'" \
    "UPDATE target SET option_value = (SELECT missing_value FROM source WHERE id = 11)
         WHERE id = 999;" \
    "$DATABASE"

expect_error \
    "unknown source table" \
    1146 \
    42S02 \
    "doesn't exist" \
    "UPDATE target SET option_value = (SELECT option_value FROM missing_source WHERE id = 11)
         WHERE id = 999;" \
    "$DATABASE"

expect_error \
    "unknown source where column" \
    1054 \
    42S22 \
    "Unknown column 'missing_predicate' in 'where clause'" \
    "UPDATE target SET option_value = (
         SELECT option_value FROM source WHERE missing_predicate = 11
     ) WHERE id = 999;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_update_scalar_subquery_assignment_expectations: ok"
