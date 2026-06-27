#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_setup_threads_expectations: $1" >&2
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
    "Performance Schema setup_threads columns" \
    "$(printf '%b' 'setup_threads\tNAME\t1\tNULL\tNO\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_threads\tENABLED\t2\tNULL\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_threads\tHISTORY\t3\tNULL\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_threads\tPROPERTIES\t4\tNULL\tNO\tset\t14\t56\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tset(\047singleton\047,\047user\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_threads\tVOLATILITY\t5\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'setup_threads\tDOCUMENTATION\t6\tNULL\tYES\tlongtext\t4294967295\t4294967295\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tlongtext\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_threads statistics" \
    "PRIMARY	0	1	NAME	1	1	HASH	YES" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads';"

expect_output \
    "Performance Schema setup_threads constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads';"

expect_output \
    "Performance Schema setup_threads key usage" \
    "PRIMARY	NAME	1" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads';"

expect_output \
    "Performance Schema setup_threads constraint extensions" \
    "performance_schema	setup_threads	PRIMARY	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads';"

setup_threads_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_threads'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_threads_status" != "setup_threads	PERFORMANCE_SCHEMA	Dynamic	100	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_threads SHOW TABLE STATUS: got [$setup_threads_status]"
fi

expect_output \
    "Performance Schema setup_threads table metadata" \
    "setup_threads	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	100	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_threads';"

expect_output \
    "Performance Schema setup_threads rows" \
    "$(printf '%b' 'thread/innodb/buf_dump_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/buf_pool_create_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/buf_resize_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/bulk_alloc_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/bulk_flusher_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/clone_ddl_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/clone_gtid_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/ddl_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/dict_stats_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/fts_optimize_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/fts_parallel_merge_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/fts_parallel_tokenization_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/io_ibuf_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/io_read_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/io_write_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/log_archiver_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_checkpointer_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_files_governor_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_flush_notifier_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_flusher_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_write_notifier_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/log_writer_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/meb::redo_log_archive_consumer_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/page_archiver_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/page_flush_coordinator_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/page_flush_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/parallel_read_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/parallel_rseg_init_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/recv_writer_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_error_monitor_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_lock_timeout_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_master_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_monitor_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_purge_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_ts_alter_encrypt_thread\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/innodb/srv_worker_thread\tYES\tYES\t\t0\t1\n' \
        'thread/innodb/trx_recovery_rollback_thread\tYES\tYES\t\t0\t1\n' \
        'thread/myisam/find_all_keys\tYES\tYES\t\t0\t1\n' \
        'thread/mysqlx/acceptor_network\tYES\tYES\t\t0\t1\n' \
        'thread/mysqlx/worker\tYES\tYES\tuser\t0\t1\n' \
        'thread/mysys/thread_timer_notifier\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/performance_schema/setup\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/admin_interface\tYES\tYES\tuser\t0\t1\n' \
        'thread/sql/bootstrap\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/compress_gtid_table\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/event_scheduler\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/event_worker\tYES\tYES\t\t0\t1\n' \
        'thread/sql/main\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/manager\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/one_connection\tYES\tYES\tuser\t0\t1\n' \
        'thread/sql/parser_service\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/replica_io\tYES\tYES\t\t0\t1\n' \
        'thread/sql/replica_monitor\tYES\tYES\tsingleton\t0\t1\n' \
        'thread/sql/replica_sql\tYES\tYES\t\t0\t1\n' \
        'thread/sql/replica_worker\tYES\tYES\t\t0\t1\n' \
        'thread/sql/signal_handler\tYES\tYES\tsingleton\t0\t1')" \
    "SELECT NAME, ENABLED, HISTORY, PROPERTIES, VOLATILITY, DOCUMENTATION IS NULL
       FROM performance_schema.setup_threads
      ORDER BY NAME;"

printf '%s\n' "mysql_baseline_performance_schema_setup_threads_expectations: ok"
