#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_query_expression_clause_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_query_expression_clause_surfaces_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t1 (a INT, b INT, c VARCHAR(20), j JSON, col_varchar_10 VARCHAR(10)); "\
"CREATE TABLE t_tuple (a INT, b INT, c VARCHAR(20)); "\
"CREATE TABLE t_dml (a INT, b INT, c VARCHAR(20)); "\
"CREATE TABLE t_dml_in (a INT, b INT, c VARCHAR(20)); "\
"CREATE TABLE t2 (a INT, b INT, c2 DATE, c3 TIME, c4 TIMESTAMP NULL, "\
"pk INT, col_int_key INT, col_varchar_10_key VARCHAR(10)); "\
"CREATE TABLE t (u INT); "\
"CREATE TABLE t_dates (f1 DATE, f2 DATETIME, f3 DATE, a DATETIME, "\
"value DECIMAL(30,0)); "\
"CREATE TABLE ft (x TEXT, FULLTEXT KEY ft_x (x)) ENGINE=InnoDB; "\
"CREATE TABLE v1 (f1 DATE); "\
"INSERT INTO t1 VALUES (1,2,'x','{\"id\":5,\"name\":\"James\"}','k1'),"\
"(3,4,'y','{\"id\":7,\"name\":\"james\"}','k2'); "\
"INSERT INTO t_tuple VALUES (1,2,'x'),(1,NULL,'n'),(3,4,'y'); "\
"INSERT INTO t_dml VALUES (1,2,'x'),(3,4,'y'),(5,6,'z'); "\
"INSERT INTO t_dml_in VALUES (1,2,'x'),(3,4,'y'),(5,6,'z'); "\
"INSERT INTO t2 VALUES (1,20,'2014-01-03','01:01:03','2014-01-03 01:01:01',"\
"1,10,'k1'),"\
"(3,40,'2014-02-01','02:00:00','2014-02-01 01:01:01',0,20,'k2'); "\
"INSERT INTO t VALUES (256),(257),(NULL); "\
"INSERT INTO t_dates VALUES "\
"('2001-01-01','2001-04-10 12:34:56','2001-05-01',"\
"'2010-02-01 09:31:02',100000000000000000000002),"\
"('2001-01-01','2001-03-01 00:00:00','2001-03-20',"\
"'2010-02-02 00:00:00',5); "\
"INSERT INTO v1 VALUES ('2005-02-02'); "\
"INSERT INTO ft VALUES ('abc one'),('def two');" >/dev/null

expect_output \
    "arithmetic predicate" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE a + 1 > 1;"

expect_output \
    "nested arithmetic predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ((a + 1) * 2) > 4;"

expect_output \
    "function order key" \
    "3
1" \
    "USE ${DATABASE}; SELECT a FROM t1 ORDER BY ABS(b - 5);"

expect_output \
    "expression group having order" \
    "1	1
3	1" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT a, COUNT(*) FROM t1 GROUP BY a + 0 "\
"HAVING COUNT(*) >= 1 AND a > 0 ORDER BY a + 0;"

expect_output \
    "row tuple comparison" \
    "1" \
    "USE ${DATABASE}; SELECT a FROM t1 WHERE (a,b) = (1,2);"

expect_output \
    "row tuple null-safe comparison" \
    "0" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE(a,b) <=> (1,NULL);"

expect_output \
    "postfix IS predicate" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u=256 IS NOT NULL;"

expect_output \
    "postfix IS UNKNOWN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u=256 IS UNKNOWN;"

expect_output \
    "postfix IS NULL predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u=256 IS NULL;"

expect_output \
    "postfix IS NOT UNKNOWN predicate" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u=256 IS NOT UNKNOWN;"

expect_output \
    "postfix range IS UNKNOWN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u > 256 IS UNKNOWN;"

expect_output \
    "postfix null-safe IS NOT UNKNOWN predicate" \
    "3" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t WHERE u <=> NULL IS NOT UNKNOWN;"

expect_output \
    "postfix IS UPDATE predicate" \
    "1" \
    "USE ${DATABASE}; UPDATE t SET u = 300 WHERE u=256 IS UNKNOWN; "\
"SELECT COUNT(*) FROM t WHERE u = 300;"

expect_output \
    "postfix IS DELETE predicate" \
    "3" \
    "USE ${DATABASE}; INSERT INTO t VALUES (NULL); "\
"DELETE FROM t WHERE u=256 IS UNKNOWN; SELECT COUNT(*) FROM t;"

expect_output \
    "JSON arrow predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE j->\"$.id\" = 5;"

