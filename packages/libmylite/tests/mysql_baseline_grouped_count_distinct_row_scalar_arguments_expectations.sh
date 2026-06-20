#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_gcd_row_scalar_$$"

fail() {
    printf '%s\n' "mysql_baseline_grouped_count_distinct_row_scalar_arguments_expectations: $1" >&2
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
       (5, 2, 30, 9);
     CREATE TABLE string_grouped(
       id INT NOT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       body TEXT NULL,
       raw VARBINARY(4) NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO string_grouped VALUES
       (1, NULL, NULL, NULL, NULL, 10),
       (2, 'alice', 'A', 'essay', X'41', 20),
       (3, 'Alice', 'A   ', 'Essay', X'4100', 30),
       (4, 'bob', 'B', 'note', X'42', NULL),
       (5, 'BOB', 'B    ', 'Note', X'42', 5),
       (6, 'carol', 'C', NULL, X'43', 7);" \
    >/dev/null

literal_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(DISTINCT 1), COUNT(DISTINCT TRUE), COUNT(DISTINCT NULL)
       FROM t GROUP BY g ORDER BY g;"
)
expect_value "literal distinct null group" "NULL	1	1	0" \
    "$(printf '%s\n' "$literal_counts" | sed -n '1p')"
expect_value "literal distinct group one" "1	1	1	0" \
    "$(printf '%s\n' "$literal_counts" | sed -n '2p')"
expect_value "literal distinct group two" "2	1	1	0" \
    "$(printf '%s\n' "$literal_counts" | sed -n '3p')"

numeric_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(DISTINCT n + 1), COUNT(DISTINCT n + nn),
            COUNT(DISTINCT IFNULL(n, 0)), COUNT(DISTINCT NULLIF(n, 20))
       FROM t GROUP BY g ORDER BY g;
     SELECT g, COUNT(DISTINCT n + 1) AS c
       FROM t GROUP BY g HAVING c > 1 ORDER BY g;
     SELECT g, COUNT(DISTINCT n + 1) AS c
       FROM t GROUP BY g HAVING COUNT(DISTINCT n + 1) > 1 ORDER BY g;
     SELECT g, COUNT(DISTINCT n + 1) AS c
       FROM t GROUP BY g ORDER BY COUNT(DISTINCT n + 1) DESC, g LIMIT 2;"
)
expect_value "numeric distinct null group" "NULL	0	0	1	0" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '1p')"
expect_value "numeric distinct group one" "1	1	1	2	1" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '2p')"
expect_value "numeric distinct group two" "2	2	2	2	1" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '3p')"
expect_value "numeric distinct having alias" "2	2" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '4p')"
expect_value "numeric distinct having expression" "2	2" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '5p')"
expect_value "numeric distinct order first" "2	2" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '6p')"
expect_value "numeric distinct order second" "1	1" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '7p')"

string_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT name, COUNT(DISTINCT CONCAT(body, '!')),
            COUNT(DISTINCT IFNULL(body, '')), COUNT(DISTINCT LOWER(name)),
            COUNT(DISTINCT CAST(n AS CHAR))
       FROM string_grouped GROUP BY name ORDER BY name;"
)
expect_value "string distinct null group" "NULL	0	1	0	1" \
    "$(printf '%s\n' "$string_counts" | sed -n '1p')"
expect_value "string distinct alice group" "alice	1	1	1	2" \
    "$(printf '%s\n' "$string_counts" | sed -n '2p')"
expect_value "string distinct bob group" "bob	1	1	1	1" \
    "$(printf '%s\n' "$string_counts" | sed -n '3p')"
expect_value "string distinct carol group" "carol	0	1	1	1" \
    "$(printf '%s\n' "$string_counts" | sed -n '4p')"

printf '%s\n' "mysql_baseline_grouped_count_distinct_row_scalar_arguments_expectations: ok"
