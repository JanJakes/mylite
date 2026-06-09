#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_expression_values_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_expression_value_surfaces_expectations: $1" >&2
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
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
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
"CREATE TABLE numbers (id INT PRIMARY KEY, amount INT, region VARCHAR(16)); "\
"INSERT INTO numbers VALUES (1, 10, 'east'), (2, 20, 'west'); "\
"CREATE TABLE bits (a BIGINT UNSIGNED, b BIGINT UNSIGNED); "\
"CREATE TABLE geo (g GEOMETRY); "\
"CREATE TABLE articles ("\
"title TEXT, body TEXT, FULLTEXT KEY ft_title (title), FULLTEXT KEY ft_body (title, body)); "\
"INSERT INTO articles VALUES ('database engine', 'mysql compatibility'), "\
"('plain title', 'unrelated body');" >/dev/null

expect_output \
    "SET user variable spatial expression" \
    "POINT(1 1)" \
    "USE ${DATABASE}; SET @a = ST_AsText(ST_GeomFromText('POINT(1 1)')); SELECT @a;"
expect_output \
    "SET user variable arithmetic function expression" \
    "3" \
    "USE ${DATABASE}; SET @a := LOG10(100.0) + 1; SELECT CAST(@a AS DECIMAL(10,0));"
expect_output \
    "SET user variable convert expression" \
    "abc" \
    "USE ${DATABASE}; SET @b = 'abc'; SET @a = CONVERT(@b USING utf8mb4); SELECT @a;"
expect_output \
    "SET user variable scalar subquery" \
    "2" \
    "USE ${DATABASE}; SET @a = (SELECT COUNT(*) FROM numbers); SELECT @a;"
expect_output \
    "SET timestamp expression" \
    "2019-03-11 12:00:00" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET TIMESTAMP = UNIX_TIMESTAMP('2019-03-11 12:00:00'); SELECT NOW();"

expect_output \
    "decimal comparison predicates" \
    "1	1	2" \
    "USE ${DATABASE}; "\
"SELECT "\
"(SELECT COUNT(*) FROM numbers WHERE amount = 10.0), "\
"(SELECT COUNT(*) FROM numbers WHERE amount BETWEEN 9.5 AND 10.5), "\
"(SELECT COUNT(*) FROM numbers WHERE amount IN (10.0, 20.0));"

expect_output \
    "DML bitwise expressions" \
    "18446744073709551615	18446744073709551615" \
    "USE ${DATABASE}; INSERT INTO bits VALUES (~0, -1 | 0); SELECT a, b FROM bits;"
expect_success \
    "DML spatial function values" \
    "USE ${DATABASE}; INSERT INTO geo VALUES "\
"(ST_GeomFromText('POINT(10 10)')), ((ST_PointFromText('POINT(2 2)'))), (POINT(1, 1));"

expect_output \
    "fulltext boolean modifier" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM articles "\
"WHERE MATCH(title, body) AGAINST ('+database' IN BOOLEAN MODE) > 0;"
expect_success \
    "fulltext natural language and query expansion modifiers" \
    "USE ${DATABASE}; "\
"SELECT MATCH(title) AGAINST ('database' IN NATURAL LANGUAGE MODE) FROM articles; "\
"SELECT MATCH(title) AGAINST ('database' WITH QUERY EXPANSION) FROM articles; "\
"SELECT MATCH(title) AGAINST ('database' IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION) "\
"FROM articles;"

expect_output \
    "GROUP BY WITH ROLLUP row count" \
    "3" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM "\
"(SELECT region, SUM(amount) FROM numbers GROUP BY region WITH ROLLUP) AS rolled;"

cleanup

printf '%s\n' "mysql_parser_corpus_expression_value_surfaces_expectations: ok"
