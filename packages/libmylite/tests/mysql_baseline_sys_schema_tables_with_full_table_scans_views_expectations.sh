#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_tables_full_scans_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_tables_with_full_table_scans_views_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

formatted_show_columns_expected=$(
    printf '%b' \
'object_schema\tvarchar(64)\tYES\t\tNULL\t
object_name\tvarchar(64)\tYES\t\tNULL\t
rows_full_scanned\tbigint unsigned\tNO\t\tNULL\t
latency\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns_expected=$(
    printf '%b' \
'object_schema\tvarchar(64)\tYES\t\tNULL\t
object_name\tvarchar(64)\tYES\t\tNULL\t
rows_full_scanned\tbigint unsigned\tNO\t\tNULL\t
latency\tbigint unsigned\tNO\t\tNULL\t'
)

expect_output \
    "sys.schema_tables_with_full_table_scans SHOW COLUMNS" \
    "$formatted_show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_tables_with_full_table_scans;"

expect_output \
    "sys.x schema_tables_with_full_table_scans SHOW COLUMNS" \
    "$raw_show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$schema_tables_with_full_table_scans\`;"

expect_output \
    "sys.schema_tables_with_full_table_scans DESCRIBE" \
    "$formatted_show_columns_expected" \
    "DESCRIBE sys.schema_tables_with_full_table_scans;"

expect_output \
    "sys.x schema_tables_with_full_table_scans DESCRIBE" \
    "$raw_show_columns_expected" \
    "DESCRIBE sys.\`x\$schema_tables_with_full_table_scans\`;"

expect_output \
    "sys.schema_tables_with_full_table_scans SHOW INDEX" \
    "" \
    "SHOW INDEX FROM sys.schema_tables_with_full_table_scans;"

expect_output \
    "sys.x schema_tables_with_full_table_scans SHOW INDEX" \
    "" \
    "SHOW INDEX FROM sys.\`x\$schema_tables_with_full_table_scans\`;"

expect_output \
    "sys schema tables with full table scans INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'schema_tables_with_full_table_scans\tobject_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nschema_tables_with_full_table_scans\tobject_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nschema_tables_with_full_table_scans\trows_full_scanned\t3\tNO\tbigint unsigned\tNULL\tNULL\nschema_tables_with_full_table_scans\tlatency\t4\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nx$schema_tables_with_full_table_scans\tobject_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$schema_tables_with_full_table_scans\tobject_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$schema_tables_with_full_table_scans\trows_full_scanned\t3\tNO\tbigint unsigned\tNULL\tNULL\nx$schema_tables_with_full_table_scans\tlatency\t4\tNO\tbigint unsigned\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                           'x\$schema_tables_with_full_table_scans')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys schema tables with full table scans TABLES rows" \
    "$(printf '%b' 'sys\tschema_tables_with_full_table_scans\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$schema_tables_with_full_table_scans\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                           'x\$schema_tables_with_full_table_scans')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema tables with full table scans VIEWS rows" \
    "$(printf '%b' 'sys\tschema_tables_with_full_table_scans\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nsys\tx$schema_tables_with_full_table_scans\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                           'x\$schema_tables_with_full_table_scans')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema tables with full table scans dependency metadata" \
    "$(printf '%b' 'sys\tschema_tables_with_full_table_scans\tperformance_schema\ttable_io_waits_summary_by_index_usage\nsys\tx$schema_tables_with_full_table_scans\tperformance_schema\ttable_io_waits_summary_by_index_usage\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('schema_tables_with_full_table_scans',
                          'x\$schema_tables_with_full_table_scans')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                           'x\$schema_tables_with_full_table_scans');"

expect_output \
    "sys schema tables with full table scans empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                               'x\$schema_tables_with_full_table_scans')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                               'x\$schema_tables_with_full_table_scans')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                               'x\$schema_tables_with_full_table_scans')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_tables_with_full_table_scans',
                               'x\$schema_tables_with_full_table_scans'));"

run_mysql "CREATE TABLE scan_target (id INT PRIMARY KEY, value_col INT, filler VARCHAR(20));
           INSERT INTO scan_target VALUES (1, 10, 'a'), (2, 20, 'b'), (3, 20, 'c');
           SELECT COUNT(*) FROM scan_target WHERE value_col = 20;" \
    "$DATABASE" >/dev/null

expect_output \
    "sys.schema_tables_with_full_table_scans user row presence" \
    "$(printf '%b' '1\t1\t1\t1')" \
    "SELECT COUNT(*), MIN(rows_full_scanned >= 3), MIN(latency IS NOT NULL), MIN(latency <> '')
       FROM sys.schema_tables_with_full_table_scans
      WHERE object_schema = '${DATABASE}' AND object_name = 'scan_target';"

expect_output \
    "selected sys x schema_tables_with_full_table_scans read" \
    "$(printf '%b' '1\t1\t1')" \
    "USE sys;
     SELECT COUNT(*), MIN(rows_full_scanned >= 3), MIN(latency >= 0)
       FROM \`x\$schema_tables_with_full_table_scans\`
      WHERE object_schema = '${DATABASE}' AND object_name = 'scan_target';"

expect_contains \
    "sys.schema_tables_with_full_table_scans SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_tables_with_full_table_scans\`" \
    "SHOW CREATE VIEW sys.schema_tables_with_full_table_scans;"

expect_contains \
    "sys.schema_tables_with_full_table_scans SHOW CREATE VIEW definition" \
    "format_pico_time(\`performance_schema\`.\`table_io_waits_summary_by_index_usage\`.\`SUM_TIMER_WAIT\`) AS \`latency\`" \
    "SHOW CREATE VIEW sys.schema_tables_with_full_table_scans;"

expect_contains \
    "sys.x schema_tables_with_full_table_scans SHOW CREATE TABLE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$schema_tables_with_full_table_scans\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_tables_with_full_table_scans\`;"

expect_contains \
    "sys.x schema_tables_with_full_table_scans SHOW CREATE TABLE definition" \
    "\`performance_schema\`.\`table_io_waits_summary_by_index_usage\`.\`SUM_TIMER_WAIT\` AS \`latency\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_tables_with_full_table_scans\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_tables_with_full_table_scans WHERE object_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_tables_with_full_table_scans SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_tables_with_full_table_scans_views_expectations: ok"
