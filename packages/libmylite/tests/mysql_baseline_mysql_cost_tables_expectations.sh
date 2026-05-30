#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_cost_tables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_show_table_status_row() {
    table_name=$1
    expected_rows=$2
    expected_avg_row_length=$3

    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE '${table_name}';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    update_time=$(printf '%s\n' "$output" | cut -f 13)
    stable_tail=$(printf '%s\n' "$output" | cut -f 14-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' \
        "${table_name}\tInnoDB\t10\tDynamic\t${expected_rows}\t${expected_avg_row_length}\t16384\t0\t0\t4194304\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS ${table_name}: expected Create_time datetime, got [$create_time]" ;;
    esac
    expect_value "SHOW TABLE STATUS ${table_name} update time" "NULL" "$update_time"
    expected_tail=$(printf '%b' \
        "NULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\t")
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

server_rows=$(run_mysql "SELECT cost_name, COALESCE(CAST(cost_value AS CHAR), 'NULL'),
        last_update IS NOT NULL, comment, COALESCE(CAST(default_value AS CHAR), 'NULL')
   FROM mysql.server_cost
  ORDER BY cost_name;")
expected_server_rows="disk_temptable_create_cost	NULL	1	NULL	20
disk_temptable_row_cost	NULL	1	NULL	0.5
key_compare_cost	NULL	1	NULL	0.05
memory_temptable_create_cost	NULL	1	NULL	1
memory_temptable_row_cost	NULL	1	NULL	0.1
row_evaluate_cost	NULL	1	NULL	0.1"
expect_value "mysql.server_cost rows" "$expected_server_rows" "$server_rows"

engine_rows=$(run_mysql "SELECT engine_name, device_type, cost_name,
        COALESCE(CAST(cost_value AS CHAR), 'NULL'), last_update IS NOT NULL,
        comment, COALESCE(CAST(default_value AS CHAR), 'NULL')
   FROM mysql.engine_cost
  ORDER BY engine_name, device_type, cost_name;")
expected_engine_rows="default	0	io_block_read_cost	NULL	1	NULL	1
default	0	memory_block_read_cost	NULL	1	NULL	0.25"
expect_value "mysql.engine_cost rows" "$expected_engine_rows" "$engine_rows"

expect_value \
    "mysql.server_cost count" \
    "6" \
    "$(run_mysql "SELECT COUNT(*) FROM mysql.server_cost;")"
expect_value \
    "mysql.engine_cost count" \
    "2" \
    "$(run_mysql "SELECT COUNT(*) FROM mysql.engine_cost;")"

expected_server_names="disk_temptable_create_cost
disk_temptable_row_cost
key_compare_cost
memory_temptable_create_cost
memory_temptable_row_cost
row_evaluate_cost"
expect_value \
    "mysql.server_cost unqualified filtered rows" \
    "$expected_server_names" \
    "$(run_mysql "USE mysql; SELECT cost_name FROM server_cost WHERE default_value IS NOT NULL ORDER BY cost_name;")"

server_columns_expected=$(
    printf '%b' \
        'cost_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'cost_value\tfloat\tYES\t\tNULL\t\n' \
        'last_update\ttimestamp\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n' \
        'comment\tvarchar(1024)\tYES\t\tNULL\t\n' \
        'default_value\tfloat\tYES\t\tNULL\tVIRTUAL GENERATED'
)
expect_value \
    "mysql.server_cost SHOW COLUMNS rows" \
    "$server_columns_expected" \
    "$(run_mysql "SHOW COLUMNS FROM mysql.server_cost;")"

engine_columns_expected=$(
    printf '%b' \
        'engine_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'device_type\tint\tNO\tPRI\tNULL\t\n' \
        'cost_name\tvarchar(64)\tNO\tPRI\tNULL\t\n' \
        'cost_value\tfloat\tYES\t\tNULL\t\n' \
        'last_update\ttimestamp\tNO\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n' \
        'comment\tvarchar(1024)\tYES\t\tNULL\t\n' \
        'default_value\tfloat\tYES\t\tNULL\tVIRTUAL GENERATED'
)
expect_value \
    "mysql.engine_cost SHOW COLUMNS rows" \
    "$engine_columns_expected" \
    "$(run_mysql "SHOW COLUMNS FROM mysql.engine_cost;")"

expect_value \
    "mysql.server_cost unqualified DESC rows" \
    "$server_columns_expected" \
    "$(run_mysql "USE mysql; DESC server_cost;")"

show_index_expected=$(
    printf '%b' \
        'engine_cost\t0\tPRIMARY\t1\tcost_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'engine_cost\t0\tPRIMARY\t2\tengine_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'engine_cost\t0\tPRIMARY\t3\tdevice_type\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'server_cost\t0\tPRIMARY\t1\tcost_name\tA\t6\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
actual_show_index=$(
    run_mysql "SHOW INDEX FROM mysql.engine_cost; SHOW INDEX FROM mysql.server_cost;"
)
expect_value "mysql cost tables SHOW INDEX rows" "$show_index_expected" "$actual_show_index"

columns_generated_expected=$(
    printf '%b' \
        'engine_cost\tdefault_value\tVIRTUAL GENERATED\t(case `cost_name` when _utf8mb4\\'\''io_block_read_cost\\'\'' then 1.0 when _utf8mb4\\'\''memory_block_read_cost\\'\'' then 0.25 else NULL end)\n' \
        'server_cost\tdefault_value\tVIRTUAL GENERATED\t(case `cost_name` when _utf8mb4\\'\''disk_temptable_create_cost\\'\'' then 20.0 when _utf8mb4\\'\''disk_temptable_row_cost\\'\'' then 0.5 when _utf8mb4\\'\''key_compare_cost\\'\'' then 0.05 when _utf8mb4\\'\''memory_temptable_create_cost\\'\'' then 1.0 when _utf8mb4\\'\''memory_temptable_row_cost\\'\'' then 0.1 when _utf8mb4\\'\''row_evaluate_cost\\'\'' then 0.1 else NULL end)'
)
expect_value \
    "mysql cost tables generated column metadata" \
    "$columns_generated_expected" \
    "$(run_mysql "SELECT TABLE_NAME, COLUMN_NAME, EXTRA, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
        AND COLUMN_NAME = 'default_value'
      ORDER BY TABLE_NAME;")"

expect_value \
    "mysql cost tables TABLE_CONSTRAINTS rows" \
    "engine_cost	PRIMARY	PRIMARY KEY	YES
server_cost	PRIMARY	PRIMARY KEY	YES" \
    "$(run_mysql "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
      ORDER BY TABLE_NAME;")"

expect_value \
    "mysql cost tables KEY_COLUMN_USAGE rows" \
    "engine_cost	PRIMARY	cost_name	1	NULL	NULL	NULL	NULL
engine_cost	PRIMARY	engine_name	2	NULL	NULL	NULL	NULL
engine_cost	PRIMARY	device_type	3	NULL	NULL	NULL	NULL
server_cost	PRIMARY	cost_name	1	NULL	NULL	NULL	NULL" \
    "$(run_mysql "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;")"

expect_value \
    "mysql cost tables TABLE_CONSTRAINTS_EXTENSIONS rows" \
    "engine_cost	PRIMARY	NULL	NULL
server_cost	PRIMARY	NULL	NULL" \
    "$(run_mysql "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE,
            SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
      ORDER BY TABLE_NAME;")"

statistics_expected=$(
    printf '%b' \
        'engine_cost\tPRIMARY\t1\tcost_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'engine_cost\tPRIMARY\t2\tengine_name\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'engine_cost\tPRIMARY\t3\tdevice_type\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'server_cost\tPRIMARY\t1\tcost_name\tA\t6\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_value \
    "mysql cost tables INFORMATION_SCHEMA.STATISTICS rows" \
    "$statistics_expected" \
    "$(run_mysql "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT,
            INDEX_COMMENT, IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;")"

tables_expected=$(
    printf '%b' \
        'engine_cost\tBASE TABLE\tInnoDB\t10\tDynamic\t2\t8192\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\t\n' \
        'server_cost\tBASE TABLE\tInnoDB\t10\tDynamic\t6\t2730\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\t'
)
expect_value \
    "mysql cost tables INFORMATION_SCHEMA.TABLES rows" \
    "$tables_expected" \
    "$(run_mysql "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('server_cost', 'engine_cost')
      ORDER BY TABLE_NAME;")"

expect_show_table_status_row "engine_cost" "2" "8192"
expect_show_table_status_row "server_cost" "6" "2730"

status=$(run_mysql "SELECT cost_name FROM mysql.server_cost ORDER BY cost_name LIMIT 1; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "mysql cost tables warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_mysql_cost_tables_expectations: ok"
