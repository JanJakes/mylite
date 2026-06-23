#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_session_views_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_no_ssl() {
    sql=$1
    shift
    run_mysql "$sql" --ssl-mode=DISABLED "$@"
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

expect_output \
    "sys.session SHOW COLUMNS" \
    "$(printf '%b' 'statement_latency\tvarchar(11)\tYES\t\tNULL\t\ncurrent_memory\tvarchar(11)\tYES\t\tNULL\t\nlast_wait_latency\tvarchar(13)\tYES\t\tNULL\t\ntrx_autocommit\tenum('\''YES'\'','\''NO'\'')\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.session
      WHERE Field IN ('statement_latency', 'current_memory',
                      'last_wait_latency', 'trx_autocommit');"

expect_output \
    "sys.x session SHOW COLUMNS" \
    "$(printf '%b' 'statement_latency\tbigint unsigned\tYES\t\tNULL\t\ncurrent_memory\tdecimal(41,0)\tYES\t\tNULL\t\nlast_wait_latency\tvarchar(20)\tYES\t\tNULL\t\ntrx_autocommit\tenum('\''YES'\'','\''NO'\'')\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$session\`
      WHERE Field IN ('statement_latency', 'current_memory',
                      'last_wait_latency', 'trx_autocommit');"

expect_output \
    "sys.session_ssl_status SHOW COLUMNS" \
    "$(printf '%b' 'thread_id\tbigint unsigned\tNO\t\tNULL\t\nssl_version\tvarchar(1024)\tYES\t\tNULL\t\nssl_cipher\tvarchar(1024)\tYES\t\tNULL\t\nssl_sessions_reused\tvarchar(1024)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.session_ssl_status;"

expect_output \
    "sys session INFORMATION_SCHEMA.COLUMNS differences" \
    "$(printf '%b' 'session\tcurrent_statement\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\t4294967295\t4294967295\tNULL\tNULL\nsession\tstatement_latency\t10\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nsession\tcurrent_memory\t22\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nsession_ssl_status\tthread_id\t1\tNO\tbigint unsigned\tNULL\tNULL\tNULL\tNULL\t20\t0\nsession_ssl_status\tssl_version\t2\tYES\tvarchar(1024)\tutf8mb4\tutf8mb4_0900_ai_ci\t1024\t4096\tNULL\tNULL\nsession_ssl_status\tssl_cipher\t3\tYES\tvarchar(1024)\tutf8mb4\tutf8mb4_0900_ai_ci\t1024\t4096\tNULL\tNULL\nsession_ssl_status\tssl_sessions_reused\t4\tYES\tvarchar(1024)\tutf8mb4\tutf8mb4_0900_ai_ci\t1024\t4096\tNULL\tNULL\nx$session\tcurrent_statement\t8\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\t4294967295\t4294967295\tNULL\tNULL\nx$session\tstatement_latency\t10\tYES\tbigint unsigned\tNULL\tNULL\tNULL\tNULL\t20\t0\nx$session\tcurrent_memory\t22\tYES\tdecimal(41,0)\tNULL\tNULL\tNULL\tNULL\t41\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')
        AND COLUMN_NAME IN ('current_statement', 'statement_latency',
                            'current_memory', 'thread_id', 'ssl_version',
                            'ssl_cipher', 'ssl_sessions_reused')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys session INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tsession\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tsession_ssl_status\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$session\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "session"
expect_show_table_status_row "x\$session"
expect_show_table_status_row "session_ssl_status"

expect_output \
    "sys session INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tsession\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tsession_ssl_status\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$session\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys session dependency metadata" \
    "$(printf '%b' 'sys\tsession\tsys\tprocesslist\nsys\tsession\tsys\tsys_config\nsys\tsession_ssl_status\tperformance_schema\tstatus_by_thread\nsys\tx$session\tsys\tx$processlist')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('session', 'x\$session', 'session_ssl_status')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys session empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys session empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('session', 'x\$session', 'session_ssl_status'));"

expect_contains \
    "sys.session SHOW CREATE VIEW" \
    "CREATE ALGORITHM=UNDEFINED DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`session\`" \
    "SHOW CREATE VIEW sys.session;"

expect_contains \
    "sys.x session SHOW CREATE VIEW" \
    "from \`sys\`.\`x\$processlist\` where ((\`sys\`.\`x\$processlist\`.\`conn_id\` is not null) and (\`sys\`.\`x\$processlist\`.\`command\` <> 'Daemon'))" \
    "SHOW CREATE VIEW sys.\`x\$session\`;"

expect_contains \
    "sys.session_ssl_status SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`session_ssl_status\`" \
    "SHOW CREATE VIEW sys.session_ssl_status;"

expect_contains \
    "sys.session selected schema SHOW CREATE" \
    "CREATE ALGORITHM=UNDEFINED DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`session\`" \
    "USE sys; SHOW CREATE TABLE session;"

expect_contains \
    "sys.session_ssl_status selected schema SHOW CREATE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`session_ssl_status\`" \
    "USE sys; SHOW CREATE TABLE session_ssl_status;"

expect_output \
    "sys session has runtime rows" \
    "$(printf '%b' '1\n1')" \
    "SELECT COUNT(*) > 0 FROM sys.session;
     SELECT COUNT(*) > 0 FROM sys.\`x\$session\`;"

current_row=$(run_mysql "SELECT conn_id = CONNECTION_ID(), user LIKE '%@%',
                                command, state, current_statement IS NOT NULL,
                                (trx_autocommit IS NULL OR trx_autocommit IN ('YES', 'NO'))
                           FROM sys.session
                          WHERE conn_id = CONNECTION_ID();")
if [ "$current_row" != "1	1	Query	executing	1	1" ]; then
    fail "sys.session current row: expected [1	1	Query	executing	1	1], got [$current_row]"
fi

current_raw_row=$(run_mysql "SELECT conn_id = CONNECTION_ID(), user LIKE '%@%',
                                    command, state, current_statement IS NOT NULL,
                                    (trx_autocommit IS NULL OR trx_autocommit IN ('YES', 'NO'))
                               FROM sys.\`x\$session\`
                              WHERE conn_id = CONNECTION_ID();")
if [ "$current_raw_row" != "1	1	Query	executing	1	1" ]; then
    fail "sys.x session current row: expected [1	1	Query	executing	1	1], got [$current_raw_row]"
fi

no_ssl_row=$(run_mysql_no_ssl "SELECT thread_id IS NOT NULL, ssl_version = '',
                                      ssl_cipher = '', ssl_sessions_reused
                                 FROM sys.session_ssl_status
                                WHERE thread_id IS NOT NULL
                                LIMIT 1;")
if [ "$no_ssl_row" != "1	1	1	0" ]; then
    fail "sys.session_ssl_status no-SSL row: expected [1	1	1	0], got [$no_ssl_row]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.session; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.session SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_session_views_expectations: ok"
