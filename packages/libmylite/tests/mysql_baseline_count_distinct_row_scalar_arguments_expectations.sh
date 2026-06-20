#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_distinct_row_scalar_arguments_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_distinct_row_scalar_arguments_expectations: $1" >&2
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
       m INT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, 5, NULL, NULL),
       (2, 10, 6, 'alice', 'A'),
       (3, 20, 6, 'Alice', 'A   '),
       (4, 20, NULL, 'bob', 'B'),
       (5, 30, 8, 'BOB', 'B   ');
     CREATE TABLE posts(id INT NOT NULL);
     CREATE TABLE comments(id INT NOT NULL, post_id INT NULL, score INT NULL);
     INSERT INTO posts VALUES (1), (2), (3);
     INSERT INTO comments VALUES
       (101, 1, 5), (102, 1, 5), (103, 1, 6), (104, 2, NULL), (105, 99, 7);" \
    >/dev/null

literal_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT 1), COUNT(DISTINCT TRUE), COUNT(DISTINCT NULL)
       FROM t;
     SELECT COUNT(DISTINCT 1), COUNT(DISTINCT NULL)
       FROM t WHERE id > 99;"
)
expect_value "literal distinct counts" "1	1	0" "$(printf '%s\n' "$literal_counts" | sed -n '1p')"
expect_value "empty literal distinct counts" "0	0" "$(printf '%s\n' "$literal_counts" | sed -n '2p')"

numeric_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT n + 1), COUNT(DISTINCT n + m),
            COUNT(DISTINCT IFNULL(n, 0)), COUNT(DISTINCT NULLIF(n, 20)),
            COUNT(DISTINCT LENGTH(name))
       FROM t;
     SELECT COUNT(DISTINCT n + 1)
       FROM t WHERE n IS NULL;"
)
expect_value "numeric row-scalar distinct counts" "3	3	4	2	2" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '1p')"
expect_value "all-null numeric row-scalar distinct count" "0" \
    "$(printf '%s\n' "$numeric_counts" | sed -n '2p')"

string_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT CONCAT(name, '!')), COUNT(DISTINCT IFNULL(name, '')),
            COUNT(DISTINCT LOWER(name)), COUNT(DISTINCT CAST(n AS CHAR))
       FROM t;"
)
expect_value "string row-scalar distinct counts" "2	3	2	3" \
    "$(printf '%s\n' "$string_counts" | sed -n '1p')"

mixed_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(n + 1), COUNT(DISTINCT n + 1),
            COUNT(DISTINCT CONCAT(name, '!'))
       FROM t
      WHERE id >= 2
      LIMIT 1;"
)
expect_value "mixed count expression distinct row" "4	4	3	2" "$mixed_counts"

joined_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*), COUNT(DISTINCT c.score + 1), COUNT(DISTINCT IFNULL(c.score, 0))
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id;"
)
expect_value "joined row-scalar distinct counts" "5	2	3" "$joined_counts"

printf '%s\n' "mysql_baseline_count_distinct_row_scalar_arguments_expectations: ok"
