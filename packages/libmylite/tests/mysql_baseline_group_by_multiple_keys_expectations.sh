#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_multiple_keys_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_multiple_keys_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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

run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(
       id INT NOT NULL,
       a INT NULL,
       b INT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, NULL, NULL, 1),
       (2, 1, 1, 'alice', 'A', 10),
       (3, 1, 1, 'Alice', 'A   ', 20),
       (4, 1, 2, 'alice', 'B', 30),
       (5, 2, 1, 'bob', 'B', 40),
       (6, 2, 1, 'BOB', 'B   ', NULL),
       (7, 2, 2, 'bob', 'C', 50),
       (8, 2, 2, 'carol', 'C', 60);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT a, b, COUNT(*) AS c, COUNT(n) AS cn, SUM(n) AS s, MIN(n) AS mn, MAX(n) AS mx,
            AVG(n) AS av
       FROM t GROUP BY a, b ORDER BY a, b;
     SELECT @@warning_count, ROW_COUNT();
     SELECT name, label, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY name, label ORDER BY name, label;"
)
expect_value "integer null tuple" "NULL	NULL	1	1	1	1	1	1.0000" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer one one tuple" "1	1	2	2	30	10	20	15.0000" \
    "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "integer one two tuple" "1	2	1	1	30	30	30	30.0000" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "integer two one tuple" "2	1	2	1	40	40	40	40.0000" \
    "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "integer two two tuple" "2	2	2	2	110	50	60	55.0000" \
    "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "multi key status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "string null tuple" "NULL	NULL	1	1" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "string alice a tuple" "alice	A	2	30" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "string alice b tuple" "alice	B	1	30" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "string bob b tuple" "bob	B	2	40" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "string bob c tuple" "bob	C	1	50" "$(printf '%s\n' "$core" | sed -n '11p')"
expect_value "string carol c tuple" "carol	C	1	60" "$(printf '%s\n' "$core" | sed -n '12p')"

filters=$(run_mysql \
    "USE ${DATABASE};
     SELECT a, b, COUNT(*) AS c, SUM(n) AS s
       FROM t WHERE id >= 2 GROUP BY a, b HAVING c > 1 ORDER BY a, b;
     SELECT a, b, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY a, b HAVING b = 1 ORDER BY a, b;
     SELECT name, label, COUNT(*) AS c
       FROM t GROUP BY name, label HAVING label IS NOT NULL ORDER BY name DESC LIMIT 1;"
)
expect_value "where having first" "1	1	2	30" "$(printf '%s\n' "$filters" | sed -n '1p')"
expect_value "where having second" "2	1	2	40" "$(printf '%s\n' "$filters" | sed -n '2p')"
expect_value "where having third" "2	2	2	110" "$(printf '%s\n' "$filters" | sed -n '3p')"
expect_value "having group key first" "1	1	2	30" "$(printf '%s\n' "$filters" | sed -n '4p')"
expect_value "having group key second" "2	1	2	40" "$(printf '%s\n' "$filters" | sed -n '5p')"
expect_value "string having/order first" "carol	C	1" "$(printf '%s\n' "$filters" | sed -n '6p')"

ordering=$(run_mysql \
    "USE ${DATABASE};
     SELECT a AS x, b AS y, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY a, b ORDER BY s DESC LIMIT 2;
     SELECT q.a AS x, q.b AS y, COUNT(*) AS c
       FROM ${DATABASE}.t AS q WHERE q.b = 1 GROUP BY q.a, q.b ORDER BY x DESC;"
)
expect_value "aggregate order first" "2	2	2	110" "$(printf '%s\n' "$ordering" | sed -n '1p')"
expect_value "aggregate order second" "2	1	2	40" "$(printf '%s\n' "$ordering" | sed -n '2p')"
expect_value "qualified alias order first" "2	1	2" "$(printf '%s\n' "$ordering" | sed -n '3p')"
expect_value "qualified alias order second" "1	1	2" "$(printf '%s\n' "$ordering" | sed -n '4p')"

headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT a AS x, b AS y, COUNT(*) AS c FROM t GROUP BY a, b ORDER BY x, y LIMIT 1;"
)
expect_value "header labels" "x	y	c" "$(printf '%s\n' "$headers" | sed -n '1p')"
expect_value "header row" "NULL	NULL	1" "$(printf '%s\n' "$headers" | sed -n '2p')"

expect_error \
    "non grouped selected column" \
    1055 \
    "42000" \
    "Expression #3 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE}; SELECT a, b, n, COUNT(*) FROM t GROUP BY a, b;"
expect_error \
    "missing selected group key" \
    1055 \
    "42000" \
    "Expression #2 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE}; SELECT a, b, COUNT(*) FROM t GROUP BY a;"
expect_error \
    "unknown group key" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT a, missing, COUNT(*) FROM t GROUP BY a, missing;"

cleanup

printf '%s\n' "mysql_baseline_group_by_multiple_keys_expectations: ok"
