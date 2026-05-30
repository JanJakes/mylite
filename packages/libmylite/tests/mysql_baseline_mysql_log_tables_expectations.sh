#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_log_tables_expectations: $1" >&2
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
    table_name=$1
    comment=$2
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE '$table_name';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS $table_name: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "$table_name\tCSV\t10\tDynamic\t2\t0\t0\t0\t0\t0\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS $table_name: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS $table_name: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(printf '%b' "NULL\tNULL\tutf8mb3_general_ci\tNULL\t\t$comment")
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS $table_name: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql.general_log baseline row count" \
    "0" \
    "SELECT COUNT(*) FROM mysql.general_log;"
expect_output \
    "mysql.slow_log baseline row count" \
    "0" \
    "SELECT COUNT(*) FROM mysql.slow_log;"
expect_output \
    "mysql.general_log direct read is empty" \
    "" \
    "SELECT event_time, user_host, thread_id, server_id, command_type, argument
       FROM mysql.general_log
      ORDER BY event_time;"
expect_output \
    "mysql.slow_log direct read is empty" \
    "" \
    "SELECT start_time, user_host, query_time, lock_time, rows_sent, rows_examined,
            db, last_insert_id, insert_id, server_id, sql_text, thread_id
       FROM mysql.slow_log
      ORDER BY start_time;"
expect_output \
    "mysql log table read ROW_COUNT" \
    "-1" \
    "SELECT event_time FROM mysql.general_log;
     SELECT ROW_COUNT();"

general_columns_expected=$(
    printf '%b' \
        'event_time\ttimestamp(6)\tNO\t\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\n' \
        'user_host\tmediumtext\tNO\t\tNULL\t\n' \
        'thread_id\tbigint unsigned\tNO\t\tNULL\t\n' \
        'server_id\tint unsigned\tNO\t\tNULL\t\n' \
        'command_type\tvarchar(64)\tNO\t\tNULL\t\n' \
        'argument\tmediumblob\tNO\t\tNULL\t'
)
expect_output \
    "mysql.general_log SHOW COLUMNS rows" \
    "$general_columns_expected" \
    "SHOW COLUMNS FROM mysql.general_log;"

slow_columns_expected=$(
    printf '%b' \
        'start_time\ttimestamp(6)\tNO\t\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\n' \
        'user_host\tmediumtext\tNO\t\tNULL\t\n' \
        'query_time\ttime(6)\tNO\t\tNULL\t\n' \
        'lock_time\ttime(6)\tNO\t\tNULL\t\n' \
        'rows_sent\tint\tNO\t\tNULL\t\n' \
        'rows_examined\tint\tNO\t\tNULL\t\n' \
        'db\tvarchar(512)\tNO\t\tNULL\t\n' \
        'last_insert_id\tint\tNO\t\tNULL\t\n' \
        'insert_id\tint\tNO\t\tNULL\t\n' \
        'server_id\tint unsigned\tNO\t\tNULL\t\n' \
        'sql_text\tmediumblob\tNO\t\tNULL\t\n' \
        'thread_id\tbigint unsigned\tNO\t\tNULL\t'
)
expect_output \
    "mysql.slow_log SHOW COLUMNS rows" \
    "$slow_columns_expected" \
    "SHOW COLUMNS FROM mysql.slow_log;"