expect_output \
    "JSON unquote arrow predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE j->>\"$.name\" = \"James\";"

expect_output \
    "ODBC scalar escape" \
    "ab" \
    "USE ${DATABASE}; SELECT {fn CONCAT('a','b')};"

expect_output \
    "column BETWEEN column bounds" \
    "0" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a "\
"WHERE t1.a BETWEEN t2.b AND t1.b;"

expect_output \
    "descriptor IN expression list" \
    "2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a "\
"WHERE t1.a IN(t2.a, t2.b);"

expect_output \
    "row constructor comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')=ROW(a,b,c);"

expect_output \
    "row constructor null-safe comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')<=>ROW(a,b,c);"

expect_output \
    "row constructor inequality comparison" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')<>ROW(a,b,c);"

expect_output \
    "row constructor bang inequality comparison" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')!=ROW(a,b,c);"

expect_output \
    "row constructor NULL inequality comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<>ROW(a,b,c);"

expect_output \
    "row constructor null-safe NULL comparison" \
    "0" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<=>ROW(a,b,c);"

expect_output \
    "parenthesized row tuple inequality comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE (a,b) <> (1,2);"

expect_output \
    "row constructor greater comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(a,b) > ROW(1,2);"

expect_output \
    "row constructor greater-equal comparison" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(a,b) >= ROW(1,2);"

expect_output \
    "row constructor less comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(a,b) < ROW(3,4);"

expect_output \
    "parenthesized row tuple less-equal comparison" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE (a,b) <= (3,4);"

expect_output \
    "literal-left row constructor order comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,3) > ROW(a,b);"

expect_output \
    "row constructor NULL null-safe comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <=> ROW(1,NULL);"

expect_output \
    "row constructor NULL equality comparison" \
    "0" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) = ROW(1,NULL);"

expect_output \
    "row constructor NULL inequality comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <> ROW(1,NULL);"

expect_output \
    "row constructor NULL order comparison" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) > ROW(1,2);"

expect_output \
    "row constructor NULL inclusive order comparison" \
    "0" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE ROW(a,b) <= ROW(1,NULL);"

expect_output \
    "parenthesized row tuple IN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2),(9,9));"

expect_output \
    "row constructor IN predicate" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(a,b) IN (ROW(1,2), ROW(3,4));"

expect_output \
    "parenthesized row tuple NOT IN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE (a,b) NOT IN ((1,2),(9,9));"

expect_output \
    "literal-left row constructor IN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2) IN (ROW(a,b));"

expect_output \
    "row constructor NULL IN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE "\
"ROW(a,b) IN (ROW(1,NULL), ROW(3,4));"

expect_output \
    "row constructor NULL NOT IN predicate" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE "\
"ROW(a,b) NOT IN (ROW(1,NULL), ROW(9,9));"

expect_output \
    "parenthesized row tuple NULL NOT IN filtering predicate" \
    "0" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t_tuple WHERE (a,b) NOT IN ((1,NULL),(3,4));"

expect_output \
    "row constructor IN UPDATE predicate" \
    "2" \
    "USE ${DATABASE}; UPDATE t_dml_in SET c = 'tuple-in' "\
"WHERE (a,b) IN ((1,2),(5,6)); SELECT COUNT(*) FROM t_dml_in WHERE c = 'tuple-in';"

expect_output \
    "row constructor NOT IN DELETE predicate" \
    "2" \
    "USE ${DATABASE}; DELETE FROM t_dml_in WHERE "\
"ROW(a,b) NOT IN (ROW(3,4), ROW(5,6)); SELECT COUNT(*) FROM t_dml_in;"

expect_output \
    "row constructor UPDATE predicate" \
    "2" \
    "USE ${DATABASE}; UPDATE t_dml SET c = 'hit' WHERE (a,b) >= (3,4); "\
"SELECT COUNT(*) FROM t_dml WHERE c = 'hit';"

expect_output \
    "row constructor DELETE predicate" \
    "2" \
    "USE ${DATABASE}; DELETE FROM t_dml WHERE ROW(a,b) < ROW(3,4); "\
"SELECT COUNT(*) FROM t_dml;"

expect_error \
    "row constructor arity mismatch predicate" \
    1241 \
    21000 \
    "Operand should contain 2 column(s)" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE ROW(1,2)=ROW(a,b,c);"

expect_error \
    "row constructor IN arity mismatch predicate" \
    1241 \
    21000 \
    "Operand should contain 2 column(s)" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2,3));"

expect_output \
    "parenthesized fulltext match" \
    "abc one" \
    "USE ${DATABASE}; "\
