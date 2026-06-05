#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_in_subquery_predicates_$$"

fail() {
    printf '%s\n' "mysql_baseline_in_subquery_predicates_expectations: $1" >&2
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
    "CREATE TABLE users (
         id INT PRIMARY KEY,
         name VARCHAR(20),
         group_id INT NULL
     );
     CREATE TABLE orders (
         id INT PRIMARY KEY,
         user_id INT NULL,
         status VARCHAR(20),
         group_id INT NULL
     );
     CREATE TABLE empty_orders (
         id INT PRIMARY KEY,
         user_id INT NULL
     );
     CREATE TABLE names (
         name VARCHAR(20)
     );
     CREATE TABLE selected_names (
         name VARCHAR(20)
     );
     CREATE TABLE single_values (
         user_id INT
     );
     INSERT INTO users VALUES
         (1, 'Ann', 1),
         (2, 'Bob', 2),
         (3, 'Cat', 99),
         (4, 'Don', NULL),
         (5, 'Eve', 3);
     INSERT INTO orders VALUES
         (10, 1, 'open', 1),
         (11, 1, 'closed', NULL),
         (12, 2, 'open', 2),
         (13, NULL, 'open', 2),
         (14, 5, 'closed', NULL);
     INSERT INTO names VALUES ('Ann'), ('bob'), ('CAT'), (NULL);
     INSERT INTO selected_names VALUES ('ann'), ('cat'), (NULL);
     INSERT INTO single_values VALUES (2), (3), (NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "integer in subquery matches non-null values" \
    "1,2,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT user_id FROM orders);" \
    "$DATABASE"

expect_output \
    "integer in distinct subquery matches non-null values" \
    "1,2,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT DISTINCT user_id FROM orders);" \
    "$DATABASE"

expect_output \
    "integer not in with inner null filters every nonmatch" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id NOT IN (SELECT user_id FROM orders);" \
    "$DATABASE"

expect_output \
    "integer not in without inner null keeps nonmatches" \
    "3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id NOT IN (SELECT user_id FROM orders WHERE user_id IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "in over empty subquery is false" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT user_id FROM empty_orders);" \
    "$DATABASE"

expect_output \
    "not in over empty subquery is true" \
    "1,2,3,4,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id NOT IN (SELECT user_id FROM empty_orders);" \
    "$DATABASE"

expect_output \
    "outer nullable value does not match ordinary in" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE group_id IN (SELECT group_id FROM orders);" \
    "$DATABASE"

expect_output \
    "outer nullable not in with inner null filters every nonmatch" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE group_id NOT IN (SELECT group_id FROM orders);" \
    "$DATABASE"

expect_output \
    "ascii string in subquery uses default case-insensitive collation" \
    "Ann,CAT" \
    "SELECT GROUP_CONCAT(name ORDER BY name)
     FROM names
     WHERE name IN (SELECT name FROM selected_names);" \
    "$DATABASE"

expect_output \
    "inner where filters membership source" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT user_id FROM orders WHERE status = 'open');" \
    "$DATABASE"

expect_output \
    "correlated equality filters inner source" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE u.id IN (
         SELECT o.user_id FROM orders AS o WHERE o.group_id = u.group_id
     );" \
    "$DATABASE"

expect_output \
    "correlated null-safe equality filters inner source" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE u.id IN (
         SELECT o.user_id FROM orders AS o WHERE o.group_id <=> u.group_id
     );" \
    "$DATABASE"

expect_output \
    "inner unqualified names prefer inner source" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users AS u
     WHERE u.id IN (
         SELECT user_id FROM orders AS o WHERE group_id = u.group_id
     );" \
    "$DATABASE"

expect_output \
    "schema qualified outer and inner sources" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM ${DATABASE}.users
     WHERE id IN (
         SELECT user_id FROM ${DATABASE}.orders WHERE status = 'open'
     );"

expect_output \
    "outer joined source with in subquery" \
    "1,2" \
    "SELECT GROUP_CONCAT(u.id ORDER BY u.id)
     FROM users u JOIN orders o ON u.id = o.user_id
     WHERE o.status = 'open'
       AND u.id IN (SELECT user_id FROM orders WHERE user_id IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "inner joined distinct in subquery" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (
         SELECT DISTINCT o.user_id
         FROM orders o JOIN users u2 ON o.user_id = u2.id
         WHERE u2.name IN ('Ann', 'Bob')
     );" \
    "$DATABASE"

expect_output \
    "mysql accepts inner order without visible membership effect" \
    "1,2,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT user_id FROM orders ORDER BY user_id DESC);" \
    "$DATABASE"

expect_output \
    "mysql accepts tableless in subquery" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT 1);" \
    "$DATABASE"

expect_output \
    "mysql accepts one-column wildcard in subquery" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM users
     WHERE id IN (SELECT * FROM single_values);" \
    "$DATABASE"

expect_error \
    "inner limit rejected by mysql" \
    1235 \
    42000 \
    "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'" \
    "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders LIMIT 0);" \
    "$DATABASE"

expect_error \
    "inner order with limit rejected by mysql limit diagnostic" \
    1235 \
    42000 \
    "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'" \
    "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders ORDER BY user_id LIMIT 1);" \
    "$DATABASE"

expect_error \
    "multi-column subquery rejected" \
    1241 \
    21000 \
    "Operand should contain 1 column(s)" \
    "SELECT id FROM users WHERE id IN (SELECT user_id, status FROM orders);" \
    "$DATABASE"

expect_error \
    "unknown selected inner column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT id FROM users WHERE id IN (SELECT missing FROM orders);" \
    "$DATABASE"

expect_error \
    "unknown inner where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders WHERE missing = 1);" \
    "$DATABASE"

expect_error \
    "unknown correlated outer column" \
    1054 \
    42S22 \
    "Unknown column 'u.missing' in 'where clause'" \
    "SELECT u.id FROM users AS u
     WHERE u.id IN (
         SELECT o.user_id FROM orders AS o WHERE o.group_id = u.missing
     );" \
    "$DATABASE"

expect_error \
    "unknown inner table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_orders' doesn't exist" \
    "SELECT id FROM users WHERE id IN (SELECT user_id FROM missing_orders);" \
    "$DATABASE"

printf '%s\n' "baseline-in-subquery-predicates MySQL 8.4.9 expectations verified"
