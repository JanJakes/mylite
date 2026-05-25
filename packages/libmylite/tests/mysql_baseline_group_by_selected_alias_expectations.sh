#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_selected_alias_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_selected_alias_expectations: $1" >&2
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
       id INT NOT NULL PRIMARY KEY,
       name VARCHAR(20) NULL,
       label VARCHAR(20) NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, 'alice', 'x', 10),
       (2, 'Alice', 'x', 20),
       (3, 'bob', 'y', 30),
       (4, NULL, 'z', 40);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT name AS grouped_name, COUNT(*) AS c FROM t
       GROUP BY grouped_name ORDER BY grouped_name;
     SELECT n AS grouped_n, COUNT(*) AS c FROM t GROUP BY grouped_n ORDER BY grouped_n;
     SELECT name AS grouped_name, label AS grouped_label, COUNT(*) AS c FROM t
       GROUP BY grouped_name, grouped_label ORDER BY grouped_name;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "string alias null group" "NULL	1" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "string alias alice group" "alice	2" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "string alias bob group" "bob	1" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "integer alias first group" "10	1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "integer alias second group" "20	1" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "integer alias third group" "30	1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "integer alias fourth group" "40	1" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "multi alias null group" "NULL	z	1" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "multi alias alice group" "alice	x	2" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "multi alias bob group" "bob	y	1" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "alias grouped status" "0	-1" "$(printf '%s\n' "$core" | sed -n '11p')"

filters=$(run_mysql \
    "USE ${DATABASE};
     SELECT name AS grouped_name, COUNT(*) AS c FROM t
       GROUP BY grouped_name HAVING c > 1 ORDER BY grouped_name LIMIT 1;
     SELECT name AS grouped_name, COUNT(*) AS c FROM t
       GROUP BY grouped_name HAVING grouped_name IS NULL;
     SELECT id AS post_id, name FROM t GROUP BY post_id ORDER BY post_id;"
)
expect_value "having alias aggregate" "alice	2" "$(printf '%s\n' "$filters" | sed -n '1p')"
expect_value "having group alias null" "NULL	1" "$(printf '%s\n' "$filters" | sed -n '2p')"
expect_value "pk alias projection first" "1	alice" "$(printf '%s\n' "$filters" | sed -n '3p')"
expect_value "pk alias projection second" "2	Alice" "$(printf '%s\n' "$filters" | sed -n '4p')"
expect_value "pk alias projection third" "3	bob" "$(printf '%s\n' "$filters" | sed -n '5p')"
expect_value "pk alias projection fourth" "4	NULL" "$(printf '%s\n' "$filters" | sed -n '6p')"

headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT name AS grouped_name, COUNT(*) AS c FROM t
       GROUP BY grouped_name ORDER BY grouped_name LIMIT 1;"
)
expect_value "header labels" "grouped_name	c" "$(printf '%s\n' "$headers" | sed -n '1p')"
expect_value "header row" "NULL	1" "$(printf '%s\n' "$headers" | sed -n '2p')"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE ambiguous(a INT NULL, b INT NULL) ENGINE=InnoDB;
     INSERT INTO ambiguous VALUES (1, 10), (1, 20), (2, 10);" >/dev/null

expect_error \
    "source column wins over alias" \
    1055 \
    "42000" \
    "Expression #1 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE}; SELECT a AS b, COUNT(*) FROM ambiguous GROUP BY b ORDER BY b;"
expect_error \
    "duplicate selected aliases" \
    1052 \
    "23000" \
    "Column 'x' in group statement is ambiguous" \
    "USE ${DATABASE}; SELECT name AS x, label AS x, COUNT(*) FROM t GROUP BY x ORDER BY x;"

expression_alias=$(run_mysql \
    "USE ${DATABASE};
     SELECT id + 1 AS k, COUNT(*) FROM t GROUP BY k ORDER BY k;"
)
expect_value "expression alias first" "2	1" "$(printf '%s\n' "$expression_alias" | sed -n '1p')"
expect_value "expression alias fourth" "5	1" "$(printf '%s\n' "$expression_alias" | sed -n '4p')"

cleanup

printf '%s\n' "mysql_baseline_group_by_selected_alias_expectations: ok"
