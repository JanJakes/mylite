#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_reduced_parser_retry_$$"

fail() {
    printf '%s\n' "mysql_reduced_parser_retry_recognizer_expectations: $1" >&2
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
run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE types(type INT, a INT, b INT, c INT) ENGINE=MEMORY;
     CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE t2(id INT PRIMARY KEY) ENGINE=InnoDB;
     INSERT INTO types VALUES (7,1,1,1);
     INSERT INTO t1 VALUES (1);
     INSERT INTO t2 VALUES (1);" \
    >/dev/null

expect_output \
    "tableless limit forms" \
    "1
1" \
    "SELECT 0 LIMIT 0;
     SELECT 1 LIMIT 1;
     SELECT 1 LIMIT 1 OFFSET 0;"

expect_output \
    "result option before distinct" \
    "7" \
    "USE ${DATABASE}; SELECT SQL_BIG_RESULT DISTINCT type FROM types;"

expect_output \
    "row constructor forms" \
    "1	1	1" \
    "SELECT ROW(1,2)=ROW(1,2), (1,2)=(1,2), (1,2) IN ((1,2),(3,4));"

expect_output \
    "repeated locking clauses" \
    "1	1" \
    "USE ${DATABASE};
     START TRANSACTION;
     SELECT t1.id,t2.id FROM t1 JOIN t2 ON t1.id=t2.id
       FOR SHARE OF t1 FOR UPDATE OF t2;
     ROLLBACK;"

run_mysql \
    "USE ${DATABASE};
     CREATE INDEX i_prefix TYPE HASH ON types(a);
     CREATE INDEX i_suffix ON types(b) TYPE BTREE;
     CREATE INDEX i_both TYPE HASH ON types(c) TYPE BTREE;" \
    >/dev/null

expect_output \
    "legacy index type positions and precedence" \
    "i_both	BTREE
i_prefix	HASH
i_suffix	BTREE" \
    "SELECT INDEX_NAME, INDEX_TYPE
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='types'
      ORDER BY INDEX_NAME;"

expect_output \
    "type remains a nonreserved identifier" \
    "7" \
    "USE ${DATABASE}; SELECT type FROM types LIMIT 1;"

cleanup

printf '%s\n' "mysql_reduced_parser_retry_recognizer_expectations: ok"
