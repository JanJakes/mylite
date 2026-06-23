#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_default_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_default_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names "$@"
        fi
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
run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"SET time_zone = '+00:00'; SET timestamp = 1700000000;" >/dev/null

run_mysql \
    "CREATE TABLE t ("\
"id INT NOT NULL DEFAULT 7, "\
"n INT NULL DEFAULT 8, "\
"nul INT NULL DEFAULT NULL, "\
"nn INT NOT NULL, "\
"s VARCHAR(10) DEFAULT 'abc', "\
"e VARCHAR(10) DEFAULT '', "\
"d DATE DEFAULT '2001-02-03', "\
"dt DATETIME DEFAULT '2001-02-03 04:05:06', "\
"ct DATETIME DEFAULT CURRENT_TIMESTAMP, "\
"ctn DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "\
"ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP, "\
"tsn TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "\
"cd DATE DEFAULT (CURDATE()), "\
"cti TIME DEFAULT (CURTIME()), "\
"ex INT DEFAULT (1 + 2)"\
"); "\
"INSERT INTO t(id, n, nul, nn, s, e, d, dt) "\
"VALUES(1, 10, NULL, 2, 'x', 'y', '2020-01-01', '2020-01-01 01:02:03');" \
    "$DATABASE" >/dev/null

expect_output \
    "select default function values and qualifiers" \
    "7	8	NULL	abc		2001-02-03	2001-02-03 04:05:06	NULL	0000-00-00 00:00:00	NULL	0000-00-00 00:00:00	8	8	0
8" \
    "DO 0; SELECT DEFAULT(id), DEFAULT(n), IFNULL(DEFAULT(nul), 'NULL'), DEFAULT(s), DEFAULT(e), "\
"DEFAULT(d), DEFAULT(dt), IFNULL(DEFAULT(ct), 'NULL'), DEFAULT(ctn), IFNULL(DEFAULT(ts), 'NULL'), "\
"DEFAULT(tsn), DEFAULT(t.n), DEFAULT(${DATABASE}.t.n), @@warning_count FROM t; "\
"SELECT DEFAULT(q.n) FROM t AS q;" \
    "$DATABASE"

expect_output \
    "insert values default function" \
    "1	0
7	8	abc" \
    "INSERT INTO t(id, n, nul, nn, s) VALUES(DEFAULT(id), DEFAULT(n), DEFAULT(nul), 9, DEFAULT(s)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, s FROM t WHERE nn = 9;" \
    "$DATABASE"

expect_output \
    "insert set default function" \
    "1	0
7	8	abc" \
    "INSERT INTO t SET id = DEFAULT(id), n = DEFAULT(n), nul = DEFAULT(nul), nn = 10, s = DEFAULT(s); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, s FROM t WHERE nn = 10;" \
    "$DATABASE"

expect_output \
    "replace values default function" \
    "1	0
7	8	abc" \
    "REPLACE INTO t(id, n, nul, nn, s) VALUES(DEFAULT(id), DEFAULT(n), DEFAULT(nul), 11, DEFAULT(s)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, s FROM t WHERE nn = 11;" \
    "$DATABASE"

expect_output \
    "replace set default function" \
    "1	0
7	8	abc" \
    "REPLACE INTO t SET id = DEFAULT(id), n = DEFAULT(n), nul = DEFAULT(nul), nn = 12, s = DEFAULT(s); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, s FROM t WHERE nn = 12;" \
    "$DATABASE"

expect_output \
    "update default function changed and no match" \
    "1	0	1	7	abc	NULL	NULL
0	0" \
    "UPDATE t SET n = DEFAULT(id), s = DEFAULT(s), ct = DEFAULT(ct), ts = DEFAULT(ts) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, id, n, s, IFNULL(ct, 'NULL'), IFNULL(ts, 'NULL') FROM t WHERE id = 1; "\
"UPDATE t SET n = DEFAULT(id) WHERE id = 999; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "update null target conversion only on matched rows" \
    "0" \
    "UPDATE t SET nn = DEFAULT(nul) WHERE id = 999; SELECT ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "matched null into not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "UPDATE t SET nn = DEFAULT(nul) WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "select expression default" \
    3773 \
    HY000 \
    "DEFAULT function cannot be used with default value expressions" \
    "SELECT DEFAULT(ex) FROM t;" \
    "$DATABASE"

expect_error \
    "select generated date default" \
    3773 \
    HY000 \
    "DEFAULT function cannot be used with default value expressions" \
    "SELECT DEFAULT(cd) FROM t;" \
    "$DATABASE"

expect_error \
    "select generated time default" \
    3773 \
    HY000 \
    "DEFAULT function cannot be used with default value expressions" \
    "SELECT DEFAULT(cti) FROM t;" \
    "$DATABASE"

expect_error \
    "select no explicit default" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "SELECT DEFAULT(nn) FROM t;" \
    "$DATABASE"

expect_error \
    "update no match validates source default" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "UPDATE t SET n = DEFAULT(nn) WHERE id = 999;" \
    "$DATABASE"

expect_error \
    "unknown no source" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "SELECT DEFAULT(n);" \
    "$DATABASE"

expect_error \
    "unknown qualified source" \
    1054 \
    42S22 \
    "Unknown column 'other.t.n' in 'field list'" \
    "SELECT DEFAULT(other.t.n) FROM t;" \
    "$DATABASE"

expect_error \
    "literal argument syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT DEFAULT(1) FROM t;" \
    "$DATABASE"

run_mysql "CREATE TABLE p(id INT PRIMARY KEY DEFAULT 1, n INT DEFAULT 8, s VARCHAR(10) DEFAULT 'abc'); INSERT INTO p VALUES(1, 5, 'x');" "$DATABASE" >/dev/null

expect_output \
    "duplicate update default function" \
    "2	0
1	8	abc" \
    "INSERT INTO p(id, n, s) VALUES(1, 9, 'y') ON DUPLICATE KEY UPDATE n = DEFAULT(n), s = DEFAULT(s); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, s FROM p;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_default_function_expectations: ok"
