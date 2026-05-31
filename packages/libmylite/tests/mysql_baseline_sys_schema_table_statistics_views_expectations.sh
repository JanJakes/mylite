#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_table_statistics_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_table_statistics_views_expectations: $1" >&2
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
'table_schema\tvarchar(64)\tYES\t\tNULL\t
table_name\tvarchar(64)\tYES\t\tNULL\t
total_latency\tvarchar(11)\tYES\t\tNULL\t
rows_fetched\tbigint unsigned\tNO\t\tNULL\t
fetch_latency\tvarchar(11)\tYES\t\tNULL\t
rows_inserted\tbigint unsigned\tNO\t\tNULL\t
insert_latency\tvarchar(11)\tYES\t\tNULL\t
rows_updated\tbigint unsigned\tNO\t\tNULL\t
update_latency\tvarchar(11)\tYES\t\tNULL\t
rows_deleted\tbigint unsigned\tNO\t\tNULL\t
delete_latency\tvarchar(11)\tYES\t\tNULL\t
io_read_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_read\tvarchar(11)\tYES\t\tNULL\t
io_read_latency\tvarchar(11)\tYES\t\tNULL\t
io_write_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_write\tvarchar(11)\tYES\t\tNULL\t
io_write_latency\tvarchar(11)\tYES\t\tNULL\t
io_misc_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_misc_latency\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tYES\t\tNULL\t
table_name\tvarchar(64)\tYES\t\tNULL\t
total_latency\tbigint unsigned\tNO\t\tNULL\t
rows_fetched\tbigint unsigned\tNO\t\tNULL\t
fetch_latency\tbigint unsigned\tNO\t\tNULL\t
rows_inserted\tbigint unsigned\tNO\t\tNULL\t
insert_latency\tbigint unsigned\tNO\t\tNULL\t
rows_updated\tbigint unsigned\tNO\t\tNULL\t
update_latency\tbigint unsigned\tNO\t\tNULL\t
rows_deleted\tbigint unsigned\tNO\t\tNULL\t
delete_latency\tbigint unsigned\tNO\t\tNULL\t
io_read_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_read\tdecimal(41,0)\tYES\t\tNULL\t
io_read_latency\tdecimal(42,0)\tYES\t\tNULL\t
io_write_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_write\tdecimal(41,0)\tYES\t\tNULL\t
io_write_latency\tdecimal(42,0)\tYES\t\tNULL\t
io_misc_requests\tdecimal(42,0)\tYES\t\tNULL\t
io_misc_latency\tdecimal(42,0)\tYES\t\tNULL\t'
)

expect_output \
    "sys.schema_table_statistics SHOW COLUMNS" \
    "$formatted_show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_table_statistics;"

expect_output \
    "sys.x schema_table_statistics SHOW COLUMNS" \
    "$raw_show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$schema_table_statistics\`;"

expect_output \
    "sys schema table statistics INFORMATION_SCHEMA.COLUMNS samples" \
    "$(printf '%b' 'schema_table_statistics\ttable_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nschema_table_statistics\ttotal_latency\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nschema_table_statistics\trows_fetched\t4\tNO\tbigint unsigned\tNULL\tNULL\nschema_table_statistics\tio_read\t13\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nschema_table_statistics\tio_write_requests\t15\tYES\tdecimal(42,0)\tNULL\tNULL\nx$schema_table_statistics\ttable_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$schema_table_statistics\ttotal_latency\t3\tNO\tbigint unsigned\tNULL\tNULL\nx$schema_table_statistics\trows_fetched\t4\tNO\tbigint unsigned\tNULL\tNULL\nx$schema_table_statistics\tio_read\t13\tYES\tdecimal(41,0)\tNULL\tNULL\nx$schema_table_statistics\tio_write_requests\t15\tYES\tdecimal(42,0)\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_statistics', 'x\$schema_table_statistics')
        AND COLUMN_NAME IN ('table_schema', 'total_latency', 'rows_fetched',
                            'io_read', 'io_write_requests')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys schema table statistics TABLES rows" \
    "$(printf '%b' 'sys\tschema_table_statistics\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$schema_table_statistics\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_statistics', 'x\$schema_table_statistics')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema table statistics VIEWS rows" \
    "$(printf '%b' 'sys\tschema_table_statistics\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$schema_table_statistics\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_statistics', 'x\$schema_table_statistics')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema table statistics dependency metadata" \
    "$(printf '%b' 'sys\tschema_table_statistics\tperformance_schema\ttable_io_waits_summary_by_table\nsys\tschema_table_statistics\tsys\tx$ps_schema_table_statistics_io\nsys\tx$schema_table_statistics\tperformance_schema\ttable_io_waits_summary_by_table\nsys\tx$schema_table_statistics\tsys\tx$ps_schema_table_statistics_io\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('schema_table_statistics', 'x\$schema_table_statistics')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_statistics', 'x\$schema_table_statistics');"

expect_output \
    "sys schema table statistics empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_table_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_table_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_table_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_table_statistics');"

run_mysql "CREATE TABLE base_one (id INT PRIMARY KEY, a INT, b INT, KEY idx_a (a));
           SELECT COUNT(*)
             FROM sys.schema_table_statistics
            WHERE table_schema = '${DATABASE}' AND table_name = 'base_one';
           INSERT INTO base_one VALUES (1, 10, 100), (2, 20, 200);
           SELECT * FROM base_one WHERE a = 10;
           UPDATE base_one SET b = b + 1 WHERE id = 2;
           DELETE FROM base_one WHERE id = 1;" \
    "$DATABASE" >/dev/null

expect_output \
    "sys.schema_table_statistics user row presence" \
    "$(printf '%b' '1\t1\t1\t1\t1')" \
    "SELECT COUNT(*), MIN(rows_fetched >= 1), MIN(rows_inserted >= 2),
            MIN(rows_updated >= 1), MIN(rows_deleted >= 1)
       FROM sys.schema_table_statistics
      WHERE table_schema = '${DATABASE}' AND table_name = 'base_one';"

expect_output \
    "selected sys x schema_table_statistics read" \
    "$(printf '%b' "1\t1\t1")" \
    "USE sys;
     SELECT COUNT(*), MIN(rows_inserted >= 2), MIN(rows_deleted >= 1)
       FROM \`x\$schema_table_statistics\`
      WHERE table_schema = '${DATABASE}' AND table_name = 'base_one';"

expect_contains \
    "sys.schema_table_statistics SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_table_statistics\`" \
    "SHOW CREATE VIEW sys.schema_table_statistics;"

expect_contains \
    "sys.schema_table_statistics SHOW CREATE VIEW definition" \
    "format_pico_time(\`pst\`.\`SUM_TIMER_WAIT\`) AS \`total_latency\`" \
    "SHOW CREATE VIEW sys.schema_table_statistics;"

expect_contains \
    "sys.x schema_table_statistics SHOW CREATE TABLE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$schema_table_statistics\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_table_statistics\`;"

expect_contains \
    "sys.x schema_table_statistics SHOW CREATE TABLE definition" \
    "\`pst\`.\`SUM_TIMER_WAIT\` AS \`total_latency\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_table_statistics\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_table_statistics WHERE table_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_table_statistics SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_table_statistics_views_expectations: ok"
