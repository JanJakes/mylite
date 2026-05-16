#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporal_z_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporal_z_predicate_strings_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE dt (id INT NOT NULL, d DATETIME NULL); "\
"CREATE TABLE ts (id INT NOT NULL, ts TIMESTAMP NULL); "\
"INSERT INTO dt VALUES "\
"(1, '2024-01-01 00:00:00'), "\
"(2, '2024-01-01 02:00:00'), "\
"(3, '2024-01-03 00:00:00'), "\
"(4, NULL); "\
"INSERT INTO ts VALUES "\
"(1, '2024-01-01 00:00:00'), "\
"(2, '2024-01-01 02:00:00'), "\
"(3, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "datetime T-Z equality truncates with warning" \
    "dt_eq|1
1
Warning|1292|Incorrect datetime value: '2024-01-01T00:00:00Z' for column 'd' at row 1" \
    "SELECT 'dt_eq', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime space-Z and lowercase-z forms truncate" \
    "dt_space|1
1
dt_lower|1
1" \
    "SELECT 'dt_space', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01 00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'dt_lower', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01T00:00:00z'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime comparisons and in lists truncate" \
    "dt_lt|1,2
1
dt_in|1,3
2" \
    "SELECT 'dt_lt', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d < '2024-01-02T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'dt_in', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d IN ('2024-01-01T00:00:00Z', '2024-01-03T00:00:00Z'); "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime between duplicates truncation warnings per evaluated row" \
    "dt_between|1,2
6
dt_not_between|3
6" \
    "SELECT 'dt_between', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d BETWEEN '2023-12-31T00:00:00Z' AND '2024-01-02T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'dt_not_between', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d NOT BETWEEN '2023-12-31T00:00:00Z' AND '2024-01-02T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime trailing Z is not a UTC designator" \
    "dt_plus2|1
1" \
    "SET time_zone = '+02:00'; "\
"SELECT 'dt_plus2', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "timestamp trailing Z follows normal timestamp comparison" \
    "ts_plus2|NULL
1
ts_utc|1
1
Warning|1292|Incorrect datetime value: '2024-01-01T00:00:00Z' for column 'ts' at row 1" \
    "SET time_zone = '+02:00'; "\
"SELECT 'ts_plus2', GROUP_CONCAT(id ORDER BY id) FROM ts "\
"WHERE ts = '2024-01-01T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; "\
"SET time_zone = '+00:00'; "\
"SELECT 'ts_utc', GROUP_CONCAT(id ORDER BY id) FROM ts "\
"WHERE ts = '2024-01-01T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "broader trailing garbage truncation is a deferred MySQL surface" \
    "dt_q|1
1
dt_z_offset|1
1" \
    "SELECT 'dt_q', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01T00:00:00Q'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'dt_z_offset', GROUP_CONCAT(id ORDER BY id) FROM dt "\
"WHERE d = '2024-01-01T00:00:00Z+00:00'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_temporal_z_predicate_strings_expectations: ok"
