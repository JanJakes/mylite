#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_numeric_comparison_update_contexts_expectations_$$"

fail() {
    printf '%s\n' \
        "mysql_baseline_row_scalar_numeric_comparison_update_contexts_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

setup_sql="CREATE TABLE t("\
"id INT, i INT, d DECIMAL(10,2), s VARCHAR(32), "\
"out_greatest INT, out_least VARCHAR(64), out_interval INT, out_isnull INT, "\
"out_crc32 BIGINT, out_format VARCHAR(64), out_trunc VARCHAR(64), out_mod INT); "\
"INSERT INTO t(id, i, d, s) VALUES "\
"(1, 7, 12.34, 'alpha'), "\
"(2, 3, 56.78, 'Zulu'), "\
"(3, NULL, NULL, NULL);"
run_mysql "$setup_sql" "$DATABASE" >/dev/null

expected=$(cat <<EXPECTED
greatest	2
least	2
interval	3
isnull	3
crc32	2
format	2
truncate	2
mod	2
1	7	alpha	2	0	3504355690	12.3	12.3	3
2	5	m	1	0	1055472505	56.8	56.7	3
3	NULL	NULL	-1	1	NULL	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "row-scalar numeric/comparison UPDATE assignments" \
    "$expected" \
    "UPDATE t SET out_greatest = GREATEST(i, 5); SELECT 'greatest', ROW_COUNT(); "\
"UPDATE t SET out_least = LEAST(s, 'm'); SELECT 'least', ROW_COUNT(); "\
"UPDATE t SET out_interval = INTERVAL(i, 1, 5, 10); SELECT 'interval', ROW_COUNT(); "\
"UPDATE t SET out_isnull = ISNULL(s); SELECT 'isnull', ROW_COUNT(); "\
"UPDATE t SET out_crc32 = CRC32(s); SELECT 'crc32', ROW_COUNT(); "\
"UPDATE t SET out_format = FORMAT(d, 1); SELECT 'format', ROW_COUNT(); "\
"UPDATE t SET out_trunc = TRUNCATE(d, 1); SELECT 'truncate', ROW_COUNT(); "\
"UPDATE t SET out_mod = MOD(i, 4); SELECT 'mod', ROW_COUNT(); "\
"SELECT id, out_greatest, out_least, out_interval, out_isnull, out_crc32, "\
"out_format, out_trunc, out_mod FROM t ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_scalar_numeric_comparison_update_contexts_expectations: ok"
