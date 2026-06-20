#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bitwise_having_$$"

fail() {
    printf '%s\n' "mysql_baseline_grouped_bitwise_aggregate_having_expectations: $1" >&2
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
     CREATE TABLE bitwise_order(
       g INT NULL,
       bor INT NOT NULL,
       band INT NOT NULL,
       bxor INT NOT NULL
     ) ENGINE=InnoDB;
     INSERT INTO bitwise_order VALUES
       (NULL, 7, 7, 7),
       (1, 7, 15, 7),
       (1, 8, 13, 8),
       (2, 10, 15, 10),
       (2, 1, 11, 1);
     CREATE TABLE grouped_numbers(
       g INT NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO grouped_numbers VALUES
       (NULL, NULL),
       (1, 10),
       (1, NULL),
       (2, 20),
       (2, 30);" \
    >/dev/null

rows=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, BIT_OR(bor) AS bo
       FROM bitwise_order GROUP BY g HAVING bo > 11 ORDER BY bo;
     SELECT g, BIT_AND(band) AS ba
       FROM bitwise_order GROUP BY g HAVING ba >= 11 ORDER BY ba;
     SELECT g, BIT_XOR(bxor) AS bx
       FROM bitwise_order GROUP BY g HAVING bx < 12 ORDER BY bx;
     SELECT g, BIT_OR(bor) AS bo
       FROM bitwise_order GROUP BY g HAVING BIT_OR(bor) = 15 ORDER BY g;
     SELECT g, BIT_OR(bor + 1) AS bo
       FROM bitwise_order GROUP BY g HAVING BIT_OR(bor + 1) > 9 ORDER BY g;
     SELECT g, BIT_AND(n) AS ba
       FROM grouped_numbers GROUP BY g HAVING ba > 9223372036854775807 ORDER BY g;"
)

expect_value "bit or alias having" "1	15" "$(printf '%s\n' "$rows" | sed -n '1p')"
expect_value "bit and alias having first" "2	11" "$(printf '%s\n' "$rows" | sed -n '2p')"
expect_value "bit and alias having second" "1	13" "$(printf '%s\n' "$rows" | sed -n '3p')"
expect_value "bit xor alias having first" "NULL	7" "$(printf '%s\n' "$rows" | sed -n '4p')"
expect_value "bit xor alias having second" "2	11" "$(printf '%s\n' "$rows" | sed -n '5p')"
expect_value "bit or expression having" "1	15" "$(printf '%s\n' "$rows" | sed -n '6p')"
expect_value "bit or row-scalar expression having" "2	11" "$(printf '%s\n' "$rows" | sed -n '7p')"
expect_value "bit and uint64 literal having" "NULL	18446744073709551615" \
    "$(printf '%s\n' "$rows" | sed -n '8p')"
expect_value "bitwise having extra row" "" "$(printf '%s\n' "$rows" | sed -n '9p')"

printf '%s\n' "mysql_baseline_grouped_bitwise_aggregate_having_expectations: ok"
