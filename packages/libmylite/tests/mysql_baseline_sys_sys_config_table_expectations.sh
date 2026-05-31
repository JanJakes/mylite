#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
PRIVILEGES="select,insert,update,references"

fail() {
    printf '%s\n' "mysql_baseline_sys_sys_config_table_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE 'sys_config';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    update_time=$(printf '%s\n' "$output" | cut -f 13)
    check_time=$(printf '%s\n' "$output" | cut -f 14)
    collation=$(printf '%s\n' "$output" | cut -f 15)
    checksum=$(printf '%s\n' "$output" | cut -f 16)
    create_options=$(printf '%s\n' "$output" | cut -f 17)
    comment=$(printf '%s\n' "$output" | cut -f 18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys_config: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(
        printf '%b' 'sys_config\tInnoDB\t10\tDynamic\t6\t2730\t16384\t0\t0\t0\tNULL'
    )
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys_config: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS sys_config: expected Create_time datetime, got [$create_time]" ;;
    esac
    if [ "$update_time" != "NULL" ]; then
        fail "SHOW TABLE STATUS sys_config: expected NULL Update_time, got [$update_time]"
    fi
    if [ "$check_time" != "NULL" ]; then
        fail "SHOW TABLE STATUS sys_config: expected NULL Check_time, got [$check_time]"
    fi
    if [ "$collation" != "utf8mb4_0900_ai_ci" ]; then
        fail "SHOW TABLE STATUS sys_config: expected utf8mb4_0900_ai_ci collation, got [$collation]"
    fi
    if [ "$checksum" != "NULL" ]; then
        fail "SHOW TABLE STATUS sys_config: expected NULL Checksum, got [$checksum]"
    fi
    if [ -n "$create_options" ]; then
        fail "SHOW TABLE STATUS sys_config: expected empty Create_options, got [$create_options]"
    fi
    if [ -n "$comment" ]; then
        fail "SHOW TABLE STATUS sys_config: expected empty Comment, got [$comment]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "sys.sys_config target row count" \
    "6" \
    "SELECT COUNT(*) FROM sys.sys_config;"

default_rows_expected=$(
    printf '%b' \
        'diagnostics.allow_i_s_tables\tOFF\t1\tNULL\n' \
        'diagnostics.include_raw\tOFF\t1\tNULL\n' \
        'ps_thread_trx_info.max_length\t65535\t1\tNULL\n' \
        'statement_performance_analyzer.limit\t100\t1\tNULL\n' \
        'statement_performance_analyzer.view\tNULL\t1\tNULL\n' \
        'statement_truncate_len\t64\t1\tNULL'
)
expect_output \
    "sys.sys_config default rows" \
    "$default_rows_expected" \
    "SELECT variable, value, set_time IS NOT NULL, set_by
       FROM sys.sys_config
      ORDER BY variable;"

expect_output \
    "sys.sys_config selected-schema read" \
    "64" \
    "USE sys; SELECT value FROM sys_config WHERE variable = 'statement_truncate_len';"

show_columns_expected=$(
    {
        printf '%b\n' 'variable\tvarchar(128)\tNO\tPRI\tNULL\t'
        printf '%b\n' 'value\tvarchar(128)\tYES\t\tNULL\t'
        printf '%b\n' 'set_time\ttimestamp\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP'
        printf '%b\n' 'set_by\tvarchar(128)\tYES\t\tNULL\t'
    }
)
expect_output \
    "sys.sys_config SHOW COLUMNS rows" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.sys_config;"

full_columns_expected=$(
    {
        printf '%b\n' "variable\tvarchar(128)\tutf8mb4_0900_ai_ci\tNO\tPRI\tNULL\t\t$PRIVILEGES\t"
        printf '%b\n' "value\tvarchar(128)\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\t$PRIVILEGES\t"
        printf '%b\n' "set_time\ttimestamp\tNULL\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\t$PRIVILEGES\t"
        printf '%b\n' "set_by\tvarchar(128)\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\t$PRIVILEGES\t"
    }
)
expect_output \
    "sys.sys_config SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM sys.sys_config;"

expect_output \
    "sys.sys_config SHOW INDEX row" \
    "$(printf '%b' 'sys_config\t0\tPRIMARY\t1\tvariable\tA\t6\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SHOW INDEX FROM sys.sys_config;"

information_schema_columns_expected=$(
    {
        printf '%b\n' "variable\t1\tNULL\tNO\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\tPRI\t\t$PRIVILEGES\t\t"
        printf '%b\n' "value\t2\tNULL\tYES\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\t\t\t$PRIVILEGES\t\t"
        printf '%b\n' "set_time\t3\tCURRENT_TIMESTAMP\tYES\ttimestamp\tNULL\tNULL\tNULL\tNULL\t0\tNULL\tNULL\ttimestamp\t\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\t$PRIVILEGES\t\t"
        printf '%b\n' "set_by\t4\tNULL\tYES\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\t\t\t$PRIVILEGES\t\t"
    }
)
expect_output \
    "sys.sys_config INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.sys_config INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_output \
    "sys.sys_config INFORMATION_SCHEMA.KEY_COLUMN_USAGE row" \
    "$(printf '%b' 'PRIMARY\tvariable\t1\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_output \
    "sys.sys_config INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tsys_config\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_output \
    "sys.sys_config INFORMATION_SCHEMA.STATISTICS row" \
    "$(printf '%b' 'PRIMARY\t1\tvariable\tA\t6\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY,
            SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
            IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_output \
    "sys.sys_config INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'sys_config\tBASE TABLE\tInnoDB\t10\tDynamic\t6\t2730\t16384\t0\t0\t0\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t\t')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_sys_sys_config_table_expectations: ok"
