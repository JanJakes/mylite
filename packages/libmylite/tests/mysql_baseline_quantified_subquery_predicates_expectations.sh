#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_quantified_subquery_predicates_$$"

fail() {
    printf '%s\n' "mysql_baseline_quantified_subquery_predicates_expectations: $1" >&2
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
    "CREATE TABLE outer_t(id INT PRIMARY KEY, v INT NULL, s VARCHAR(20) NULL);
     CREATE TABLE inner_t(v INT NULL, s VARCHAR(20) NULL);
     CREATE TABLE empty_t(v INT NULL, s VARCHAR(20) NULL);
     INSERT INTO outer_t VALUES
         (1, 1, 'ann'),
         (2, 2, 'BOB'),
         (3, 3, 'cat'),
         (4, NULL, NULL),
         (5, 5, 'eve');
     INSERT INTO inner_t VALUES (2, 'bob'), (3, 'CAT'), (NULL, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "equals any" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = ANY (SELECT v FROM inner_t);" \
    "$DATABASE"

expect_output \
    "equals some" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = SOME (SELECT v FROM inner_t);" \
    "$DATABASE"

expect_output \
    "greater than any" \
    "3,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v > ANY (SELECT v FROM inner_t);" \
    "$DATABASE"

expect_output \
    "less than all" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v < ALL (SELECT v FROM inner_t WHERE v IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "not equal all" \
    "1,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v <> ALL (SELECT v FROM inner_t WHERE v IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "all over empty subquery" \
    "1,2,3,4,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = ALL (SELECT v FROM empty_t);" \
    "$DATABASE"

expect_output \
    "any over empty subquery" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = ANY (SELECT v FROM empty_t);" \
    "$DATABASE"

expect_output \
    "string any uses default collation" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE s = ANY (SELECT s FROM inner_t);" \
    "$DATABASE"

expect_output \
    "string not equal all uses default collation" \
    "1,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE s <> ALL (SELECT s FROM inner_t WHERE s IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "any unknown suffix" \
    "1,2,3,4,5" \
     "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = ANY (SELECT v FROM inner_t WHERE v IS NULL) IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "not any preserves unknown" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE NOT (v = ANY (SELECT v FROM inner_t));" \
    "$DATABASE"

expect_output \
    "all with inner null filters direct where" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v <= ALL (SELECT v FROM inner_t);" \
    "$DATABASE"

expect_output \
    "all false takes precedence over unknown" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v <= ALL (SELECT v FROM inner_t) IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "any not unknown suffix" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v = ANY (SELECT v FROM inner_t) IS NOT UNKNOWN;" \
    "$DATABASE"

expect_output \
    "greater equal all" \
    "3,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id)
     FROM outer_t
     WHERE v >= ALL (SELECT v FROM inner_t WHERE v IS NOT NULL);" \
    "$DATABASE"

expect_output \
    "correlated integer any" \
    "2,3" \
    "SELECT GROUP_CONCAT(o.id ORDER BY o.id)
     FROM outer_t AS o
     WHERE o.v = ANY (SELECT i.v FROM inner_t AS i WHERE i.v = o.v);" \
    "$DATABASE"

expect_error \
    "multi-column quantified subquery" \
    1241 \
    21000 \
    "Operand should contain 1 column(s)" \
    "SELECT id FROM outer_t WHERE v = ANY (SELECT v, s FROM inner_t);" \
    "$DATABASE"

expect_error \
    "limit quantified subquery" \
    1235 \
    42000 \
    "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'" \
    "SELECT id FROM outer_t WHERE v = ANY (SELECT v FROM inner_t LIMIT 1);" \
    "$DATABASE"

expect_error \
    "null-safe quantified comparison syntax" \
    1064 \
    42000 \
    "syntax" \
    "SELECT id FROM outer_t WHERE v <=> ANY (SELECT v FROM inner_t);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_quantified_subquery_predicates_expectations: ok"
