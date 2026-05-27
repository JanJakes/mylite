#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_subquery_predicates_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_subquery_predicates_expectations: $1" >&2
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
         group_id INT NULL,
         flag INT
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
         name VARCHAR(20),
         flag INT
     );
     CREATE TABLE selected_names (
         name VARCHAR(20)
     );
     INSERT INTO users VALUES
         (1, 'Ann', 1, 0),
         (2, 'Bob', 2, 0),
         (3, 'Cat', 99, 0),
         (4, 'Don', NULL, 0),
         (5, 'Eve', 3, 0);
     INSERT INTO orders VALUES
         (10, 1, 'open', 1),
         (11, 1, 'closed', NULL),
         (12, 2, 'open', 2),
         (13, NULL, 'open', 2),
         (14, 5, 'closed', NULL);
     INSERT INTO names VALUES ('Ann', 0), ('bob', 0), ('CAT', 0), (NULL, 0);
     INSERT INTO selected_names VALUES ('ann'), ('cat'), (NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "integer in subquery updates matched rows" \
    "3
1:7,2:7,3:0,4:0,5:7
0" \
    "UPDATE users SET flag = 7 WHERE id IN (SELECT user_id FROM orders);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;
     SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "inner where filters in membership" \
    "2
1:8,2:8,3:0,4:0,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 8 WHERE id IN (
         SELECT user_id FROM orders WHERE status = 'open'
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "not in with inner null filters nonmatches" \
    "0
1:0,2:0,3:0,4:0,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 3 WHERE id NOT IN (SELECT user_id FROM orders);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "not in without inner null updates nonmatches" \
    "2
1:0,2:0,3:4,4:4,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 4 WHERE id NOT IN (
         SELECT user_id FROM orders WHERE user_id IS NOT NULL
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "empty in subquery is false" \
    "0
1:0,2:0,3:0,4:0,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 5 WHERE id IN (SELECT user_id FROM empty_orders);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "uncorrelated exists updates all rows" \
    "5
1:6,2:6,3:6,4:6,5:6" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 6 WHERE EXISTS (SELECT 1 FROM orders);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "exists limit zero updates no rows" \
    "0
1:0,2:0,3:0,4:0,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 5 WHERE EXISTS (SELECT 1 FROM orders LIMIT 0);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "not exists over empty table updates all rows" \
    "5
1:9,2:9,3:9,4:9,5:9" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 9 WHERE NOT EXISTS (SELECT 1 FROM empty_orders);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "correlated in filters by target row" \
    "2
1:1,2:1,3:0,4:0,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 1 WHERE id IN (
         SELECT user_id FROM orders WHERE orders.group_id = users.group_id
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "correlated exists filters by target row" \
    "3
1:2,2:2,3:0,4:0,5:2" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 2 WHERE EXISTS (
         SELECT 1 FROM orders WHERE orders.user_id = users.id
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "correlated null safe exists matches null group" \
    "3
1:3,2:3,3:0,4:3,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 3 WHERE EXISTS (
         SELECT 1 FROM orders WHERE orders.group_id <=> users.group_id
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "outer order and limit update highest matched row" \
    "1
1:0,2:0,3:0,4:0,5:8" \
    "UPDATE users SET flag = 0;
     UPDATE users SET flag = 8
     WHERE id IN (SELECT user_id FROM orders)
     ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "schema qualified target and source" \
    "2
1:0,2:0,3:11,4:11,5:0" \
    "UPDATE users SET flag = 0;
     UPDATE ${DATABASE}.users SET flag = 11
     WHERE id NOT IN (
         SELECT user_id FROM ${DATABASE}.orders WHERE user_id IS NOT NULL
     );
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM users;" \
    "$DATABASE"

expect_output \
    "string in subquery uses default collation" \
    "2
Ann:1,bob:0,CAT:1" \
    "UPDATE names SET flag = 1 WHERE name IN (SELECT name FROM selected_names);
     SELECT ROW_COUNT();
     SELECT GROUP_CONCAT(CONCAT(name, ':', flag) ORDER BY name) FROM names
     WHERE name IS NOT NULL;" \
    "$DATABASE"

expect_error \
    "same target source in in subquery" \
    1093 \
    "HY000" \
    "You can't specify target table 'users' for update in FROM clause" \
    "UPDATE users SET flag = 5 WHERE id IN (SELECT id FROM users);" \
    "$DATABASE"

expect_error \
    "same target source in exists subquery" \
    1093 \
    "HY000" \
    "You can't specify target table 'users' for update in FROM clause" \
    "UPDATE users SET flag = 5 WHERE EXISTS (SELECT 1 FROM users);" \
    "$DATABASE"

expect_error \
    "inner in limit keeps mysql diagnostic" \
    1235 \
    "42000" \
    "LIMIT & IN/ALL/ANY/SOME subquery" \
    "UPDATE users SET flag = 5 WHERE id IN (SELECT user_id FROM orders LIMIT 1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_update_subquery_predicates_expectations: ok"
