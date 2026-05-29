#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_system_show_columns_expectations: $1" >&2
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

table_columns_expected=$(
    printf '%b' \
        'database_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'table_name\tvarchar(199)\tNO\tPRI\tNULL\t\n' \
        'last_update\ttimestamp\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n' \
        'n_rows\tbigint unsigned\tNO\t\tNULL\t\n' \
        'clustered_index_size\tbigint unsigned\tNO\t\tNULL\t\n' \
        'sum_of_other_index_sizes\tbigint unsigned\tNO\t\tNULL\t'
)
expect_output \
    "show columns innodb_table_stats" \
    "$table_columns_expected" \
    "SHOW COLUMNS FROM mysql.innodb_table_stats;"

index_full_expected=$(
    printf '%b' \
        'database_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'table_name\tvarchar(199)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'index_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'last_update\ttimestamp\tNULL\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\tselect,insert,update,references\t\n' \
        'stat_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'stat_value\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'sample_size\tbigint unsigned\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\n' \
        'stat_description\tvarchar(1024)\tutf8mb3_bin\tNO\t\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "show full columns innodb_index_stats" \
    "$index_full_expected" \
    "SHOW FULL COLUMNS FROM mysql.innodb_index_stats;"

expect_output \
    "describe innodb_table_stats" \
    "$table_columns_expected" \
    "DESCRIBE mysql.innodb_table_stats;"

expect_output \
    "explicit mysql schema innodb_table_stats" \
    "$table_columns_expected" \
    "SHOW COLUMNS IN innodb_table_stats IN mysql;"

selected_like_expected=$(
    printf '%b' 'n_rows\tbigint unsigned\tNO\t\tNULL\t'
)
expect_output \
    "selected mysql schema like" \
    "$selected_like_expected" \
    "USE mysql; SHOW COLUMNS FROM innodb_table_stats LIKE 'n%';"

where_expected=$(
    printf '%b' 'stat_name\tvarchar(64)\tNO\tPRI\tNULL\t'
)
expect_output \
    "show fields where" \
    "$where_expected" \
    "SHOW FIELDS FROM mysql.innodb_index_stats WHERE Field = 'stat_name';"

full_where_expected=$(
    printf '%b' \
        'database_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'table_name\tvarchar(199)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'index_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'stat_name\tvarchar(64)\tutf8mb3_bin\tNO\tPRI\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "show full columns where" \
    "$full_where_expected" \
    "SHOW FULL COLUMNS FROM mysql.innodb_index_stats
      WHERE Collation = 'utf8mb3_bin' AND Field LIKE '%name';"

printf '%s\n' "mysql_baseline_mysql_system_show_columns_expectations: ok"