"SELECT x FROM ft GROUP BY x, MATCH(x) AGAINST ('abc') "\
"HAVING MATCH(x) AGAINST ('abc') ORDER BY x;"

expect_output \
    "ODBC date escape BETWEEN" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a "\
"WHERE c2 BETWEEN {d'2014-01-01'} AND {d'2014-01-05'};"

expect_output \
    "ODBC time escape BETWEEN" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a "\
"WHERE c3 BETWEEN {t'01:01:01'} AND {t'01:01:05'};"

expect_output \
    "ODBC timestamp escape BETWEEN" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a "\
"WHERE c4 BETWEEN {ts'2014-01-01 01:01:01'} AND {ts'2014-01-05 01:01:01'};"

expect_output \
    "VALUES string order key" \
    "1
2" \
    "VALUES ROW(1),ROW(2) ORDER BY '1' DESC;"

expect_output \
    "subquery expression predicate" \
    "1
3" \
    "USE ${DATABASE}; "\
"SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20) ORDER BY a;"

expect_output \
    "subquery nested expression predicate" \
    "1
3" \
    "USE ${DATABASE}; "\
"SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE ((b + 1) * 2) > 40) ORDER BY a;"

expect_output \
    "string literal left BETWEEN datetime and string" \
    "2001-04-10 12:34:56
2001-03-01 00:00:00" \
    "USE ${DATABASE}; "\
"SELECT f2 FROM t_dates "\
"WHERE '2001-04-10 12:34:56' BETWEEN f2 AND '01-05-01';"

expect_output \
    "string literal left NOT BETWEEN datetime and string" \
    "0" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates "\
"WHERE '2001-04-10 12:34:56' NOT BETWEEN f2 AND '01-05-01';"

expect_output \
    "string literal left BETWEEN descriptor bounds" \
    "2001-03-01 00:00:00	2001-03-20" \
    "USE ${DATABASE}; "\
"SELECT f2, f3 FROM t_dates WHERE '01-03-10' BETWEEN f2 AND f3;"

expect_output \
    "string literal left IN descriptor list" \
    "4" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates,t2 WHERE '01-01-01' IN (f1, '01-02-03');"

expect_output \
    "string literal left NOT IN descriptor list" \
    "0" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates,t2 WHERE '01-01-01' NOT IN (f1, '01-02-03');"

expect_output \
    "string literal left equality predicate" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '100000000000000000000002' = value;"

expect_output \
    "string literal left less-or-equal predicate" \
    "2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' <= a;"

expect_output \
    "string literal left less-than predicate" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' < a;"

expect_output \
    "string literal left greater-or-equal predicate" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '2010-02-01 09:31:02.0' >= a;"

expect_output \
    "string literal left greater-than predicate" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '2010-02-02 00:00:00.0' > a;"

expect_output \
    "string literal left not-equal predicate" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t_dates WHERE '5' <> value;"

expect_output \
    "dotted date string equality against date column" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM v1 WHERE '2005.02.02'=f1;"

expect_output \
    "dotted date string null-safe equality against date column" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM v1 WHERE '2005.02.02'<=>f1;"

expect_output \
    "group by user-variable assignment" \
    "1
NULL" \
    "USE ${DATABASE}; SET @a := 1; SET @b := NULL; "\
"SELECT 1 FROM t_dates GROUP BY @b := @a, @b; SELECT @b;"

expect_error \
    "quoted missing function call parses before resolution" \
    1305 \
    42000 \
    "FUNCTION ${DATABASE}.foo does not exist" \
    "USE ${DATABASE}; SELECT \`foo\` ();"

expect_output \
    "distinct joined order with qualified bare truth predicate" \
    "10" \
    "USE ${DATABASE}; "\
"SELECT DISTINCT t2.col_int_key FROM t1 LEFT JOIN t2 "\
"ON t1.col_varchar_10 = t2.col_varchar_10_key "\
"WHERE t2.pk ORDER BY t2.col_int_key;"

expect_output \
    "multi-table update expression assignment" \
    "1	3
3	5" \
    "USE ${DATABASE}; "\
"UPDATE t1, t2 SET t1.b = t1.b + 1 WHERE t1.a = t2.a; "\
"SELECT a,b FROM t1 ORDER BY a;"

expect_output \
    "ordered delete expression predicate" \
    "1" \
    "USE ${DATABASE}; "\
"DELETE FROM t1 WHERE a = a + sleep(0) ORDER BY a LIMIT 1; "\
"SELECT COUNT(*) FROM t1;"

cleanup

printf '%s\n' "mysql_parser_corpus_query_expression_clause_surfaces_expectations: ok"
