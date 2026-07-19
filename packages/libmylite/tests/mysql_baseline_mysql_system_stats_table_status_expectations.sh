#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_system_stats_table_status_expectations: $1" >&2
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

expect_show_status_row() {
    label=$1
    like_pattern=$2
    expected_prefix=$3

    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE '$like_pattern';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-4)
    storage_metrics=$(printf '%s\n' "$output" | cut -f 5-10)
    auto_increment=$(printf '%s\n' "$output" | cut -f 11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    update_time=$(printf '%s\n' "$output" | cut -f 13)
    stable_tail=$(printf '%s\n' "$output" | cut -f 14-17)
    comment=$(printf '%s\n' "$output" | cut -f 18)

    if [ "$field_count" != "18" ]; then
        fail "$label: expected 18 SHOW TABLE STATUS fields, got [$field_count]"
    fi
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "$label: expected stable prefix [$expected_prefix], got [$prefix]"
    fi
    if ! printf '%s\n' "$storage_metrics" | awk -F '\t' 'NF == 6 {
        for (i = 1; i <= NF; i++) {
            if ($i !~ /^[0-9]+$/) {
                exit 1;
            }
        }
        exit 0;
    } { exit 1; }'; then
        fail "$label: expected numeric storage metrics, got [$storage_metrics]"
    fi
    if [ "$auto_increment" != "NULL" ]; then
        fail "$label: expected NULL Auto_increment, got [$auto_increment]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "$label: expected non-NULL Create_time datetime, got [$create_time]" ;;
    esac
    case "$update_time" in
        NULL) ;;
        ????-??-??\ ??:??:??) ;;
        *) fail "$label: expected NULL or Update_time datetime, got [$update_time]" ;;
    esac
    expected_tail=$(printf '%b' 'NULL\tutf8mb3_bin\tNULL\trow_format=DYNAMIC stats_persistent=0')
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "$label: expected stable tail [$expected_tail], got [$stable_tail]"
    fi
    if [ "$comment" != "" ]; then
        fail "$label: expected empty Comment field, got [$comment]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

tables_expected=$(
    printf '%b' \
        'innodb_index_stats\tBASE TABLE\tInnoDB\t10\tDynamic\t1\t1\t1\t1\tutf8mb3_bin\t1\trow_format=DYNAMIC stats_persistent=0\t\n' \
        'innodb_table_stats\tBASE TABLE\tInnoDB\t10\tDynamic\t1\t1\t1\t1\tutf8mb3_bin\t1\trow_format=DYNAMIC stats_persistent=0\t'
)
expect_output \
    "mysql system stats INFORMATION_SCHEMA.TABLES rows" \
    "$tables_expected" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT,
            AUTO_INCREMENT IS NULL, CREATE_TIME IS NOT NULL,
            (UPDATE_TIME IS NULL OR UPDATE_TIME <= CURRENT_TIMESTAMP),
            CHECK_TIME IS NULL, TABLE_COLLATION,
            CHECKSUM IS NULL, CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
      ORDER BY TABLE_NAME;"

expect_show_status_row \
    "mysql system innodb_index_stats SHOW TABLE STATUS row" \
    "innodb\\_index\\_stats" \
    "$(printf '%b' 'innodb_index_stats\tInnoDB\t10\tDynamic')"

expect_show_status_row \
    "mysql system innodb_table_stats SHOW TABLE STATUS row" \
    "innodb\\_table\\_stats" \
    "$(printf '%b' 'innodb_table_stats\tInnoDB\t10\tDynamic')"

expect_output \
    "mysql system stats valid update-time count" \
    "2" \
    "SELECT COUNT(*)
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
        AND (UPDATE_TIME IS NULL OR UPDATE_TIME <= CURRENT_TIMESTAMP);"

printf '%s\n' "mysql_baseline_mysql_system_stats_table_status_expectations: ok"
