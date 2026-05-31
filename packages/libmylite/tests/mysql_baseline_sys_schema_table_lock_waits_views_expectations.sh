#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_table_lock_waits_views_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_columns_expected=$(
    printf '%b' \
'object_schema\tvarchar(64)\tYES\t\tNULL\t
object_name\tvarchar(64)\tYES\t\tNULL\t
waiting_thread_id\tbigint unsigned\tNO\t\tNULL\t
waiting_pid\tbigint unsigned\tYES\t\tNULL\t
waiting_account\ttext\tYES\t\tNULL\t
waiting_lock_type\tvarchar(32)\tNO\t\tNULL\t
waiting_lock_duration\tvarchar(32)\tNO\t\tNULL\t
waiting_query\tlongtext\tYES\t\tNULL\t
waiting_query_secs\tbigint\tYES\t\tNULL\t
waiting_query_rows_affected\tbigint unsigned\tYES\t\tNULL\t
waiting_query_rows_examined\tbigint unsigned\tYES\t\tNULL\t
blocking_thread_id\tbigint unsigned\tNO\t\tNULL\t
blocking_pid\tbigint unsigned\tYES\t\tNULL\t
blocking_account\ttext\tYES\t\tNULL\t
blocking_lock_type\tvarchar(32)\tNO\t\tNULL\t
blocking_lock_duration\tvarchar(32)\tNO\t\tNULL\t
sql_kill_blocking_query\tvarchar(31)\tYES\t\tNULL\t
sql_kill_blocking_connection\tvarchar(25)\tYES\t\tNULL\t'
)

expect_output \
    "sys.schema_table_lock_waits SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_table_lock_waits;"

expect_output \
    "sys.x schema_table_lock_waits SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$schema_table_lock_waits\`;"

columns_expected=$(
    printf '%b' \
'schema_table_lock_waits\tobject_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\tobject_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\twaiting_thread_id\t3\tNO\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\twaiting_pid\t4\tYES\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\twaiting_account\t5\tYES\ttext\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\twaiting_lock_type\t6\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\twaiting_lock_duration\t7\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\twaiting_query\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\twaiting_query_secs\t9\tYES\tbigint\tNULL\tNULL
schema_table_lock_waits\twaiting_query_rows_affected\t10\tYES\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\twaiting_query_rows_examined\t11\tYES\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\tblocking_thread_id\t12\tNO\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\tblocking_pid\t13\tYES\tbigint unsigned\tNULL\tNULL
schema_table_lock_waits\tblocking_account\t14\tYES\ttext\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\tblocking_lock_type\t15\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\tblocking_lock_duration\t16\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\tsql_kill_blocking_query\t17\tYES\tvarchar(31)\tutf8mb4\tutf8mb4_0900_ai_ci
schema_table_lock_waits\tsql_kill_blocking_connection\t18\tYES\tvarchar(25)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tobject_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tobject_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\twaiting_thread_id\t3\tNO\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\twaiting_pid\t4\tYES\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\twaiting_account\t5\tYES\ttext\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\twaiting_lock_type\t6\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\twaiting_lock_duration\t7\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\twaiting_query\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\twaiting_query_secs\t9\tYES\tbigint\tNULL\tNULL
x$schema_table_lock_waits\twaiting_query_rows_affected\t10\tYES\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\twaiting_query_rows_examined\t11\tYES\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\tblocking_thread_id\t12\tNO\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\tblocking_pid\t13\tYES\tbigint unsigned\tNULL\tNULL
x$schema_table_lock_waits\tblocking_account\t14\tYES\ttext\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tblocking_lock_type\t15\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tblocking_lock_duration\t16\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tsql_kill_blocking_query\t17\tYES\tvarchar(31)\tutf8mb4\tutf8mb4_0900_ai_ci
x$schema_table_lock_waits\tsql_kill_blocking_connection\t18\tYES\tvarchar(25)\tutf8mb4\tutf8mb4_0900_ai_ci'
)

expect_output \
    "sys table lock waits INFORMATION_SCHEMA.COLUMNS" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys table lock waits INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tschema_table_lock_waits\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$schema_table_lock_waits\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys table lock waits INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tschema_table_lock_waits\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$schema_table_lock_waits\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys table lock waits dependency metadata" \
    "$(printf '%b' 'sys\tschema_table_lock_waits\tperformance_schema\tevents_statements_current\nsys\tschema_table_lock_waits\tperformance_schema\tmetadata_locks\nsys\tschema_table_lock_waits\tperformance_schema\tthreads\nsys\tschema_table_lock_waits\tsys\tsys_config\nsys\tx$schema_table_lock_waits\tperformance_schema\tevents_statements_current\nsys\tx$schema_table_lock_waits\tperformance_schema\tmetadata_locks\nsys\tx$schema_table_lock_waits\tperformance_schema\tthreads')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys table lock waits routine dependency metadata" \
    "$(printf '%b' 'sys\tschema_table_lock_waits\tsys\tformat_statement\nsys\tschema_table_lock_waits\tsys\tps_thread_account\nsys\tx$schema_table_lock_waits\tsys\tps_thread_account')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys table lock waits empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_table_lock_waits', 'x\$schema_table_lock_waits'));"

expect_output \
    "sys table lock waits empty rows" \
    "$(printf '%b' '0\n0')" \
    "SELECT COUNT(*) FROM sys.schema_table_lock_waits;
     SELECT COUNT(*) FROM sys.\`x\$schema_table_lock_waits\`;"

expect_output \
    "selected sys table lock waits empty rows" \
    "$(printf '%b' '0\n0')" \
    "USE sys;
     SELECT COUNT(*) FROM schema_table_lock_waits;
     SELECT COUNT(*) FROM \`x\$schema_table_lock_waits\`;"

expect_contains \
    "sys.schema_table_lock_waits SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_table_lock_waits\`" \
    "SHOW CREATE VIEW sys.schema_table_lock_waits;"

expect_contains \
    "sys.schema_table_lock_waits formatted query marker" \
    "sys\`.\`format_statement\`(\`pt\`.\`PROCESSLIST_INFO\`) AS \`waiting_query\`" \
    "SHOW CREATE VIEW sys.schema_table_lock_waits;"

expect_contains \
    "sys.x schema_table_lock_waits SHOW CREATE TABLE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$schema_table_lock_waits\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_table_lock_waits\`;"

expect_contains \
    "sys.x schema_table_lock_waits raw query marker" \
    "\`pt\`.\`PROCESSLIST_INFO\` AS \`waiting_query\`" \
    "SHOW CREATE VIEW sys.\`x\$schema_table_lock_waits\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_table_lock_waits; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_table_lock_waits SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_table_lock_waits_views_expectations: ok"