general_full_columns_expected=$(
    printf '%b' \
        'event_time\ttimestamp(6)\tNULL\tNO\t\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\tselect,insert,update,references\t\n' \
        'user_host\tmediumtext\tutf8mb3_general_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'thread_id\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'server_id\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'command_type\tvarchar(64)\tutf8mb3_general_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'argument\tmediumblob\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.general_log SHOW FULL COLUMNS rows" \
    "$general_full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.general_log;"

slow_full_columns_expected=$(
    printf '%b' \
        'start_time\ttimestamp(6)\tNULL\tNO\t\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\tselect,insert,update,references\t\n' \
        'user_host\tmediumtext\tutf8mb3_general_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'query_time\ttime(6)\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'lock_time\ttime(6)\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'rows_sent\tint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'rows_examined\tint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'db\tvarchar(512)\tutf8mb3_general_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'last_insert_id\tint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'insert_id\tint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'server_id\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'sql_text\tmediumblob\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'thread_id\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.slow_log SHOW FULL COLUMNS rows" \
    "$slow_full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.slow_log;"

expect_output \
    "mysql.general_log SHOW INDEX empty" \
    "" \
    "SHOW INDEX FROM mysql.general_log;"
expect_output \
    "mysql.slow_log SHOW INDEX empty" \
    "" \
    "SHOW INDEX FROM mysql.slow_log;"

general_information_schema_columns_expected=$(
    printf '%b' \
        'event_time\t1\tCURRENT_TIMESTAMP(6)\tNO\ttimestamp\tNULL\tNULL\tNULL\tNULL\t6\tNULL\tNULL\ttimestamp(6)\t\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\tselect,insert,update,references\t\t\n' \
        'user_host\t2\tNULL\tNO\tmediumtext\t16777215\t16777215\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tmediumtext\t\t\tselect,insert,update,references\t\t\n' \
        'thread_id\t3\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'server_id\t4\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'command_type\t5\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'argument\t6\tNULL\tNO\tmediumblob\t16777215\t16777215\tNULL\tNULL\tNULL\tNULL\tNULL\tmediumblob\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.general_log INFORMATION_SCHEMA.COLUMNS rows" \
    "$general_information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'general_log'
      ORDER BY ORDINAL_POSITION;"

slow_information_schema_columns_expected=$(
    printf '%b' \
        'start_time\t1\tCURRENT_TIMESTAMP(6)\tNO\ttimestamp\tNULL\tNULL\tNULL\tNULL\t6\tNULL\tNULL\ttimestamp(6)\t\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)\tselect,insert,update,references\t\t\n' \
        'user_host\t2\tNULL\tNO\tmediumtext\t16777215\t16777215\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tmediumtext\t\t\tselect,insert,update,references\t\t\n' \
        'query_time\t3\tNULL\tNO\ttime\tNULL\tNULL\tNULL\tNULL\t6\tNULL\tNULL\ttime(6)\t\t\tselect,insert,update,references\t\t\n' \
        'lock_time\t4\tNULL\tNO\ttime\tNULL\tNULL\tNULL\tNULL\t6\tNULL\tNULL\ttime(6)\t\t\tselect,insert,update,references\t\t\n' \
        'rows_sent\t5\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'rows_examined\t6\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'db\t7\tNULL\tNO\tvarchar\t512\t1536\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(512)\t\t\tselect,insert,update,references\t\t\n' \
        'last_insert_id\t8\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'insert_id\t9\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'server_id\t10\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'sql_text\t11\tNULL\tNO\tmediumblob\t16777215\t16777215\tNULL\tNULL\tNULL\tNULL\tNULL\tmediumblob\t\t\tselect,insert,update,references\t\t\n' \
        'thread_id\t12\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.slow_log INFORMATION_SCHEMA.COLUMNS rows" \
    "$slow_information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slow_log'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql log INFORMATION_SCHEMA.STATISTICS count" \
    "0" \
    "SELECT COUNT(*) FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('general_log','slow_log');"
expect_output \
    "mysql log INFORMATION_SCHEMA.TABLE_CONSTRAINTS count" \
    "0" \
    "SELECT COUNT(*) FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('general_log','slow_log');"
expect_output \
    "mysql log INFORMATION_SCHEMA.KEY_COLUMN_USAGE count" \
    "0" \
    "SELECT COUNT(*) FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('general_log','slow_log');"
expect_output \
    "mysql log INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS count" \
    "0" \
    "SELECT COUNT(*) FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME IN ('general_log','slow_log');"

information_schema_tables_expected=$(
    printf '%b' \
        'general_log\tBASE TABLE\tCSV\t10\tDynamic\t2\t0\t0\t0\t0\t0\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\t\tGeneral log\n' \
        'slow_log\tBASE TABLE\tCSV\t10\tDynamic\t2\t0\t0\t0\t0\t0\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\t\tSlow log'
)
expect_output \
    "mysql log INFORMATION_SCHEMA.TABLES rows" \
    "$information_schema_tables_expected" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('general_log','slow_log')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "general_log" "General log"
expect_show_table_status_row "slow_log" "Slow log"

printf '%s\n' "mysql_baseline_mysql_log_tables_expectations: ok"
