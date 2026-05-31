#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_ps_check_lost_instrumentation_view_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE 'ps_check_lost_instrumentation';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.ps_check_lost_instrumentation: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(
        printf '%b' 'ps_check_lost_instrumentation\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL'
    )
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.ps_check_lost_instrumentation: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *)
            fail "SHOW TABLE STATUS sys.ps_check_lost_instrumentation: expected Create_time datetime, got [$create_time]"
            ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.ps_check_lost_instrumentation: expected suffix [$expected_suffix], got [$suffix]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_columns_expected=$(
    printf '%b' \
'variable_name\tvarchar(64)\tNO\t\tNULL\t
variable_value\tvarchar(1024)\tYES\t\tNULL\t'
)

expect_output \
    "sys.ps_check_lost_instrumentation SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.ps_check_lost_instrumentation;"

expect_output \
    "sys.ps_check_lost_instrumentation DESCRIBE" \
    "$show_columns_expected" \
    "DESCRIBE sys.ps_check_lost_instrumentation;"

show_full_columns_expected=$(
    printf '%b' \
'variable_name\tvarchar(64)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t
variable_value\tvarchar(1024)\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t'
)

expect_output \
    "sys.ps_check_lost_instrumentation SHOW FULL COLUMNS" \
    "$show_full_columns_expected" \
    "SHOW FULL COLUMNS FROM sys.ps_check_lost_instrumentation;"

expect_output \
    "sys.ps_check_lost_instrumentation SHOW INDEX" \
    "" \
    "SHOW INDEX FROM sys.ps_check_lost_instrumentation;"

expect_output \
    "sys.ps_check_lost_instrumentation direct rows" \
    "0" \
    "SELECT COUNT(*) FROM sys.ps_check_lost_instrumentation;"

expect_output \
    "sys.ps_check_lost_instrumentation selected schema rows" \
    "0" \
    "USE sys; SELECT COUNT(*) FROM ps_check_lost_instrumentation;"

expect_output \
    "sys.ps_check_lost_instrumentation INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'ps_check_lost_instrumentation\tvariable_name\t1\tNO\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nps_check_lost_instrumentation\tvariable_value\t2\tYES\tvarchar(1024)\tutf8mb4\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'ps_check_lost_instrumentation'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.ps_check_lost_instrumentation TABLES row" \
    "$(printf '%b' 'sys\tps_check_lost_instrumentation\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'ps_check_lost_instrumentation';"

expect_show_table_status_row

expect_output \
    "sys.ps_check_lost_instrumentation VIEWS row" \
    "$(printf '%b' 'sys\tps_check_lost_instrumentation\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'ps_check_lost_instrumentation';"

expect_output \
    "sys.ps_check_lost_instrumentation dependency metadata" \
    "$(printf '%b' 'sys\tps_check_lost_instrumentation\tperformance_schema\tglobal_status\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME = 'ps_check_lost_instrumentation'
      ORDER BY TABLE_SCHEMA, TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'ps_check_lost_instrumentation';"

expect_output \
    "sys.ps_check_lost_instrumentation empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'ps_check_lost_instrumentation'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'ps_check_lost_instrumentation'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'ps_check_lost_instrumentation'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'ps_check_lost_instrumentation');"

expect_contains \
    "sys.ps_check_lost_instrumentation SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`ps_check_lost_instrumentation\`" \
    "SHOW CREATE VIEW sys.ps_check_lost_instrumentation;"

expect_contains \
    "sys.ps_check_lost_instrumentation SHOW CREATE VIEW name filter" \
    "(\`performance_schema\`.\`global_status\`.\`VARIABLE_NAME\` like 'perf%lost')" \
    "SHOW CREATE VIEW sys.ps_check_lost_instrumentation;"

expect_contains \
    "sys.ps_check_lost_instrumentation SHOW CREATE VIEW value filter" \
    "(\`performance_schema\`.\`global_status\`.\`VARIABLE_VALUE\` > 0)" \
    "SHOW CREATE VIEW sys.ps_check_lost_instrumentation;"

expect_contains \
    "sys.ps_check_lost_instrumentation SHOW CREATE TABLE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`ps_check_lost_instrumentation\`" \
    "USE sys; SHOW CREATE TABLE ps_check_lost_instrumentation;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.ps_check_lost_instrumentation; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.ps_check_lost_instrumentation SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_ps_check_lost_instrumentation_view_expectations: ok"
