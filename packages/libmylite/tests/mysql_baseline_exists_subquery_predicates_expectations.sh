#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_exists_subquery_predicates_$$"

fail() {
    printf '%s\n' "mysql_baseline_exists_subquery_predicates_expectations: $1" >&2
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

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
    fi
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
    "CREATE TABLE users (
         id INT PRIMARY KEY,
         name VARCHAR(20),
         group_id INT NULL
     );
     CREATE TABLE orders (
         id INT PRIMARY KEY,
         user_id INT NULL,
         status VARCHAR(20),
         total INT
     );
     CREATE TABLE empty_orders (
         id INT PRIMARY KEY
     );
     INSERT INTO users VALUES
         (1, 'Ann', 1),
         (2, 'Bob', 2),
         (3, 'Cat', 99),
         (4, 'Don', NULL);
     INSERT INTO orders VALUES
         (10, 1, 'open', 50),
         (11, 1, 'closed', 20),
         (12, 2, 'open', 30),
         (13, NULL, 'open', 99);" \
    "$DATABASE" >/dev/null

expect_output \
    "uncorrelated exists over nonempty table" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE EXISTS (SELECT * FROM orders);" \
    "$DATABASE"

expect_output \
    "not exists over nonempty table" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE NOT EXISTS (SELECT * FROM orders);" \
    "$DATABASE"

expect_output \
    "not exists over empty table" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE NOT EXISTS (SELECT * FROM empty_orders);" \
    "$DATABASE"

expect_output \
    "correlated equality exists" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT NULL FROM orders AS o WHERE o.user_id = u.id
     );" \
    "$DATABASE"

expect_output \
    "correlated equality not exists" \
    "3,4" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE NOT EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id = u.id
     );" \
    "$DATABASE"

expect_output \
    "correlated equality with inner literal predicate" \
    "1" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT o.user_id, o.status
         FROM orders AS o
         WHERE o.user_id = u.id AND o.status = 'closed'
     );" \
    "$DATABASE"

expect_output \
    "correlated null safe equality" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id <=> u.group_id
     );" \
    "$DATABASE"

expect_output \
    "limit zero makes exists false" \
    "NULL" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id = u.id LIMIT 0
     );" \
    "$DATABASE"

expect_output \
    "limit zero with literal predicate makes exists false" \
    "NULL" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.status = 'closed' LIMIT 0
     );" \
    "$DATABASE"

expect_output \
    "limit one keeps exists true for matching rows" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id = u.id LIMIT 1
     );" \
    "$DATABASE"

expect_output \
    "large limit keeps exists true for matching rows" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id = u.id LIMIT 99
     );" \
    "$DATABASE"

expect_output \
    "tableless exists is true" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (SELECT 1);" \
    "$DATABASE"

expect_output \
    "dual exists is true" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (SELECT * FROM DUAL);" \
    "$DATABASE"

expect_output \
    "inner unqualified names resolve before outer names" \
    "NULL" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE id = u.id
     );" \
    "$DATABASE"

expect_output \
    "schema qualified outer and inner tables" \
    "1
2
3
4" \
    "SELECT id
     FROM ${DATABASE}.users
     WHERE EXISTS (SELECT 1 FROM ${DATABASE}.orders);"

expect_error \
    "missing default schema for unqualified exists" \
    1046 \
    "3D000" \
    "No database selected" \
    "SELECT id FROM users WHERE EXISTS (SELECT 1 FROM orders);"

expect_error \
    "unknown inner table" \
    1146 \
    "42S02" \
    "Table '${DATABASE}.missing_orders' doesn't exist" \
    "SELECT id
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM missing_orders AS m WHERE m.user_id = u.id
     );" \
    "$DATABASE"

expect_error \
    "unknown inner selected column" \
    1054 \
    "42S22" \
    "Unknown column 'missing_value' in 'field list'" \
    "SELECT id
     FROM users AS u
     WHERE EXISTS (
         SELECT missing_value FROM orders AS o WHERE o.user_id = u.id
     );" \
    "$DATABASE"

expect_error \
    "unknown inner predicate column" \
    1054 \
    "42S22" \
    "Unknown column 'o.missing_value' in 'where clause'" \
    "SELECT id
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.missing_value = u.id
     );" \
    "$DATABASE"

expect_error \
    "unknown outer correlated column" \
    1054 \
    "42S22" \
    "Unknown column 'u.missing_value' in 'where clause'" \
    "SELECT id
     FROM users AS u
     WHERE EXISTS (
         SELECT 1 FROM orders AS o WHERE o.user_id = u.missing_value
     );" \
    "$DATABASE"

expect_accepts \
    "mysql accepts exists scalar projection outside MyLite slice" \
    "SELECT EXISTS (SELECT 1 FROM orders);" \
    "$DATABASE"

expect_accepts \
    "mysql accepts update where exists outside MyLite slice" \
    "UPDATE users
     SET group_id = 0
     WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = users.id);" \
    "$DATABASE"

printf '%s\n' "baseline-exists-subquery-predicates MySQL 8.4.9 expectations verified"
