#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ctas_surfaces_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_create_table_select_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE source1 (id INT, name VARCHAR(16)); "\
"CREATE TABLE source2 (id INT, name VARCHAR(16)); "\
"INSERT INTO source1 VALUES (1, 'one'); "\
"INSERT INTO source2 VALUES (2, 'two');" >/dev/null

expect_output \
    "explicit destination columns CTAS" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE explicit_cols (id INT, KEY (id)) SELECT id FROM source1; "\
"SELECT COUNT(*) FROM explicit_cols;"

expect_output \
    "compound CTAS" \
    "1
2" \
    "USE ${DATABASE}; "\
"CREATE TABLE compound_source AS SELECT id FROM source1 UNION SELECT id FROM source2; "\
"SELECT id FROM compound_source ORDER BY id;"

expect_output \
    "parenthesized query CTAS" \
    "1
2" \
    "USE ${DATABASE}; "\
"CREATE TABLE parenthesized_source AS (SELECT id FROM source1) UNION (SELECT id FROM source2); "\
"SELECT id FROM parenthesized_source ORDER BY id;"

expect_output \
    "cte CTAS" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE cte_source WITH cte AS (SELECT id FROM source1) SELECT id FROM cte; "\
"SELECT COUNT(*) FROM cte_source;"

expect_output \
    "TABLE CTAS" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE table_source AS TABLE source1; "\
"SELECT COUNT(*) FROM table_source;"

expect_output \
    "VALUES CTAS" \
    "2" \
    "USE ${DATABASE}; "\
"CREATE TABLE values_source AS VALUES ROW(1), ROW(2); "\
"SELECT COUNT(*) FROM values_source;"

expect_output \
    "WITH TABLE CTAS" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE with_table_source WITH cte AS (SELECT id FROM source1) TABLE cte; "\
"SELECT COUNT(*) FROM with_table_source;"

expect_output \
    "WITH VALUES CTAS" \
    "2" \
    "USE ${DATABASE}; "\
"CREATE TABLE with_values_source WITH cte AS (SELECT id FROM source1) VALUES ROW(1), ROW(2); "\
"SELECT COUNT(*) FROM with_values_source;"

expect_output \
    "partitioned CTAS" \
    "1" \
    "USE ${DATABASE}; "\
"CREATE TABLE partition_source (id INT) PARTITION BY HASH (id) AS SELECT id FROM source1; "\
"SELECT COUNT(*) FROM partition_source;"

cleanup

printf '%s\n' "mysql_parser_corpus_create_table_select_surfaces_expectations: ok"
