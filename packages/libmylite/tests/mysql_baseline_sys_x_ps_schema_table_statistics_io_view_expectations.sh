#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_x_ps_schema_table_statistics_io_view_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
}

expect_show_table_status_row() {
    table_name=$1
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE '$table_name';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "$table_name\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS sys.$table_name: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected suffix [$expected_suffix], got [$suffix]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tYES\t\tNULL\t
table_name\tvarchar(64)\tYES\t\tNULL\t
count_read\tdecimal(42,0)\tYES\t\tNULL\t
sum_number_of_bytes_read\tdecimal(41,0)\tYES\t\tNULL\t
sum_timer_read\tdecimal(42,0)\tYES\t\tNULL\t
count_write\tdecimal(42,0)\tYES\t\tNULL\t
sum_number_of_bytes_write\tdecimal(41,0)\tYES\t\tNULL\t
sum_timer_write\tdecimal(42,0)\tYES\t\tNULL\t
count_misc\tdecimal(42,0)\tYES\t\tNULL\t
sum_timer_misc\tdecimal(42,0)\tYES\t\tNULL\t'
)

expect_output \
    "sys.x ps schema table statistics io SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$ps_schema_table_statistics_io\`;"

expect_output \
    "sys.x ps schema table statistics io DESCRIBE" \
    "$show_columns_expected" \
    "DESCRIBE sys.\`x\$ps_schema_table_statistics_io\`;"

expect_output \
    "sys.x ps schema table statistics io SHOW FULL columns" \
    "$(printf '%b' 'table_schema\tvarchar(64)\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncount_read\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nsum_number_of_bytes_read\tdecimal(41,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nsum_timer_misc\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$ps_schema_table_statistics_io\`
      WHERE Field IN ('table_schema', 'count_read', 'sum_number_of_bytes_read', 'sum_timer_misc');"

expect_output \
    "sys.x ps schema table statistics io SHOW INDEX" \
    "" \
    "SHOW INDEX FROM sys.\`x\$ps_schema_table_statistics_io\`;"

expect_output \
    "sys.x ps schema table statistics io has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$ps_schema_table_statistics_io\`;"

expect_output \
    "sys.x ps schema table statistics io selected schema rows" \
    "1" \
    "USE sys;
     SELECT COUNT(*) > 0 FROM \`x\$ps_schema_table_statistics_io\`;"

expect_output \
    "sys.x ps schema table statistics io INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'x$ps_schema_table_statistics_io\ttable_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$ps_schema_table_statistics_io\ttable_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$ps_schema_table_statistics_io\tcount_read\t3\tYES\tdecimal(42,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tsum_number_of_bytes_read\t4\tYES\tdecimal(41,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tsum_timer_read\t5\tYES\tdecimal(42,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tcount_write\t6\tYES\tdecimal(42,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tsum_number_of_bytes_write\t7\tYES\tdecimal(41,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tsum_timer_write\t8\tYES\tdecimal(42,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tcount_misc\t9\tYES\tdecimal(42,0)\tNULL\tNULL\nx$ps_schema_table_statistics_io\tsum_timer_misc\t10\tYES\tdecimal(42,0)\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$ps_schema_table_statistics_io'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.x ps schema table statistics io INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tx$ps_schema_table_statistics_io\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$ps_schema_table_statistics_io';"

expect_show_table_status_row "x\$ps_schema_table_statistics_io"

expect_output \
    "sys.x ps schema table statistics io INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tx$ps_schema_table_statistics_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$ps_schema_table_statistics_io';"

expect_output \
    "sys.x ps schema table statistics io table dependency metadata" \
    "$(printf '%b' 'sys\tx$ps_schema_table_statistics_io\tperformance_schema\tfile_summary_by_instance')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME = 'x\$ps_schema_table_statistics_io'
      ORDER BY TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys.x ps schema table statistics io routine dependency metadata" \
    "$(printf '%b' 'sys\tx$ps_schema_table_statistics_io\tsys\textract_schema_from_file_name\nsys\tx$ps_schema_table_statistics_io\tsys\textract_table_from_file_name')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$ps_schema_table_statistics_io'
      ORDER BY SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys.x ps schema table statistics io empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_schema_table_statistics_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_schema_table_statistics_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_schema_table_statistics_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_schema_table_statistics_io');"

expect_contains \
    "sys.x ps schema table statistics io SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$ps_schema_table_statistics_io\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_schema_table_statistics_io\`;"

expect_contains \
    "sys.x ps schema table statistics io SHOW CREATE source" \
    "from \`performance_schema\`.\`file_summary_by_instance\` group by \`table_schema\`,\`table_name\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_schema_table_statistics_io\`;"

expect_contains \
    "sys.x ps schema table statistics io SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$ps_schema_table_statistics_io\`" \
    "USE sys; SHOW CREATE TABLE \`x\$ps_schema_table_statistics_io\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$ps_schema_table_statistics_io\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.x ps schema table statistics io SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_x_ps_schema_table_statistics_io_view_expectations: ok"
