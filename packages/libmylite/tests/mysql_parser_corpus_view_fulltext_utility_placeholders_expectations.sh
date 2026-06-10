#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_view_fulltext_utility_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_view_fulltext_utility_placeholders: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

expect_non_syntax_failure() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: command unexpectedly succeeded"
    fi
    case "$output" in
        *"ERROR 1064 (42000)"*) fail "$label: got syntax error [$output]" ;;
        *) ;;
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
"CREATE TABLE t1 (a INT, b INT, title TEXT, body TEXT, FULLTEXT KEY ft_title_body(title, body)); "\
"CREATE TABLE t2 (a INT, b INT); "\
"CREATE TABLE t_name (name TEXT, FULLTEXT KEY ft_name(name)); "\
"INSERT INTO t1 VALUES (1, 10, 'mysql parser', 'mysql fulltext parser'), "\
"(2, 20, 'sqlite layer', 'embedded storage'); "\
"INSERT INTO t2 VALUES (1, 100), (2, 200); "\
"INSERT INTO t_name VALUES ('mysql parser');" >/dev/null

expect_success \
    "complex create and alter view bodies accepted" \
    "USE ${DATABASE}; "\
"CREATE ALGORITHM=TEMPTABLE VIEW v_join AS "\
"SELECT t1.a, t2.b FROM (t1 JOIN t2 ON t1.a = t2.a); "\
"CREATE VIEW v_expr AS SELECT 1 AS c; "\
"ALTER VIEW v_expr AS SELECT LPAD('x', 1 NOT IN (0), 1) AS c; "\
"DROP VIEW v_join, v_expr;" \
    "$DATABASE"

expect_output \
    "fulltext shorthand score matches parenthesized form" \
    "1" \
    "USE ${DATABASE}; SELECT "\
"MATCH(title, body) AGAINST ('+mysql*' IN BOOLEAN MODE) = "\
"MATCH title, body AGAINST ('+mysql*' IN BOOLEAN MODE) "\
"FROM t1 WHERE a = 1;"

expect_output \
    "fulltext shorthand accepts keyword-like column name" \
    "1" \
    "USE ${DATABASE}; SELECT "\
"MATCH(name) AGAINST ('mysql' IN BOOLEAN MODE) = "\
"MATCH name AGAINST ('mysql' IN BOOLEAN MODE) "\
"FROM t_name;"

expect_success \
    "help statement accepted" \
    "HELP no_such_mylite_topic;"

expect_success \
    "backup lock statements accepted" \
    "LOCK INSTANCE FOR BACKUP; UNLOCK INSTANCE;"

expect_non_syntax_failure \
    "select into outfile parses before file restrictions" \
    "USE ${DATABASE}; SELECT a,b INTO OUTFILE '/tmp/mylite-parser-outfile.txt' FROM t1;"

expect_non_syntax_failure \
    "select into dumpfile parses before file restrictions" \
    "USE ${DATABASE}; SELECT title INTO DUMPFILE '/tmp/mylite-parser-dumpfile.txt' FROM t1 LIMIT 1;"

expect_non_syntax_failure \
    "load xml parses before file restrictions" \
    "USE ${DATABASE}; LOAD XML INFILE '/tmp/mylite-parser-rows.xml' "\
"INTO TABLE t2 ROWS IDENTIFIED BY '<row>';"

expect_non_syntax_failure \
    "import table parses before file restrictions" \
    "USE ${DATABASE}; IMPORT TABLE FROM '/tmp/mylite-parser-no-such.sdi';"

expect_non_syntax_failure \
    "discard partition tablespace parses before table resolution" \
    "USE ${DATABASE}; ALTER TABLE no_such_table DISCARD PARTITION p0 TABLESPACE;"

expect_non_syntax_failure \
    "import partition tablespace parses before table resolution" \
    "USE ${DATABASE}; ALTER TABLE no_such_table IMPORT PARTITION p0 TABLESPACE;"

cleanup

printf '%s\n' "mysql_parser_corpus_view_fulltext_utility_placeholders: ok"
