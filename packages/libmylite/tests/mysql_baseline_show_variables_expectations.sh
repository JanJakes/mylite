#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --default-character-set=utf8mb4 \
            "$@"
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql \
    "SET GLOBAL host_cache_size = 0;
     SET GLOBAL global_connection_memory_limit = DEFAULT;
     SET GLOBAL global_connection_memory_tracking = DEFAULT;
     SET GLOBAL connection_control_failed_connections_threshold = DEFAULT;
     SET GLOBAL connection_control_max_connection_delay = DEFAULT;
     SET GLOBAL connection_control_min_connection_delay = DEFAULT;
     SET GLOBAL binlog_cache_size = DEFAULT;
     SET GLOBAL binlog_checksum = DEFAULT;
     SET GLOBAL binlog_direct_non_transactional_updates = DEFAULT;
     SET GLOBAL binlog_encryption = DEFAULT;
     SET GLOBAL binlog_error_action = DEFAULT;
     SET GLOBAL binlog_expire_logs_auto_purge = DEFAULT;
     SET GLOBAL binlog_expire_logs_seconds = DEFAULT;
     SET GLOBAL binlog_format = DEFAULT;
     SET GLOBAL binlog_group_commit_sync_delay = DEFAULT;
     SET GLOBAL binlog_group_commit_sync_no_delay_count = DEFAULT;
     SET GLOBAL binlog_max_flush_queue_time = DEFAULT;
     SET GLOBAL binlog_order_commits = DEFAULT;
     SET GLOBAL binlog_row_image = DEFAULT;
     SET GLOBAL binlog_row_metadata = DEFAULT;
     SET GLOBAL binlog_row_value_options = DEFAULT;
     SET GLOBAL binlog_rows_query_log_events = DEFAULT;
     SET GLOBAL binlog_stmt_cache_size = DEFAULT;
     SET GLOBAL binlog_transaction_compression = DEFAULT;
     SET GLOBAL binlog_transaction_compression_level_zstd = DEFAULT;
     SET GLOBAL binlog_transaction_dependency_history_size = DEFAULT;
     SET GLOBAL activate_all_roles_on_login = DEFAULT;
     SET GLOBAL automatic_sp_privileges = DEFAULT;
     SET GLOBAL block_encryption_mode = DEFAULT;
     SET SESSION block_encryption_mode = DEFAULT;
     SET GLOBAL bulk_insert_buffer_size = DEFAULT;
     SET SESSION bulk_insert_buffer_size = DEFAULT;
     SET GLOBAL check_proxy_users = DEFAULT;
     SET GLOBAL completion_type = DEFAULT;
     SET SESSION completion_type = DEFAULT;
     SET GLOBAL concurrent_insert = DEFAULT;
     SET GLOBAL cte_max_recursion_depth = DEFAULT;
     SET SESSION cte_max_recursion_depth = DEFAULT;
     SET GLOBAL default_table_encryption = DEFAULT;
     SET SESSION default_table_encryption = DEFAULT;
     SET GLOBAL default_week_format = DEFAULT;
     SET SESSION default_week_format = DEFAULT;
     SET GLOBAL delay_key_write = DEFAULT;
     SET GLOBAL delayed_insert_limit = DEFAULT;
     SET GLOBAL delayed_insert_timeout = DEFAULT;
     SET GLOBAL delayed_queue_size = DEFAULT;
     SET GLOBAL div_precision_increment = DEFAULT;
     SET SESSION div_precision_increment = DEFAULT;
     SET GLOBAL enforce_gtid_consistency = DEFAULT;
     SET GLOBAL eq_range_index_dive_limit = DEFAULT;
     SET SESSION eq_range_index_dive_limit = DEFAULT;
     SET GLOBAL event_scheduler = DEFAULT;
     SET GLOBAL explain_format = DEFAULT;
     SET SESSION explain_format = DEFAULT;
     SET GLOBAL explain_json_format_version = DEFAULT;
     SET SESSION explain_json_format_version = DEFAULT;
     SET GLOBAL flush = DEFAULT;
     SET GLOBAL flush_time = DEFAULT;
     SET GLOBAL ft_boolean_syntax = DEFAULT;" >/dev/null

headers=$(run_mysql_with_headers "SHOW VARIABLES LIKE 'autocommit';" | sed -n '1p')
expect_value "headers" "Variable_name${TAB}Value" "$headers"

