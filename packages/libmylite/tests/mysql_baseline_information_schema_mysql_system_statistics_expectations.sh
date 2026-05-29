#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_mysql_system_statistics_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

statistics_expected=$(
    printf '%b' \
        'mysql\tinnodb_index_stats\t0\tmysql\tPRIMARY\t1\tdatabase_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'mysql\tinnodb_index_stats\t0\tmysql\tPRIMARY\t2\ttable_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'mysql\tinnodb_index_stats\t0\tmysql\tPRIMARY\t3\tindex_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'mysql\tinnodb_index_stats\t0\tmysql\tPRIMARY\t4\tstat_name\tA\t6\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'mysql\tinnodb_table_stats\t0\tmysql\tPRIMARY\t1\tdatabase_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'mysql\tinnodb_table_stats\t0\tmysql\tPRIMARY\t2\ttable_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql system statistics rows" \
    "$statistics_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, NON_UNIQUE, INDEX_SCHEMA, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

tail_expected=$(
    printf '%b' \
        'innodb_index_stats\tindex_name\t2\n' \
        'innodb_index_stats\tstat_name\t6'
)
expect_output \
    "mysql system statistics filtered tail" \
    "$tail_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, CARDINALITY
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME = 'innodb_index_stats'
        AND SEQ_IN_INDEX >= 3
      ORDER BY SEQ_IN_INDEX;"

expect_output \
    "mysql system statistics row count" \
    "6" \
    "SELECT COUNT(*)
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');"

printf '%s\n' "mysql_baseline_information_schema_mysql_system_statistics_expectations: ok"
