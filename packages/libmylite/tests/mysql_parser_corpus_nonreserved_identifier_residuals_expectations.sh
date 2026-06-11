#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_nonreserved_identifiers_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_nonreserved_identifier_residuals: $1" >&2
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

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2

    if ! run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected success"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "keyword reservation state" \
    "CURRENT	0
DIAGNOSTICS	0
LOCKED	0
NOWAIT	0
NUMBER	0
RETURNED_SQLSTATE	0
SKIP	0
STACKED	0
USER	0" \
    "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS "\
"WHERE WORD IN ('CURRENT','DIAGNOSTICS','NUMBER','RETURNED_SQLSTATE',"\
"'STACKED','USER','SKIP','LOCKED','NOWAIT') ORDER BY WORD;"

expect_success \
    "diagnostics identifier table" \
    "USE ${DATABASE}; CREATE TABLE t1 ("\
"current INT, diagnostics INT, number INT, returned_sqlstate INT);"
expect_output \
    "diagnostics identifier projection" \
    "1	2	3	4" \
    "USE ${DATABASE}; INSERT INTO t1 "\
"(current, diagnostics, number, returned_sqlstate) VALUES (1,2,3,4); "\
"SELECT current, diagnostics, number, returned_sqlstate FROM t1 WHERE number = 3;"

expect_success \
    "locking keyword columns" \
    "USE ${DATABASE}; CREATE TABLE t0 (skip INT, locked INT, nowait INT);"
expect_success \
    "stored diagnostics keyword columns" \
    "USE ${DATABASE}; CREATE TABLE diag_non_reserved ("\
"diagnostics INT, current INT, stacked INT, exception INT);"
expect_success \
    "session user table name" \
    "USE ${DATABASE}; CREATE TABLE SESSION_USER(a INT);"
expect_success \
    "system user table name" \
    "USE ${DATABASE}; CREATE TABLE SYSTEM_USER(a INT);"

expect_output \
    "plural optimize table targets" \
    "${DATABASE}.columns_priv	optimize	note	Table does not support optimize, doing recreate + analyze instead
${DATABASE}.columns_priv	optimize	status	OK
${DATABASE}.db	optimize	note	Table does not support optimize, doing recreate + analyze instead
${DATABASE}.db	optimize	status	OK
${DATABASE}.user	optimize	note	Table does not support optimize, doing recreate + analyze instead
${DATABASE}.user	optimize	status	OK" \
    "USE ${DATABASE}; CREATE TABLE columns_priv(a INT); CREATE TABLE db(a INT); "\
"CREATE TABLE user(a INT); OPTIMIZE TABLES columns_priv, db, user;"

cleanup

printf '%s\n' "mysql_parser_corpus_nonreserved_identifier_residuals: ok"
