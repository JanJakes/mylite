#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_aggregate_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_multi_aggregate_select_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
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
       n INT NULL,
       m INT NOT NULL,
       label VARCHAR(20) NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, 5, 'a'),
       (2, 10, 7, 'b'),
       (3, 20, 9, 'c'),
       (4, 30, 11, NULL);" >/dev/null

mixed=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(n), COUNT(NULL), COUNT(DISTINCT n), SUM(n), AVG(n),
            MIN(n), MAX(n), BIT_AND(n), BIT_OR(n), BIT_XOR(n), STDDEV_POP(n),
            STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n),
            GROUP_CONCAT(label ORDER BY id SEPARATOR '|')
       FROM t;"
)
expect_value \
    "mixed aggregate row" \
    "4	3	0	3	60	20.0000	10	30	0	30	0	8.16496580927726	10	66.66666666666667	100	a|b|c" \
    "$mixed"

filtered=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n), STDDEV_POP(n),
            VAR_SAMP(n), GROUP_CONCAT(label ORDER BY id SEPARATOR ',')
       FROM t
      WHERE n >= 20;"
)
expect_value \
    "filtered aggregate row" \
    "2	2	50	25.0000	20	30	5	50	c" \
    "$filtered"

empty=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(n), COUNT(NULL), SUM(n), AVG(n), MIN(n), MAX(n),
            BIT_AND(n), BIT_OR(n), BIT_XOR(n), STDDEV_POP(n), VAR_POP(n),
            GROUP_CONCAT(label ORDER BY id)
       FROM t
      WHERE id > 99;"
)
expect_value \
    "empty aggregate row" \
    "0	0	0	NULL	NULL	NULL	NULL	18446744073709551615	0	0	NULL	NULL	NULL" \
    "$empty"

tableless=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(1), COUNT(NULL), MIN(5), MAX(9), STDDEV_POP(1),
            VAR_SAMP(1);"
)
expect_value "tableless aggregate row" "1	1	0	5	9	0	NULL" "$tableless"

limit_zero=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), SUM(n) FROM t LIMIT 0;"
)
expect_value "limit zero suppresses aggregate row" "" "$limit_zero"

limit_offset=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), SUM(n) FROM t LIMIT 1 OFFSET 1;"
)
expect_value "limit offset suppresses aggregate row" "" "$limit_offset"

printf '%s\n' "mysql_baseline_multi_aggregate_select_expectations: ok"
