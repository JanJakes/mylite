#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_mysql_system_constraints_expectations: $1" >&2
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

table_constraints_expected=$(
    printf '%b' \
        'def\tmysql\tPRIMARY\tmysql\tinnodb_index_stats\tPRIMARY KEY\tYES\n' \
        'def\tmysql\tPRIMARY\tmysql\tinnodb_table_stats\tPRIMARY KEY\tYES'
)
expect_output \
    "mysql system table constraints rows" \
    "$table_constraints_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_SCHEMA,
            TABLE_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

key_column_usage_expected=$(
    printf '%b' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_index_stats\tdatabase_name\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_index_stats\ttable_name\t2\tNULL\tNULL\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_index_stats\tindex_name\t3\tNULL\tNULL\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_index_stats\tstat_name\t4\tNULL\tNULL\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_table_stats\tdatabase_name\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tdef\tmysql\tinnodb_table_stats\ttable_name\t2\tNULL\tNULL\tNULL\tNULL'
)
expect_output \
    "mysql system key column usage rows" \
    "$key_column_usage_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_CATALOG,
            TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;"

table_constraints_extensions_expected=$(
    printf '%b' \
        'def\tmysql\tPRIMARY\tinnodb_index_stats\tNULL\tNULL\n' \
        'def\tmysql\tPRIMARY\tinnodb_table_stats\tNULL\tNULL'
)
expect_output \
    "mysql system table constraints extensions rows" \
    "$table_constraints_extensions_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_NAME,
            ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "mysql system key column usage row count" \
    "6" \
    "SELECT COUNT(*)
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');"

printf '%s\n' "mysql_baseline_information_schema_mysql_system_constraints_expectations: ok"
