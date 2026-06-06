#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_wordpress_meta_cast_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_wordpress_meta_cast_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET SESSION sql_mode = '';" >/dev/null

regexp_expected=$(cat <<EXPECTED
1,5
1,5
2,3,4
1,2,3,5
EXPECTED
)
expect_output \
    "binary regexp predicates" \
    "$regexp_expected" \
    "CREATE TABLE meta_keys(id INT PRIMARY KEY, meta_key VARCHAR(64)); "\
"INSERT INTO meta_keys VALUES "\
"(1, 'AAA_FOO_one'), (2, 'aaa_foo_two'), (3, 'AAA_foo_three'), "\
"(4, 'BBB_FOO_four'), (5, 'AAA_FOO_five'); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM meta_keys "\
"WHERE CAST(meta_key AS BINARY) REGEXP BINARY 'AAA_FOO_.*'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM meta_keys "\
"WHERE CAST(meta_key AS BINARY) RLIKE BINARY 'AAA_FOO_.*'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM meta_keys "\
"WHERE CAST(meta_key AS BINARY) NOT REGEXP BINARY 'AAA_FOO_.*'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM meta_keys "\
"WHERE meta_key REGEXP 'AAA_foo_.*';" \
    "$DATABASE"

between_expected=$(cat <<EXPECTED
1,2
3,4
EXPECTED
)
expect_output \
    "signed between predicates" \
    "$between_expected" \
    "CREATE TABLE colors(id INT PRIMARY KEY, meta_value VARCHAR(20) NULL); "\
"INSERT INTO colors VALUES (1, '1'), (2, '3'), (3, '4'), (4, '0'), (5, NULL); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM colors "\
"WHERE CAST(meta_value AS SIGNED) BETWEEN '1' AND '3'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM colors "\
"WHERE CAST(meta_value AS SIGNED) NOT BETWEEN '1' AND '3';" \
    "$DATABASE"

decimal_expected=$(cat <<EXPECTED
3
4
1,2,3
1,3
2,4
3
1,2,4
EXPECTED
)
expect_output \
    "decimal comparison predicates" \
    "$decimal_expected" \
    "CREATE TABLE decimals(id INT PRIMARY KEY, meta_value VARCHAR(20) NULL); "\
"INSERT INTO decimals VALUES "\
"(1, '-0.3'), (2, '0.0'), (3, '0.3'), (4, '0.4'), (5, NULL); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,2)) = '.300'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,2)) > '0.35'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,2)) <= '0.3'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,2)) LIKE '%.3%'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,2)) NOT LIKE '%.3%'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,10)) BETWEEN '0.23409845' AND '.31'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM decimals "\
"WHERE CAST(meta_value AS DECIMAL(10,10)) NOT BETWEEN '0.23409845' AND '.31';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_wordpress_meta_cast_predicates_expectations: ok"
