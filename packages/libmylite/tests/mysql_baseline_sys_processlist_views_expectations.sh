#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_processlist_views_expectations: $1" >&2
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

show_columns_formatted_expected=$(printf '%b' \
    "execution_engine\tenum('PRIMARY','SECONDARY')\tYES\t\tNULL\t\n\
statement_latency\tvarchar(11)\tYES\t\tNULL\t\n\
current_memory\tvarchar(11)\tYES\t\tNULL\t\n\
last_wait_latency\tvarchar(13)\tYES\t\tNULL\t\n\
trx_state\tenum('ACTIVE','COMMITTED','ROLLED BACK')\tYES\t\tNULL\t\n\
trx_autocommit\tenum('YES','NO')\tYES\t\tNULL\t")

expect_output \
    "sys.processlist SHOW COLUMNS" \
    "$show_columns_formatted_expected" \
    "SHOW COLUMNS FROM sys.processlist
      WHERE Field IN ('execution_engine', 'statement_latency', 'current_memory',
                      'last_wait_latency', 'trx_state', 'trx_autocommit');"

expect_output \
    "sys.x processlist SHOW COLUMNS" \
    "$(printf '%b' 'statement_latency\tbigint unsigned\tYES\t\tNULL\t\nlock_latency\tbigint unsigned\tYES\t\tNULL\t\ncpu_latency\tbigint unsigned\tYES\t\tNULL\t\nlast_statement_latency\tbigint unsigned\tYES\t\tNULL\t\ncurrent_memory\tdecimal(41,0)\tYES\t\tNULL\t\nlast_wait_latency\tvarchar(20)\tYES\t\tNULL\t\ntrx_latency\tbigint unsigned\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$processlist\`
      WHERE Field IN ('statement_latency', 'lock_latency', 'cpu_latency',
                      'last_statement_latency', 'current_memory',
                      'last_wait_latency', 'trx_latency');"

expect_output \
    "sys processlist INFORMATION_SCHEMA.COLUMNS differences" \
    "$(printf '%b' 'processlist\tcurrent_statement\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\t4294967295\t4294967295\nprocesslist\tstatement_latency\t10\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\nprocesslist\tfull_scan\t19\tNO\tvarchar(3)\tutf8mb4\tutf8mb4_0900_ai_ci\t3\t12\nprocesslist\tcurrent_memory\t22\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\nprocesslist\tlast_wait_latency\t24\tYES\tvarchar(13)\tutf8mb4\tutf8mb4_0900_ai_ci\t13\t52\nx$processlist\tcurrent_statement\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\t4294967295\t4294967295\nx$processlist\tstatement_latency\t10\tYES\tbigint unsigned\tNULL\tNULL\tNULL\tNULL\nx$processlist\tfull_scan\t19\tNO\tvarchar(3)\tutf8mb4\tutf8mb4_0900_ai_ci\t3\t12\nx$processlist\tcurrent_memory\t22\tYES\tdecimal(41,0)\tNULL\tNULL\tNULL\tNULL\nx$processlist\tlast_wait_latency\t24\tYES\tvarchar(20)\tutf8mb4\tutf8mb4_0900_ai_ci\t20\t80')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('processlist', 'x\$processlist')
        AND COLUMN_NAME IN ('current_statement', 'statement_latency', 'full_scan',
                            'current_memory', 'last_wait_latency')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys processlist INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tprocesslist\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$processlist\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('processlist', 'x\$processlist')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "processlist"
expect_show_table_status_row "x\$processlist"

expect_output \
    "sys processlist INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tprocesslist\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$processlist\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('processlist', 'x\$processlist')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys processlist dependency metadata" \
    "$(printf '%b' 'sys\tprocesslist\tperformance_schema\tevents_stages_current\nsys\tprocesslist\tperformance_schema\tevents_statements_current\nsys\tprocesslist\tperformance_schema\tevents_transactions_current\nsys\tprocesslist\tperformance_schema\tevents_waits_current\nsys\tprocesslist\tperformance_schema\tsession_connect_attrs\nsys\tprocesslist\tperformance_schema\tthreads\nsys\tprocesslist\tsys\tsys_config\nsys\tprocesslist\tsys\tx$memory_by_thread_by_current_bytes\nsys\tx$processlist\tperformance_schema\tevents_stages_current\nsys\tx$processlist\tperformance_schema\tevents_statements_current\nsys\tx$processlist\tperformance_schema\tevents_transactions_current\nsys\tx$processlist\tperformance_schema\tevents_waits_current\nsys\tx$processlist\tperformance_schema\tsession_connect_attrs\nsys\tx$processlist\tperformance_schema\tthreads\nsys\tx$processlist\tsys\tx$memory_by_thread_by_current_bytes')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('processlist', 'x\$processlist')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys processlist routine dependency metadata" \
    "$(printf '%b' 'sys\tprocesslist\tsys\tformat_statement')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('processlist', 'x\$processlist')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys processlist empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('processlist', 'x\$processlist')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('processlist', 'x\$processlist')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('processlist', 'x\$processlist')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('processlist', 'x\$processlist'));"

expect_contains \
    "sys.processlist SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`processlist\`" \
    "SHOW CREATE VIEW sys.processlist;"

expect_contains \
    "sys.processlist SHOW CREATE formatted statement" \
    "\`sys\`.\`format_statement\`(\`pps\`.\`PROCESSLIST_INFO\`) AS \`current_statement\`" \
    "SHOW CREATE VIEW sys.processlist;"

expect_contains \
    "sys.x processlist SHOW CREATE raw statement" \
    "\`pps\`.\`PROCESSLIST_INFO\` AS \`current_statement\`" \
    "SHOW CREATE VIEW sys.\`x\$processlist\`;"

expect_contains \
    "sys.x processlist SHOW CREATE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$processlist\`" \
    "USE sys; SHOW CREATE TABLE \`x\$processlist\`;"

expect_output \
    "sys processlist has runtime rows" \
    "$(printf '%b' '1\n1')" \
    "SELECT COUNT(*) > 0 FROM sys.processlist;
     SELECT COUNT(*) > 0 FROM sys.\`x\$processlist\`;"

current_row=$(run_mysql "SELECT conn_id = CONNECTION_ID(), user LIKE '%@%',
                                command, state, current_statement IS NOT NULL,
                                (trx_autocommit IS NULL OR trx_autocommit IN ('YES', 'NO'))
                           FROM sys.processlist
                          WHERE conn_id = CONNECTION_ID();")
if [ "$current_row" != "1	1	Query	executing	1	1" ]; then
    fail "sys.processlist current row: expected [1	1	Query	executing	1	1], got [$current_row]"
fi

current_raw_row=$(run_mysql "SELECT conn_id = CONNECTION_ID(), user LIKE '%@%',
                                    command, state, current_statement IS NOT NULL,
                                    (trx_autocommit IS NULL OR trx_autocommit IN ('YES', 'NO'))
                               FROM sys.\`x\$processlist\`
                              WHERE conn_id = CONNECTION_ID();")
if [ "$current_raw_row" != "1	1	Query	executing	1	1" ]; then
    fail "sys.x processlist current row: expected [1	1	Query	executing	1	1], got [$current_raw_row]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.processlist; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.processlist SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_processlist_views_expectations: ok"
