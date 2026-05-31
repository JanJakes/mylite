#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_io_global_by_file_by_latency_views_expectations: $1" >&2
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

formatted_show_columns=$(
    printf '%b' \
'file\tvarchar(512)\tYES\t\tNULL\t
total\tbigint unsigned\tNO\t\tNULL\t
total_latency\tvarchar(11)\tYES\t\tNULL\t
count_read\tbigint unsigned\tNO\t\tNULL\t
read_latency\tvarchar(11)\tYES\t\tNULL\t
count_write\tbigint unsigned\tNO\t\tNULL\t
write_latency\tvarchar(11)\tYES\t\tNULL\t
count_misc\tbigint unsigned\tNO\t\tNULL\t
misc_latency\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'file\tvarchar(512)\tNO\t\tNULL\t
total\tbigint unsigned\tNO\t\tNULL\t
total_latency\tbigint unsigned\tNO\t\tNULL\t
count_read\tbigint unsigned\tNO\t\tNULL\t
read_latency\tbigint unsigned\tNO\t\tNULL\t
count_write\tbigint unsigned\tNO\t\tNULL\t
write_latency\tbigint unsigned\tNO\t\tNULL\t
count_misc\tbigint unsigned\tNO\t\tNULL\t
misc_latency\tbigint unsigned\tNO\t\tNULL\t'
)

expect_output \
    "sys io global by file by latency SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.io_global_by_file_by_latency;"

expect_output \
    "sys x io global by file by latency SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$io_global_by_file_by_latency\`;"

expect_output \
    "sys io global by file by latency SHOW FULL columns" \
    "$(printf '%b' 'file\tvarchar(512)\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nread_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nmisc_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.io_global_by_file_by_latency
      WHERE Field IN ('file', 'total_latency', 'read_latency', 'misc_latency');"

expect_output \
    "sys x io global by file by latency SHOW FULL columns" \
    "$(printf '%b' 'file\tvarchar(512)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\ntotal_latency\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\nread_latency\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\nmisc_latency\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$io_global_by_file_by_latency\`
      WHERE Field IN ('file', 'total_latency', 'read_latency', 'misc_latency');"

expect_output \
    "sys io global by file by latency INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'io_global_by_file_by_latency\tfile\t1\tYES\tvarchar(512)\tutf8mb4\tutf8mb4_0900_ai_ci\nio_global_by_file_by_latency\ttotal\t2\tNO\tbigint unsigned\tNULL\tNULL\nio_global_by_file_by_latency\ttotal_latency\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nio_global_by_file_by_latency\tcount_read\t4\tNO\tbigint unsigned\tNULL\tNULL\nio_global_by_file_by_latency\tread_latency\t5\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nio_global_by_file_by_latency\tcount_write\t6\tNO\tbigint unsigned\tNULL\tNULL\nio_global_by_file_by_latency\twrite_latency\t7\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nio_global_by_file_by_latency\tcount_misc\t8\tNO\tbigint unsigned\tNULL\tNULL\nio_global_by_file_by_latency\tmisc_latency\t9\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nx$io_global_by_file_by_latency\tfile\t1\tNO\tvarchar(512)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$io_global_by_file_by_latency\ttotal\t2\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\ttotal_latency\t3\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\tcount_read\t4\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\tread_latency\t5\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\tcount_write\t6\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\twrite_latency\t7\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\tcount_misc\t8\tNO\tbigint unsigned\tNULL\tNULL\nx$io_global_by_file_by_latency\tmisc_latency\t9\tNO\tbigint unsigned\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('io_global_by_file_by_latency', 'x\$io_global_by_file_by_latency')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys io global by file by latency INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tio_global_by_file_by_latency\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$io_global_by_file_by_latency\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('io_global_by_file_by_latency', 'x\$io_global_by_file_by_latency')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "io_global_by_file_by_latency"
expect_show_table_status_row "x\$io_global_by_file_by_latency"

expect_output \
    "sys io global by file by latency INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tio_global_by_file_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nsys\tx$io_global_by_file_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('io_global_by_file_by_latency', 'x\$io_global_by_file_by_latency')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys io global by file by latency table dependency metadata" \
    "$(printf '%b' 'sys\tio_global_by_file_by_latency\tperformance_schema\tfile_summary_by_instance\nsys\tio_global_by_file_by_latency\tperformance_schema\tglobal_variables\nsys\tx$io_global_by_file_by_latency\tperformance_schema\tfile_summary_by_instance')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('io_global_by_file_by_latency', 'x\$io_global_by_file_by_latency')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys io global by file by latency routine dependency metadata" \
    "$(printf '%b' 'sys\tio_global_by_file_by_latency\tsys\tformat_path')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('io_global_by_file_by_latency', 'x\$io_global_by_file_by_latency')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys io global by file by latency empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'io_global_by_file_by_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$io_global_by_file_by_latency');"

expect_contains \
    "sys io global by file by latency SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`io_global_by_file_by_latency\`" \
    "SHOW CREATE VIEW sys.io_global_by_file_by_latency;"

expect_contains \
    "sys io global by file by latency SHOW CREATE source" \
    "from \`performance_schema\`.\`file_summary_by_instance\` order by" \
    "SHOW CREATE VIEW sys.io_global_by_file_by_latency;"

expect_contains \
    "sys x io global by file by latency SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$io_global_by_file_by_latency\`" \
    "SHOW CREATE VIEW sys.\`x\$io_global_by_file_by_latency\`;"

expect_contains \
    "sys io global by file by latency SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`io_global_by_file_by_latency\`" \
    "USE sys; SHOW CREATE TABLE io_global_by_file_by_latency;"

expect_contains \
    "sys x io global by file by latency SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$io_global_by_file_by_latency\`" \
    "USE sys; SHOW CREATE TABLE \`x\$io_global_by_file_by_latency\`;"

expect_output \
    "sys io global by file by latency has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.io_global_by_file_by_latency;"

expect_output \
    "sys x io global by file by latency has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$io_global_by_file_by_latency\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.io_global_by_file_by_latency; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys io global by file by latency SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$io_global_by_file_by_latency\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x io global by file by latency SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_io_global_by_file_by_latency_views_expectations: ok"
