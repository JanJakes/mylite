#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_setup_metrics_table_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "Performance Schema setup_metrics columns" \
    "$(printf '%b' 'setup_metrics\tNAME\t1\tNULL\tNO\tvarchar\t63\t252\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(63)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_metrics\tMETER\t2\tNULL\tNO\tvarchar\t63\t252\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(63)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_metrics\tMETRIC_TYPE\t3\tNULL\tNO\tenum\t20\t80\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047ASYNC COUNTER\047,\047ASYNC UPDOWN COUNTER\047,\047ASYNC GAUGE COUNTER\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_metrics\tNUM_TYPE\t4\tNULL\tNO\tenum\t7\t28\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047INTEGER\047,\047DOUBLE\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_metrics\tUNIT\t5\tNULL\tYES\tvarchar\t63\t252\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(63)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_metrics\tDESCRIPTION\t6\tNULL\tYES\tvarchar\t1023\t4092\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1023)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_metrics statistics" \
    "PRIMARY	0	1	NAME	1	1	HASH	YES" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics';"

expect_output \
    "Performance Schema setup_metrics constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics';"

expect_output \
    "Performance Schema setup_metrics key usage" \
    "PRIMARY	NAME	1" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics';"

expect_output \
    "Performance Schema setup_metrics constraint extensions" \
    "performance_schema	setup_metrics	PRIMARY	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics';"

setup_metrics_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_metrics'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_metrics_status" != "setup_metrics	PERFORMANCE_SCHEMA	Dynamic	422	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_metrics SHOW TABLE STATUS: got [$setup_metrics_status]"
fi

expect_output \
    "Performance Schema setup_metrics table metadata" \
    "setup_metrics	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	422	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_metrics';"

expect_output \
    "Performance Schema setup_metrics count and distinct names" \
    "422	412	10" \
    "SELECT COUNT(*), COUNT(DISTINCT NAME), COUNT(*) - COUNT(DISTINCT NAME)
       FROM performance_schema.setup_metrics;"

expect_output \
    "Performance Schema setup_metrics meter counts" \
    "$(printf '%b' 'mysql.inno\t34\n' \
        'mysql.inno.buffer_pool\t15\n' \
        'mysql.inno.data\t8\n' \
        'mysql.myisam\t7\n' \
        'mysql.perf_schema\t34\n' \
        'mysql.stats\t57\n' \
        'mysql.stats.com\t167\n' \
        'mysql.stats.connection\t7\n' \
        'mysql.stats.handler\t18\n' \
        'mysql.stats.ssl\t13\n' \
        'mysql.x\t46\n' \
        'mysql.x.stmt\t16')" \
    "SELECT METER, COUNT(*)
       FROM performance_schema.setup_metrics
      GROUP BY METER
      ORDER BY METER;"

expect_output \
    "Performance Schema setup_metrics representative rows" \
    "$(printf '%b' 'dblwr_pages_written\tmysql.inno\tASYNC COUNTER\tINTEGER\t\tNumber of pages that have been written for doublewrite operations (innodb_dblwr_pages_written)\n' \
        'os_log_written\tmysql.inno\tASYNC COUNTER\tINTEGER\tBy\tBytes of log written (innodb_os_log_written)\n' \
        'wait_free\tmysql.inno.buffer_pool\tASYNC COUNTER\tINTEGER\t\tNumber of times waited for free buffer (innodb_buffer_pool_wait_free)\n' \
        'fsyncs\tmysql.inno.data\tASYNC COUNTER\tINTEGER\t\tNumber of fsync() calls (innodb_data_fsyncs)\n' \
        'key_writes\tmysql.myisam\tASYNC COUNTER\tINTEGER\t\tThe number of physical writes of a key block from the MyISAM key cache to disk (Key_writes)\n' \
        'users_lost\tmysql.perf_schema\tASYNC COUNTER\tINTEGER\t\tThe number of times a row could not be added to the users table because it was full (Performance_schema_users_lost)\n' \
        'slow_queries\tmysql.stats\tASYNC COUNTER\tINTEGER\t\tThe number of queries that have taken more than long_query_time seconds (Slow_queries)\n' \
        'stmt_reprepare\tmysql.stats.com\tASYNC COUNTER\tINTEGER\t\tNumber of times corresponding command statement has been executed.\n' \
        'errors_tcpwrap\tmysql.stats.connection\tASYNC COUNTER\tINTEGER\t\tThe number of connections refused by the libwrap library (Connection_errors_tcpwrap)\n' \
        'update\tmysql.stats.handler\tASYNC COUNTER\tINTEGER\t\tThe number of requests to update a row in a table (Handler_update)\n' \
        'callback_cache_hits\tmysql.stats.ssl\tASYNC COUNTER\tINTEGER\t\tThe number of accepted SSL connections (Ssl_callback_cache_hits)\n' \
        'ssl_finished_accepts\tmysql.x\tASYNC COUNTER\tINTEGER\t\tThe number of successful SSL connections to the server (Mysqlx_ssl_finished_accepts)\n' \
        'list_clients\tmysql.x.stmt\tASYNC COUNTER\tINTEGER\t\tThe number of list client statements received (Mysqlx_stmt_list_clients)')" \
    "SELECT NAME, METER, METRIC_TYPE, NUM_TYPE, UNIT, DESCRIPTION
       FROM performance_schema.setup_metrics
      WHERE (METER, NAME) IN (
            ('mysql.inno', 'dblwr_pages_written'),
            ('mysql.inno', 'os_log_written'),
            ('mysql.inno.buffer_pool', 'wait_free'),
            ('mysql.inno.data', 'fsyncs'),
            ('mysql.myisam', 'key_writes'),
            ('mysql.perf_schema', 'users_lost'),
            ('mysql.stats', 'slow_queries'),
            ('mysql.stats.com', 'stmt_reprepare'),
            ('mysql.stats.connection', 'errors_tcpwrap'),
            ('mysql.stats.handler', 'update'),
            ('mysql.stats.ssl', 'callback_cache_hits'),
            ('mysql.x', 'ssl_finished_accepts'),
            ('mysql.x.stmt', 'list_clients'))
      ORDER BY METER, NAME;"

expect_output \
    "Performance Schema setup_metrics row digest" \
    "973c99e28c4949fb057d37e42a439ba0e5aae8f134ff96005e39f4cc4b22ce50" \
    "SET SESSION group_concat_max_len = 1048576;
     SELECT SHA2(
                GROUP_CONCAT(
                    CONCAT_WS('\t', NAME, METER, METRIC_TYPE, NUM_TYPE,
                              COALESCE(UNIT, '<NULL>'), COALESCE(DESCRIPTION, '<NULL>'))
                    ORDER BY METER, NAME, DESCRIPTION SEPARATOR '\n'),
                256)
       FROM performance_schema.setup_metrics;"

printf '%s\n' "mysql_baseline_performance_schema_setup_metrics_table_expectations: ok"
