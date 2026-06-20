#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_string_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_string_column_expectations: $1" >&2
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
     CREATE TABLE s(
       id INT NOT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       body TEXT NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO s VALUES
       (1, NULL, NULL, NULL, 10),
       (2, 'alice', 'A', 'essay', 20),
       (3, 'Alice', 'A   ', 'Essay', 30),
       (4, 'bob', 'B', 'note', NULL),
       (5, 'BOB', 'B    ', 'Note', 5),
       (6, 'carol', 'C', NULL, 7);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     SELECT @@character_set_database, @@collation_database;
     DO 0;
     SELECT name, COUNT(*), SUM(n) FROM s GROUP BY name ORDER BY name;
     SELECT label, COUNT(*), SUM(n) FROM s GROUP BY label ORDER BY label;
     SELECT body, COUNT(*), SUM(n) FROM s GROUP BY body ORDER BY body;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "default schema collation" "utf8mb4	utf8mb4_0900_ai_ci" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "varchar null group" "NULL	1	10" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "varchar case-folded alice group" "alice	2	50" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "varchar case-folded bob group" "bob	2	5" \
    "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "varchar single group" "carol	1	7" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "char null group" "NULL	1	10" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "char trailing-space group a" "A	2	50" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "char trailing-space group b" "B	2	5" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "char single group" "C	1	7" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "text null group" "NULL	2	17" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "text case-folded essay group" "essay	2	50" \
    "$(printf '%s\n' "$core" | sed -n '11p')"
expect_value "text case-folded note group" "note	2	5" \
    "$(printf '%s\n' "$core" | sed -n '12p')"
expect_value "grouped string status" "0	-1" "$(printf '%s\n' "$core" | sed -n '13p')"

filters=$(run_mysql \
    "USE ${DATABASE};
     SELECT name AS k, COUNT(*) AS c FROM s GROUP BY name HAVING c > 1 ORDER BY k DESC LIMIT 1;
     SELECT name AS k, COUNT(*) AS c FROM s GROUP BY name HAVING k IS NULL;
     SELECT name AS k, COUNT(*) AS c FROM s WHERE n IS NOT NULL
       GROUP BY name HAVING c >= 1 ORDER BY k LIMIT 2 OFFSET 1;
     SELECT name AS k, COUNT(*) AS c FROM s GROUP BY name HAVING k = 'ALICE';
     SELECT name AS k, COUNT(*) AS c FROM s GROUP BY name HAVING name <> 'ALICE' ORDER BY k;
     SELECT name AS k, COUNT(*) AS c FROM s GROUP BY name HAVING s.name = 'BOB';
     SELECT label AS k, COUNT(*) AS c FROM s GROUP BY label HAVING k = 'a';
     SELECT body AS k, COUNT(*) AS c FROM s GROUP BY body HAVING body <= 'NOTE' ORDER BY k;"
)
expect_value "having aggregate alias desc limit" "bob	2" \
    "$(printf '%s\n' "$filters" | sed -n '1p')"
expect_value "having group alias null" "NULL	1" "$(printf '%s\n' "$filters" | sed -n '2p')"
expect_value "where before grouping offset first" "alice	2" \
    "$(printf '%s\n' "$filters" | sed -n '3p')"
expect_value "where before grouping offset second" "BOB	1" \
    "$(printf '%s\n' "$filters" | sed -n '4p')"
expect_value "having string alias equality" "alice	2" \
    "$(printf '%s\n' "$filters" | sed -n '5p')"
expect_value "having string direct inequality first" "bob	2" \
    "$(printf '%s\n' "$filters" | sed -n '6p')"
expect_value "having string direct inequality second" "carol	1" \
    "$(printf '%s\n' "$filters" | sed -n '7p')"
expect_value "having string qualified equality" "bob	2" \
    "$(printf '%s\n' "$filters" | sed -n '8p')"
expect_value "having char alias equality" "A	2" "$(printf '%s\n' "$filters" | sed -n '9p')"
expect_value "having text direct range first" "essay	2" \
    "$(printf '%s\n' "$filters" | sed -n '10p')"
expect_value "having text direct range second" "note	2" \
    "$(printf '%s\n' "$filters" | sed -n '11p')"

limits=$(run_mysql \
    "USE ${DATABASE};
     SELECT name, COUNT(*) FROM s GROUP BY name ORDER BY name LIMIT 0;
     SELECT name, COUNT(*) FROM s GROUP BY name ORDER BY name LIMIT 2;
     SELECT name, COUNT(*) FROM s GROUP BY name ORDER BY name DESC;"
)
expect_value "limit zero no rows before limited rows" "NULL	1" \
    "$(printf '%s\n' "$limits" | sed -n '1p')"
expect_value "limit exact second row" "alice	2" "$(printf '%s\n' "$limits" | sed -n '2p')"
expect_value "desc first row" "carol	1" "$(printf '%s\n' "$limits" | sed -n '3p')"
expect_value "desc second row" "bob	2" "$(printf '%s\n' "$limits" | sed -n '4p')"
expect_value "desc third row" "alice	2" "$(printf '%s\n' "$limits" | sed -n '5p')"
expect_value "desc null row" "NULL	1" "$(printf '%s\n' "$limits" | sed -n '6p')"

cleanup

printf '%s\n' "mysql_baseline_group_by_string_column_expectations: ok"