autocommit=$(run_mysql "SHOW VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "default autocommit" "autocommit|ON" "$autocommit"

session_autocommit=$(run_mysql "SHOW SESSION VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "session autocommit" "autocommit|ON" "$session_autocommit"

local_autocommit=$(run_mysql "SHOW LOCAL VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "local autocommit" "autocommit|ON" "$local_autocommit"

global_autocommit=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "global autocommit" "autocommit|ON" "$global_autocommit"

supported_session_rows=$(run_mysql "
    SHOW VARIABLES WHERE Variable_name IN (
      'warning_count',
      'error_count',
      'character_set_client',
      'character_set_connection',
      'character_set_results',
      'collation_connection',
      'version',
      'version_comment',
      'character_set_server',
      'collation_server',
      'completion_type',
      'concurrent_insert',
      'connect_timeout',
      'connection_control_failed_connections_threshold',
      'connection_control_max_connection_delay',
      'connection_control_min_connection_delay',
      'connection_memory_chunk_size',
      'connection_memory_limit',
      'core_file',
      'cte_max_recursion_depth',
      'character_set_database',
      'collation_database',
      'default_storage_engine',
      'default_table_encryption',
      'character_set_system',
      'character_sets_dir',
      'check_proxy_users',
      'character_set_filesystem',
      'default_week_format',
      'delay_key_write',
      'delayed_insert_limit',
      'delayed_insert_timeout',
      'delayed_queue_size',
      'disabled_storage_engines',
      'div_precision_increment',
      'enforce_gtid_consistency',
      'eq_range_index_dive_limit',
      'event_scheduler',
      'explain_format',
      'explain_json_format_version',
      'external_user',
      'flush',
      'flush_time',
      'ft_boolean_syntax',
      'ft_max_word_len',
      'ft_min_word_len',
      'ft_query_expansion_limit',
      'ft_stopword_file',
      'autocommit',
      'activate_all_roles_on_login',
      'auto_generate_certs',
      'automatic_sp_privileges',
      'back_log',
      'bind_address',
      'binlog_cache_size',
      'binlog_checksum',
      'binlog_direct_non_transactional_updates',
      'binlog_encryption',
      'binlog_error_action',
      'binlog_expire_logs_auto_purge',
      'binlog_expire_logs_seconds',
      'binlog_format',
      'binlog_group_commit_sync_delay',
      'binlog_group_commit_sync_no_delay_count',
      'binlog_gtid_simple_recovery',
      'binlog_max_flush_queue_time',
      'binlog_order_commits',
      'binlog_rotate_encryption_master_key_at_startup',
      'binlog_row_event_max_size',
      'binlog_row_image',
      'binlog_row_metadata',
      'binlog_row_value_options',
      'binlog_rows_query_log_events',
      'binlog_stmt_cache_size',
      'binlog_transaction_compression',
      'binlog_transaction_compression_level_zstd',
      'binlog_transaction_dependency_history_size',
      'block_encryption_mode',
      'build_id',
      'bulk_insert_buffer_size',
      'sql_quote_show_create',
      'foreign_key_checks',
      'global_connection_memory_limit',
      'global_connection_memory_tracking',
      'group_concat_max_len',
      'host_cache_size',
      'innodb_read_only',
      'interactive_timeout',
      'lower_case_file_system',
      'lower_case_table_names',
      'max_allowed_packet',
      'net_read_timeout',
      'net_retry_count',
      'net_write_timeout',
      'read_only',
      'transaction_isolation',
      'transaction_read_only',
      'unique_checks',
      'updatable_views_with_limit',
      'sql_auto_is_null',
      'sql_big_selects',
      'sql_generate_invisible_primary_key',
      'sql_safe_updates',
      'sql_warnings',
      'sql_select_limit',
      'sql_notes',
      'sql_buffer_result',
      'sql_log_bin',
      'sql_log_off',
      'sql_mode',
      'sql_require_primary_key',
      'sql_replica_skip_counter',
      'sql_slave_skip_counter',
      'super_read_only',
      'wait_timeout'
    );" | normalize_tsv)
expect_value "supported session rows" "activate_all_roles_on_login|OFF
auto_generate_certs|ON
autocommit|ON
automatic_sp_privileges|ON
back_log|151
bind_address|*
binlog_cache_size|32768
binlog_checksum|CRC32
binlog_direct_non_transactional_updates|OFF
binlog_encryption|OFF
binlog_error_action|ABORT_SERVER
binlog_expire_logs_auto_purge|ON
binlog_expire_logs_seconds|2592000
binlog_format|ROW
binlog_group_commit_sync_delay|0
binlog_group_commit_sync_no_delay_count|0
binlog_gtid_simple_recovery|ON
binlog_max_flush_queue_time|0
binlog_order_commits|ON
binlog_rotate_encryption_master_key_at_startup|OFF
binlog_row_event_max_size|8192
binlog_row_image|FULL
binlog_row_metadata|MINIMAL
binlog_row_value_options|
binlog_rows_query_log_events|OFF
binlog_stmt_cache_size|32768
binlog_transaction_compression|OFF
binlog_transaction_compression_level_zstd|3
binlog_transaction_dependency_history_size|25000
block_encryption_mode|aes-128-ecb
build_id|66e221b3840955d27f740799b5b2c6eb0baf3283
bulk_insert_buffer_size|8388608
character_set_client|utf8mb4
character_set_connection|utf8mb4
character_set_database|utf8mb4
character_set_filesystem|binary
character_set_results|utf8mb4
character_set_server|utf8mb4
character_set_system|utf8mb3
character_sets_dir|/usr/share/mysql-8.4/charsets/
check_proxy_users|OFF
collation_connection|utf8mb4_0900_ai_ci
collation_database|utf8mb4_0900_ai_ci
collation_server|utf8mb4_0900_ai_ci
completion_type|NO_CHAIN
concurrent_insert|AUTO
connect_timeout|10
connection_control_failed_connections_threshold|3
connection_control_max_connection_delay|2147483647
connection_control_min_connection_delay|1000
connection_memory_chunk_size|8192
connection_memory_limit|18446744073709551615
core_file|OFF
cte_max_recursion_depth|1000
default_storage_engine|InnoDB
default_table_encryption|OFF
default_week_format|0
delay_key_write|ON
delayed_insert_limit|100
delayed_insert_timeout|300
delayed_queue_size|1000
disabled_storage_engines|
div_precision_increment|4
enforce_gtid_consistency|OFF
eq_range_index_dive_limit|200
error_count|0
event_scheduler|ON
explain_format|TRADITIONAL
explain_json_format_version|1
external_user|
flush|OFF
flush_time|0
foreign_key_checks|ON
ft_boolean_syntax|+ -><()~*:\"\"&|
ft_max_word_len|84
ft_min_word_len|4
ft_query_expansion_limit|20
ft_stopword_file|(built-in)
global_connection_memory_limit|18446744073709551615
global_connection_memory_tracking|OFF
group_concat_max_len|1024
host_cache_size|0
innodb_read_only|OFF
interactive_timeout|28800
lower_case_file_system|OFF
lower_case_table_names|0
max_allowed_packet|67108864
net_read_timeout|30
net_retry_count|10
net_write_timeout|60
read_only|OFF
sql_auto_is_null|OFF
sql_big_selects|ON
sql_buffer_result|OFF
sql_generate_invisible_primary_key|OFF
sql_log_bin|ON
sql_log_off|OFF
sql_mode|ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
sql_notes|ON
sql_quote_show_create|ON
sql_replica_skip_counter|0
sql_require_primary_key|OFF
sql_safe_updates|OFF
sql_select_limit|18446744073709551615
sql_slave_skip_counter|0
sql_warnings|OFF
super_read_only|OFF
transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF
unique_checks|ON
updatable_views_with_limit|YES
version|8.4.9
version_comment|MySQL Community Server - GPL
wait_timeout|28800
warning_count|0" "$supported_session_rows"

supported_global_rows=$(run_mysql "
    SHOW GLOBAL VARIABLES WHERE Variable_name IN (
      'warning_count',
      'error_count',
      'character_set_client',
      'character_set_connection',
      'character_set_results',
      'collation_connection',
      'version',
      'version_comment',
      'character_set_server',
      'collation_server',
      'completion_type',
      'concurrent_insert',
      'connect_timeout',
      'connection_control_failed_connections_threshold',
      'connection_control_max_connection_delay',
      'connection_control_min_connection_delay',
      'connection_memory_chunk_size',
      'connection_memory_limit',
      'core_file',
      'cte_max_recursion_depth',
      'character_set_database',
      'collation_database',
      'default_storage_engine',
      'default_table_encryption',
      'character_set_system',
      'character_sets_dir',
      'check_proxy_users',
      'character_set_filesystem',
      'default_week_format',
      'delay_key_write',
      'delayed_insert_limit',
      'delayed_insert_timeout',
      'delayed_queue_size',
      'disabled_storage_engines',
      'div_precision_increment',
      'enforce_gtid_consistency',
      'eq_range_index_dive_limit',
      'event_scheduler',
      'explain_format',
      'explain_json_format_version',
      'flush',
      'flush_time',
      'ft_boolean_syntax',
      'ft_max_word_len',
      'ft_min_word_len',
      'ft_query_expansion_limit',
      'ft_stopword_file',
      'autocommit',
      'activate_all_roles_on_login',
      'auto_generate_certs',
      'automatic_sp_privileges',
      'back_log',
      'bind_address',
      'binlog_cache_size',
      'binlog_checksum',
      'binlog_direct_non_transactional_updates',
      'binlog_encryption',
      'binlog_error_action',
      'binlog_expire_logs_auto_purge',
      'binlog_expire_logs_seconds',
      'binlog_format',
      'binlog_group_commit_sync_delay',
      'binlog_group_commit_sync_no_delay_count',
      'binlog_gtid_simple_recovery',
      'binlog_max_flush_queue_time',
      'binlog_order_commits',
      'binlog_rotate_encryption_master_key_at_startup',
      'binlog_row_event_max_size',
      'binlog_row_image',
      'binlog_row_metadata',
      'binlog_row_value_options',
      'binlog_rows_query_log_events',
      'binlog_stmt_cache_size',
      'binlog_transaction_compression',
      'binlog_transaction_compression_level_zstd',
      'binlog_transaction_dependency_history_size',
      'block_encryption_mode',
      'build_id',
      'bulk_insert_buffer_size',
      'sql_quote_show_create',
      'foreign_key_checks',
      'global_connection_memory_limit',
      'global_connection_memory_tracking',
      'group_concat_max_len',
      'host_cache_size',
      'innodb_read_only',
      'interactive_timeout',
      'lower_case_file_system',
      'lower_case_table_names',
      'max_allowed_packet',
      'net_read_timeout',
      'net_retry_count',
      'net_write_timeout',
      'read_only',
      'transaction_isolation',
      'transaction_read_only',
      'unique_checks',
      'updatable_views_with_limit',
      'sql_auto_is_null',
      'sql_big_selects',
      'sql_generate_invisible_primary_key',
      'sql_safe_updates',
      'sql_warnings',
      'sql_select_limit',
      'sql_notes',
      'sql_buffer_result',
      'sql_log_bin',
      'sql_log_off',
      'sql_mode',
      'sql_require_primary_key',
      'sql_replica_skip_counter',
      'sql_slave_skip_counter',
      'super_read_only',
      'wait_timeout'
    );" | normalize_tsv)
expect_value "supported global rows" "activate_all_roles_on_login|OFF
auto_generate_certs|ON
autocommit|ON
automatic_sp_privileges|ON
back_log|151
bind_address|*
binlog_cache_size|32768
binlog_checksum|CRC32
binlog_direct_non_transactional_updates|OFF
binlog_encryption|OFF
binlog_error_action|ABORT_SERVER
binlog_expire_logs_auto_purge|ON
binlog_expire_logs_seconds|2592000
binlog_format|ROW
binlog_group_commit_sync_delay|0
binlog_group_commit_sync_no_delay_count|0
binlog_gtid_simple_recovery|ON
binlog_max_flush_queue_time|0
binlog_order_commits|ON
binlog_rotate_encryption_master_key_at_startup|OFF
binlog_row_event_max_size|8192
binlog_row_image|FULL
binlog_row_metadata|MINIMAL
binlog_row_value_options|
binlog_rows_query_log_events|OFF
binlog_stmt_cache_size|32768
binlog_transaction_compression|OFF
binlog_transaction_compression_level_zstd|3
binlog_transaction_dependency_history_size|25000
block_encryption_mode|aes-128-ecb
build_id|66e221b3840955d27f740799b5b2c6eb0baf3283
bulk_insert_buffer_size|8388608
character_set_client|utf8mb4
character_set_connection|utf8mb4
character_set_database|utf8mb4
character_set_filesystem|binary
character_set_results|utf8mb4
character_set_server|utf8mb4
character_set_system|utf8mb3
character_sets_dir|/usr/share/mysql-8.4/charsets/
check_proxy_users|OFF
collation_connection|utf8mb4_0900_ai_ci
collation_database|utf8mb4_0900_ai_ci
collation_server|utf8mb4_0900_ai_ci
completion_type|NO_CHAIN
concurrent_insert|AUTO
connect_timeout|10
connection_control_failed_connections_threshold|3
connection_control_max_connection_delay|2147483647
connection_control_min_connection_delay|1000
connection_memory_chunk_size|8192
connection_memory_limit|18446744073709551615
core_file|OFF
cte_max_recursion_depth|1000
default_storage_engine|InnoDB
default_table_encryption|OFF
default_week_format|0
delay_key_write|ON
delayed_insert_limit|100
delayed_insert_timeout|300
delayed_queue_size|1000
disabled_storage_engines|
div_precision_increment|4
enforce_gtid_consistency|OFF
eq_range_index_dive_limit|200
event_scheduler|ON
explain_format|TRADITIONAL
explain_json_format_version|1
flush|OFF
flush_time|0
foreign_key_checks|ON
ft_boolean_syntax|+ -><()~*:\"\"&|
ft_max_word_len|84
ft_min_word_len|4
ft_query_expansion_limit|20
ft_stopword_file|(built-in)
global_connection_memory_limit|18446744073709551615
global_connection_memory_tracking|OFF
group_concat_max_len|1024
host_cache_size|0
innodb_read_only|OFF
interactive_timeout|28800
lower_case_file_system|OFF
lower_case_table_names|0
max_allowed_packet|67108864
net_read_timeout|30
net_retry_count|10
net_write_timeout|60
read_only|OFF
sql_auto_is_null|OFF
sql_big_selects|ON
sql_buffer_result|OFF
sql_generate_invisible_primary_key|OFF
sql_log_off|OFF
sql_mode|ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
sql_notes|ON
sql_quote_show_create|ON
sql_replica_skip_counter|0
sql_require_primary_key|OFF
sql_safe_updates|OFF
sql_select_limit|18446744073709551615
sql_slave_skip_counter|0
sql_warnings|OFF
super_read_only|OFF
transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF
unique_checks|ON
updatable_views_with_limit|YES
version|8.4.9
version_comment|MySQL Community Server - GPL
wait_timeout|28800" "$supported_global_rows"

sql_log_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'sql_log_bin';")
expect_value "session-only sql_log_bin omitted globally" "" "$sql_log_global"

warning_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'warning_count';")
expect_value "session-only warning_count omitted globally" "" "$warning_global"

character_set_system_session=$(
    run_mysql "SHOW SESSION VARIABLES LIKE 'character_set_system';" | normalize_tsv
)
expect_value "global variable visible through session show" \
    "character_set_system|utf8mb3" \
    "$character_set_system_session"

log_pattern=$(run_mysql "SHOW VARIABLES LIKE 'sql\\_log\\_%';" | normalize_tsv)
expect_value "escaped underscore like" "sql_log_bin|ON
sql_log_off|OFF" "$log_pattern"

upper_pattern=$(run_mysql "SHOW VARIABLES LIKE 'SQL\\_LOG\\_%';" | normalize_tsv)
expect_value "case-insensitive like" "sql_log_bin|ON
sql_log_off|OFF" "$upper_pattern"

no_match_status=$(
    run_mysql "SHOW VARIABLES LIKE 'missing%'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "no-match status" "0${TAB}0${TAB}-1" "$no_match_status"

match_status=$(
    run_mysql "SHOW VARIABLES LIKE 'autocommit'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "match status" "0${TAB}0${TAB}-1" "$match_status"

deprecation_status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'sql_slave_skip_counter'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "deprecated variable show status" "0${TAB}0${TAB}-1" "$deprecation_status"

where_output=$(run_mysql "SHOW VARIABLES WHERE Variable_name = 'autocommit';" | normalize_tsv)
expect_value "mysql where accepted" "autocommit|ON" "$where_output"

expect_error \
    "numeric like" \
    1064 \
    42000 \
    "near '1'" \
    "SHOW VARIABLES LIKE 1;"

expect_error \
    "null like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "SHOW VARIABLES LIKE NULL;"

expect_error \
    "national like" \
    1064 \
    42000 \
    "near 'N'autocommit''" \
    "SHOW VARIABLES LIKE N'autocommit';"

expect_error \
    "introducer like" \
    1064 \
    42000 \
    "near '_utf8mb4'autocommit''" \
    "SHOW VARIABLES LIKE _utf8mb4'autocommit';"

expect_error \
    "from clause" \
    1064 \
    42000 \
    "near 'FROM mysql'" \
    "SHOW VARIABLES FROM mysql;"

expect_error \
    "full variables" \
    1064 \
    42000 \
    "near 'VARIABLES'" \
    "SHOW FULL VARIABLES;"

expect_error \
    "limit clause" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW VARIABLES LIKE 'sql_%' LIMIT 1;"
