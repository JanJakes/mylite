#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_grs_having_$$"

fail() {
    printf '%s\n' "mysql_baseline_grouped_selected_row_scalar_aggregate_expression_having_expectations: $1" >&2
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
       g INT NULL,
       n INT NULL,
       nn INT NOT NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, 5),
       (2, 1, 10, 6),
       (3, 1, NULL, 7),
       (4, 2, 20, 8),
       (5, 2, 30, 9);" \
    >/dev/null

rows=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(IFNULL(n, 0)) AS c
       FROM t GROUP BY g HAVING COUNT(IFNULL(n, 0)) > 1 ORDER BY g;
     SELECT g, COUNT(n + 1) AS c
       FROM t GROUP BY g HAVING COUNT(n + 1) > 1 ORDER BY g;
     SELECT g, SUM(n + 1) AS s
       FROM t GROUP BY g HAVING SUM(n + 1) > 20 ORDER BY g;
     SELECT g, AVG(n + 1) AS a
       FROM t GROUP BY g HAVING AVG(n + 1) > 20 ORDER BY g;
     SELECT g, MAX(IFNULL(n, 0)) AS m
       FROM t GROUP BY g HAVING MAX(IFNULL(n, 0)) > 20 ORDER BY g;
     SELECT g, MIN(IFNULL(n, 99)) AS m
       FROM t GROUP BY g HAVING MIN(IFNULL(n, 99)) < 20 ORDER BY g;"
)

expect_value "count row-scalar having first" "1	2" \
    "$(printf '%s\n' "$rows" | sed -n '1p')"
expect_value "count row-scalar having second" "2	2" \
    "$(printf '%s\n' "$rows" | sed -n '2p')"
expect_value "count row-scalar having null skip" "2	2" \
    "$(printf '%s\n' "$rows" | sed -n '3p')"
expect_value "sum row-scalar having" "2	52" \
    "$(printf '%s\n' "$rows" | sed -n '4p')"
expect_value "avg row-scalar having" "2	26.0000" \
    "$(printf '%s\n' "$rows" | sed -n '5p')"
expect_value "max row-scalar having" "2	30" \
    "$(printf '%s\n' "$rows" | sed -n '6p')"
expect_value "min row-scalar having" "1	10" \
    "$(printf '%s\n' "$rows" | sed -n '7p')"
expect_value "row-scalar having extra row" "" \
    "$(printf '%s\n' "$rows" | sed -n '8p')"

printf '%s\n' "mysql_baseline_grouped_selected_row_scalar_aggregate_expression_having_expectations: ok"
