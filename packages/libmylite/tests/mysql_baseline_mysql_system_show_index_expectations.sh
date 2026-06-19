#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_system_show_index_expectations: $1" >&2
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

expect_show_index_output() {
    label=$1
    expected=$2
    sql=$3

    output=$(run_mysql "$sql" | cut -f 1-6,8-15)
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_index_expected=$(
    printf '%b' \
        'innodb_table_stats\t0\tPRIMARY\t1\tdatabase_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'innodb_table_stats\t0\tPRIMARY\t2\ttable_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_show_index_output \
    "show index innodb_table_stats" \
    "$table_index_expected" \
    "SHOW INDEX FROM mysql.innodb_table_stats;"

index_expected=$(
    printf '%b' \
        'innodb_index_stats\t0\tPRIMARY\t1\tdatabase_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'innodb_index_stats\t0\tPRIMARY\t2\ttable_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'innodb_index_stats\t0\tPRIMARY\t3\tindex_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'innodb_index_stats\t0\tPRIMARY\t4\tstat_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_show_index_output \
    "show indexes innodb_index_stats" \
    "$index_expected" \
    "SHOW INDEXES FROM mysql.innodb_index_stats;"

index_tail_expected=$(
    printf '%b' \
        'innodb_index_stats\t0\tPRIMARY\t3\tindex_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'innodb_index_stats\t0\tPRIMARY\t4\tstat_name\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_show_index_output \
    "explicit mysql schema innodb_index_stats where" \
    "$index_tail_expected" \
    "SHOW INDEXES IN innodb_index_stats IN mysql WHERE Seq_in_index >= '3';"

expect_show_index_output \
    "selected mysql schema keys" \
    "$table_index_expected" \
    "USE mysql; SHOW KEYS FROM innodb_table_stats WHERE Key_name = 'PRIMARY';"

expect_show_index_output \
    "selected mysql schema column-name where" \
    "$index_expected" \
    "USE mysql; SHOW INDEX FROM innodb_index_stats WHERE Column_name LIKE '%name';"

printf '%s\n' "mysql_baseline_mysql_system_show_index_expectations: ok"
