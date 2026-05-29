#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_gtid_executed_table_expectations: $1" >&2
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

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'gtid_executed';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS gtid_executed: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' \
        'gtid_executed\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL')
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS gtid_executed: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS gtid_executed: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(printf '%b' \
        'NULL\tNULL\tutf8mb4_0900_ai_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\t')
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS gtid_executed: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql.gtid_executed baseline row count" \
    "0" \
    "SELECT COUNT(*) FROM mysql.gtid_executed;"
expect_output \
    "mysql.gtid_executed direct read is empty" \
    "" \
    "SELECT source_uuid, interval_start, interval_end, gtid_tag
       FROM mysql.gtid_executed
      ORDER BY source_uuid, gtid_tag, interval_start;"
expect_output \
    "mysql.gtid_executed read ROW_COUNT" \
    "-1" \
    "SELECT source_uuid, interval_start, interval_end, gtid_tag
       FROM mysql.gtid_executed
      ORDER BY source_uuid, gtid_tag, interval_start;
     SELECT ROW_COUNT();"

columns_expected=$(
    printf '%b' \
        'source_uuid\tchar(36)\tNO\tPRI\tNULL\t\n' \
        'interval_start\tbigint\tNO\tPRI\tNULL\t\n' \
        'interval_end\tbigint\tNO\t\tNULL\t\n' \
        'gtid_tag\tchar(32)\tNO\tPRI\tNULL\t'
)
expect_output \
    "mysql.gtid_executed SHOW COLUMNS rows" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.gtid_executed;"

full_columns_expected=$(
    printf '%b' \
        'source_uuid\tchar(36)\tutf8mb4_0900_ai_ci\tNO\tPRI\tNULL\t\tselect,insert,update,references\tuuid of the source where the transaction was originally executed.\n' \
        'interval_start\tbigint\tNULL\tNO\tPRI\tNULL\t\tselect,insert,update,references\tFirst number of interval.\n' \
        'interval_end\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\tLast number of interval.\n' \
        'gtid_tag\tchar(32)\tutf8mb4_0900_ai_ci\tNO\tPRI\tNULL\t\tselect,insert,update,references\tGTID Tag.'
)
expect_output \
    "mysql.gtid_executed SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.gtid_executed;"

show_index_expected=$(
    printf '%b' \
        'gtid_executed\t0\tPRIMARY\t1\tsource_uuid\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'gtid_executed\t0\tPRIMARY\t2\tgtid_tag\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'gtid_executed\t0\tPRIMARY\t3\tinterval_start\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql.gtid_executed SHOW INDEX rows" \
    "$show_index_expected" \
    "SHOW INDEX FROM mysql.gtid_executed;"

information_schema_columns_expected=$(
    printf '%b' \
        'source_uuid\t1\tNULL\tNO\tchar\t36\t144\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tchar(36)\tPRI\t\tselect,insert,update,references\tuuid of the source where the transaction was originally executed.\t\n' \
        'interval_start\t2\tNULL\tNO\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\tPRI\t\tselect,insert,update,references\tFirst number of interval.\t\n' \
        'interval_end\t3\tNULL\tNO\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\tLast number of interval.\t\n' \
        'gtid_tag\t4\tNULL\tNO\tchar\t32\t128\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tchar(32)\tPRI\t\tselect,insert,update,references\tGTID Tag.\t'
)
expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';"

key_column_usage_expected=$(
    printf '%b' \
        'PRIMARY\tsource_uuid\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'PRIMARY\tgtid_tag\t2\tNULL\tNULL\tNULL\tNULL\n' \
        'PRIMARY\tinterval_start\t3\tNULL\tNULL\tNULL\tNULL'
)
expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.KEY_COLUMN_USAGE rows" \
    "$key_column_usage_expected" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed'
      ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tgtid_executed\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';"

statistics_expected=$(
    printf '%b' \
        'PRIMARY\t1\tsource_uuid\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'PRIMARY\t2\tgtid_tag\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'PRIMARY\t3\tinterval_start\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.STATISTICS rows" \
    "$statistics_expected" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
            PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql.gtid_executed INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'gtid_executed\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\trow_format=DYNAMIC stats_persistent=0\t')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_gtid_executed_table_expectations: ok"
