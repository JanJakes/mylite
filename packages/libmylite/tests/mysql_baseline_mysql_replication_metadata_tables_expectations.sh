#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_replication_metadata_tables_expectations: $1" >&2
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

expect_line_count() {
    label=$1
    expected=$2
    sql=$3

    count=$(run_mysql "$sql" | wc -l | tr -d ' ')
    if [ "$count" != "$expected" ]; then
        fail "$label: expected [$expected] lines, got [$count]"
    fi
}

expect_show_table_status_row() {
    table_name=$1
    expected_comment=$2

    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE '$table_name';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "$table_name SHOW TABLE STATUS: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "$table_name\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "$table_name SHOW TABLE STATUS: unexpected prefix [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "$table_name SHOW TABLE STATUS: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(printf '%b' "NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\t$expected_comment")
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "$table_name SHOW TABLE STATUS: unexpected tail [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "replication metadata row counts" \
    "$(printf '%b' 'slave_master_info\t0\nslave_relay_log_info\t0\nslave_worker_info\t0')" \
    "SELECT 'slave_master_info', COUNT(*) FROM mysql.slave_master_info
     UNION ALL
     SELECT 'slave_relay_log_info', COUNT(*) FROM mysql.slave_relay_log_info
     UNION ALL
     SELECT 'slave_worker_info', COUNT(*) FROM mysql.slave_worker_info;"

expect_output \
    "selected mysql replication metadata row counts" \
    "$(printf '%b' 'slave_master_info\t0\nslave_relay_log_info\t0\nslave_worker_info\t0')" \
    "USE mysql;
     SELECT 'slave_master_info', COUNT(*) FROM slave_master_info
     UNION ALL
     SELECT 'slave_relay_log_info', COUNT(*) FROM slave_relay_log_info
     UNION ALL
     SELECT 'slave_worker_info', COUNT(*) FROM slave_worker_info;"

expect_output \
    "replication metadata column counts" \
    "$(printf '%b' 'slave_master_info\t33\nslave_relay_log_info\t15\nslave_worker_info\t13')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_line_count \
    "slave_master_info SHOW FULL COLUMNS" \
    "33" \
    "SHOW FULL COLUMNS FROM mysql.slave_master_info;"

expect_line_count \
    "slave_relay_log_info SHOW FULL COLUMNS" \
    "15" \
    "SHOW FULL COLUMNS FROM mysql.slave_relay_log_info;"

expect_line_count \
    "slave_worker_info SHOW FULL COLUMNS" \
    "13" \
    "SHOW FULL COLUMNS FROM mysql.slave_worker_info;"

selected_columns_expected=$(
    printf '%b' \
        'slave_master_info\tNumber_of_lines\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint unsigned\t\tselect,insert,update,references\tNumber of lines in the file.\n' \
        'slave_master_info\tMaster_log_name\t2\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\ttext\t\tselect,insert,update,references\tThe name of the master binary log currently being read from the master.\n' \
        'slave_master_info\tHost\t4\tNULL\tYES\tvarchar\t255\t255\tNULL\tNULL\tascii\tascii_general_ci\tvarchar(255)\t\tselect,insert,update,references\tThe host name of the source.\n' \
        'slave_master_info\tHeartbeat\t16\tNULL\tNO\tfloat\tNULL\tNULL\t12\tNULL\tNULL\tNULL\tfloat\t\tselect,insert,update,references\t\n' \
        'slave_master_info\tChannel_name\t24\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(64)\tPRI\tselect,insert,update,references\tThe channel on which the replica is connected to a source. Used in Multisource Replication\n' \
        'slave_master_info\tMaster_compression_algorithm\t29\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\tselect,insert,update,references\tCompression algorithm supported for data transfer between source and replica.\n' \
        'slave_master_info\tSource_connection_auto_failover\t32\t0\tNO\ttinyint\tNULL\tNULL\t3\t0\tNULL\tNULL\ttinyint(1)\t\tselect,insert,update,references\tIndicates whether the channel connection failover is enabled.\n' \
        'slave_master_info\tGtid_only\t33\t0\tNO\ttinyint\tNULL\tNULL\t3\t0\tNULL\tNULL\ttinyint(1)\t\tselect,insert,update,references\tIndicates if this channel only uses GTIDs and does not persist positions.\n' \
        'slave_relay_log_info\tNumber_of_lines\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint unsigned\t\tselect,insert,update,references\tNumber of lines in the file or rows in the table. Used to version table definitions.\n' \
        'slave_relay_log_info\tRelay_log_pos\t3\tNULL\tYES\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\tselect,insert,update,references\tThe relay log position of the last executed event.\n' \
        'slave_relay_log_info\tMaster_log_name\t4\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\ttext\t\tselect,insert,update,references\tThe name of the master binary log file from which the events in the relay log file were read.\n' \
        'slave_relay_log_info\tSql_delay\t6\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint\t\tselect,insert,update,references\tThe number of seconds that the slave must lag behind the master.\n' \
        'slave_relay_log_info\tId\t8\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint unsigned\t\tselect,insert,update,references\tInternal Id that uniquely identifies this record.\n' \
        'slave_relay_log_info\tChannel_name\t9\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(64)\tPRI\tselect,insert,update,references\tThe channel on which the replica is connected to a source. Used in Multisource Replication\n' \
        'slave_relay_log_info\tPrivilege_checks_hostname\t11\tNULL\tYES\tvarchar\t255\t255\tNULL\tNULL\tascii\tascii_general_ci\tvarchar(255)\t\tselect,insert,update,references\tHostname part of PRIVILEGE_CHECKS_USER.\n' \
        'slave_relay_log_info\tRequire_table_primary_key_check\t13\tSTREAM\tNO\tenum\t8\t24\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''STREAM'\'','\''ON'\'','\''OFF'\'','\''GENERATE'\'')\t\tselect,insert,update,references\tIndicates what is the channel policy regarding tables without primary keys on create and alter table queries\n' \
        'slave_relay_log_info\tAssign_gtids_to_anonymous_transactions_type\t14\tOFF\tNO\tenum\t5\t15\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''OFF'\'','\''LOCAL'\'','\''UUID'\'')\t\tselect,insert,update,references\tIndicates whether the channel will generate a new GTID for anonymous transactions. OFF means that anonymous transactions will remain anonymous. LOCAL means that anonymous transactions will be assigned a newly generated GTID based on server_uuid. UUID indicates that anonymous transactions will be assigned a newly generated GTID based on Assign_gtids_to_anonymous_transactions_value\n' \
        'slave_worker_info\tId\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint unsigned\tPRI\tselect,insert,update,references\t\n' \
        'slave_worker_info\tRelay_log_pos\t3\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\tselect,insert,update,references\t\n' \
        'slave_worker_info\tMaster_log_name\t4\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\ttext\t\tselect,insert,update,references\t\n' \
        'slave_worker_info\tCheckpoint_group_bitmap\t12\tNULL\tNO\tblob\t65535\t65535\tNULL\tNULL\tNULL\tNULL\tblob\t\tselect,insert,update,references\t\n' \
        'slave_worker_info\tChannel_name\t13\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(64)\tPRI\tselect,insert,update,references\tThe channel on which the replica is connected to a source. Used in Multisource Replication'
)
expect_output \
    "replication metadata selected columns" \
    "$selected_columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT,
            IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH,
            CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY,
            PRIVILEGES, COLUMN_COMMENT
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
        AND COLUMN_NAME IN ('Number_of_lines', 'Master_log_name', 'Host', 'Heartbeat',
                            'Channel_name', 'Master_compression_algorithm',
                            'Source_connection_auto_failover', 'Gtid_only',
                            'Relay_log_pos', 'Sql_delay', 'Privilege_checks_hostname',
                            'Require_table_primary_key_check',
                            'Assign_gtids_to_anonymous_transactions_type',
                            'Id', 'Checkpoint_group_bitmap')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "replication metadata INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' \
        'slave_master_info\tPRIMARY\t1\tChannel_name\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'slave_relay_log_info\tPRIMARY\t1\tChannel_name\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'slave_worker_info\tPRIMARY\t1\tChannel_name\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'slave_worker_info\tPRIMARY\t2\tId\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT,
            INDEX_COMMENT, IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "replication metadata TABLE_CONSTRAINTS rows" \
    "$(printf '%b' \
        'slave_master_info\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'slave_relay_log_info\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'slave_worker_info\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "replication metadata KEY_COLUMN_USAGE rows" \
    "$(printf '%b' \
        'slave_master_info\tPRIMARY\tChannel_name\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'slave_relay_log_info\tPRIMARY\tChannel_name\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'slave_worker_info\tPRIMARY\tChannel_name\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'slave_worker_info\tPRIMARY\tId\t2\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "replication metadata TABLE_CONSTRAINTS_EXTENSIONS rows" \
    "$(printf '%b' \
        'slave_master_info\tPRIMARY\tNULL\tNULL\n' \
        'slave_relay_log_info\tPRIMARY\tNULL\tNULL\n' \
        'slave_worker_info\tPRIMARY\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "replication metadata INFORMATION_SCHEMA.TABLES rows" \
    "$(printf '%b' \
        'slave_master_info\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tMaster Information\n' \
        'slave_relay_log_info\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tRelay Log Information\n' \
        'slave_worker_info\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tWorker Information')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH,
            DATA_FREE, AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('slave_master_info', 'slave_relay_log_info', 'slave_worker_info')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "slave_master_info" "Master Information"
expect_show_table_status_row "slave_relay_log_info" "Relay Log Information"
expect_show_table_status_row "slave_worker_info" "Worker Information"

printf '%s\n' "mysql_baseline_mysql_replication_metadata_tables_expectations: ok"
