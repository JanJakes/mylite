#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_status_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names \
                --default-character-set=utf8mb4 "$@"
        return
    fi

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
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw \
                --default-character-set=utf8mb4 "$@"
        return
    fi

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

expect_positive_numeric_status_row() {
    label=$1
    variable_name=$2
    actual=$3

    case "$actual" in
        "$variable_name"\|*[!0-9]* | "$variable_name"\|) ;;
        "$variable_name"\|0) fail "$label: expected positive value, got [$actual]" ;;
        "$variable_name"\|*) return 0 ;;
        *) fail "$label: row shape mismatch: [$actual]" ;;
    esac

    fail "$label: expected numeric value, got [$actual]"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

headers=$(run_mysql_with_headers "SHOW STATUS LIKE 'Threads_connected';" | sed -n '1p')
expect_value "headers" "Variable_name${TAB}Value" "$headers"

threads_connected=$(run_mysql "SHOW STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "default threads_connected" "Threads_connected" "$threads_connected"

session_threads_connected=$(run_mysql "SHOW SESSION STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row \
    "session threads_connected" \
    "Threads_connected" \
    "$session_threads_connected"

local_threads_connected=$(run_mysql "SHOW LOCAL STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "local threads_connected" "Threads_connected" "$local_threads_connected"

global_threads_connected=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "global threads_connected" "Threads_connected" "$global_threads_connected"

compression=$(run_mysql "SHOW STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "default compression" "Compression|OFF" "$compression"

session_compression=$(run_mysql "SHOW SESSION STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "session compression" "Compression|OFF" "$session_compression"

local_compression=$(run_mysql "SHOW LOCAL STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "local compression" "Compression|OFF" "$local_compression"

global_compression=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "global omits compression" "" "$global_compression"

binlog_row_names=$(run_mysql "SHOW STATUS LIKE 'Binlog\\_%';" | cut -f1)
expected_binlog_row_names="Binlog_cache_disk_use
Binlog_cache_use
Binlog_stmt_cache_disk_use
Binlog_stmt_cache_use"
expect_value "binlog counter row names" "$expected_binlog_row_names" "$binlog_row_names"

session_binlog_row_names=$(run_mysql "SHOW SESSION STATUS LIKE 'Binlog\\_%';" | cut -f1)
expect_value "session binlog counter row names" "$expected_binlog_row_names" "$session_binlog_row_names"

global_binlog_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Binlog\\_%';" | cut -f1)
expect_value "global binlog counter row names" "$expected_binlog_row_names" "$global_binlog_row_names"

connection_row_names=$(run_mysql "SHOW STATUS LIKE 'Connection\\_%';" | cut -f1)
expect_value "connection diagnostic row names" "Connection_control_delay_generated
Connection_control_exempted_unknown_users
Connection_errors_accept
Connection_errors_internal
Connection_errors_max_connections
Connection_errors_peer_address
Connection_errors_select
Connection_errors_tcpwrap" "$connection_row_names"

global_connection_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Connection\\_%';" | cut -f1)
expect_value "global connection diagnostic row names" "Connection_control_delay_generated
Connection_control_exempted_unknown_users
Connection_errors_accept
Connection_errors_internal
Connection_errors_max_connections
Connection_errors_peer_address
Connection_errors_select
Connection_errors_tcpwrap" "$global_connection_row_names"

created_row_names=$(run_mysql "SHOW STATUS LIKE 'Created\\_%';" | cut -f1)
expected_created_row_names="Created_tmp_disk_tables
Created_tmp_files
Created_tmp_tables"
expect_value "created counter row names" "$expected_created_row_names" "$created_row_names"

session_created_row_names=$(run_mysql "SHOW SESSION STATUS LIKE 'Created\\_%';" | cut -f1)
expect_value \
    "session created counter row names" \
    "$expected_created_row_names" \
    "$session_created_row_names"

local_created_row_names=$(run_mysql "SHOW LOCAL STATUS LIKE 'Created\\_%';" | cut -f1)
expect_value \
    "local created counter row names" \
    "$expected_created_row_names" \
    "$local_created_row_names"

global_created_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Created\\_%';" | cut -f1)
expect_value \
    "global created counter row names" \
    "$expected_created_row_names" \
    "$global_created_row_names"

handler_row_names=$(run_mysql "SHOW STATUS LIKE 'Handler\\_%';" | cut -f1)
expected_handler_row_names="Handler_commit
Handler_delete
Handler_discover
Handler_external_lock
Handler_mrr_init
Handler_prepare
Handler_read_first
Handler_read_key
Handler_read_last
Handler_read_next
Handler_read_prev
Handler_read_rnd
Handler_read_rnd_next
Handler_rollback
Handler_savepoint
Handler_savepoint_rollback
Handler_update
Handler_write"
expect_value "handler counter row names" "$expected_handler_row_names" "$handler_row_names"

global_handler_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Handler\\_%';" | cut -f1)
expect_value "global handler counter row names" "$expected_handler_row_names" "$global_handler_row_names"

open_row_names=$(run_mysql "SHOW STATUS LIKE 'Open%';" | cut -f1)
expected_open_row_names="Open_files
Open_streams
Open_table_definitions
Open_tables
Opened_files
Opened_table_definitions
Opened_tables"
expect_value "open resource row names" "$expected_open_row_names" "$open_row_names"

session_open_row_names=$(run_mysql "SHOW SESSION STATUS LIKE 'Open%';" | cut -f1)
expect_value "session open resource row names" "$expected_open_row_names" "$session_open_row_names"

local_open_row_names=$(run_mysql "SHOW LOCAL STATUS LIKE 'Open%';" | cut -f1)
expect_value "local open resource row names" "$expected_open_row_names" "$local_open_row_names"

global_open_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Open%';" | cut -f1)
expect_value "global open resource row names" "$expected_open_row_names" "$global_open_row_names"

thread_row_names=$(run_mysql "SHOW STATUS LIKE 'Threads\\_%';" | cut -f1)
expect_value "threads like row names" "Threads_cached
Threads_connected
Threads_created
Threads_running" "$thread_row_names"

thread_row_names_upper=$(run_mysql "SHOW STATUS LIKE 'THREADS\\_%';" | cut -f1)
expect_value "threads uppercase like row names" "Threads_cached
Threads_connected
Threads_created
Threads_running" "$thread_row_names_upper"

expected_com_row_names=$(cat <<'EOF'
Com_admin_commands
Com_assign_to_keycache
Com_alter_db
Com_alter_event
Com_alter_function
Com_alter_instance
Com_alter_procedure
Com_alter_resource_group
Com_alter_server
Com_alter_table
Com_alter_tablespace
Com_alter_user
Com_alter_user_default_role
Com_analyze
Com_begin
Com_binlog
Com_call_procedure
Com_change_db
Com_change_repl_filter
Com_change_replication_source
Com_check
Com_checksum
Com_clone
Com_commit
Com_create_db
Com_create_event
Com_create_function
Com_create_index
Com_create_procedure
Com_create_role
Com_create_server
Com_create_table
Com_create_resource_group
Com_create_trigger
Com_create_udf
Com_create_user
Com_create_view
Com_create_spatial_reference_system
Com_dealloc_sql
Com_delete
Com_delete_multi
Com_do
Com_drop_db
Com_drop_event
Com_drop_function
Com_drop_index
Com_drop_procedure
Com_drop_resource_group
Com_drop_role
Com_drop_server
Com_drop_spatial_reference_system
Com_drop_table
Com_drop_trigger
Com_drop_user
Com_drop_view
Com_empty_query
Com_execute_sql
Com_explain_other
Com_flush
Com_get_diagnostics
Com_grant
Com_grant_roles
Com_ha_close
Com_ha_open
Com_ha_read
Com_help
Com_import
Com_insert
Com_insert_select
Com_install_component
Com_install_plugin
Com_kill
Com_load
Com_lock_instance
Com_lock_tables
Com_optimize
Com_preload_keys
Com_prepare_sql
Com_purge
Com_purge_before_date
Com_release_savepoint
Com_rename_table
Com_rename_user
Com_repair
Com_replace
Com_replace_select
Com_reset
Com_resignal
Com_restart
Com_revoke
Com_revoke_all
Com_revoke_roles
Com_rollback
Com_rollback_to_savepoint
Com_savepoint
Com_select
Com_set_option
Com_set_password
Com_set_resource_group
Com_set_role
Com_signal
Com_show_binlog_events
Com_show_binlogs
Com_show_charsets
Com_show_collations
Com_show_create_db
Com_show_create_event
Com_show_create_func
Com_show_create_proc
Com_show_create_table
Com_show_create_trigger
Com_show_databases
Com_show_engine_logs
Com_show_engine_mutex
Com_show_engine_status
Com_show_events
Com_show_errors
Com_show_fields
Com_show_function_code
Com_show_function_status
Com_show_grants
Com_show_keys
Com_show_binary_log_status
Com_show_open_tables
Com_show_parse_tree
Com_show_plugins
Com_show_privileges
Com_show_procedure_code
Com_show_procedure_status
Com_show_processlist
Com_show_profile
Com_show_profiles
Com_show_relaylog_events
Com_show_replicas
Com_show_replica_status
Com_show_status
Com_show_storage_engines
Com_show_table_status
Com_show_tables
Com_show_triggers
Com_show_variables
Com_show_warnings
Com_show_create_user
Com_shutdown
Com_replica_start
Com_replica_stop
Com_group_replication_start
Com_group_replication_stop
Com_stmt_execute
Com_stmt_close
Com_stmt_fetch
Com_stmt_prepare
Com_stmt_reset
Com_stmt_send_long_data
Com_truncate
Com_uninstall_component
Com_uninstall_plugin
Com_unlock_instance
Com_unlock_tables
Com_update
Com_update_multi
Com_xa_commit
Com_xa_end
Com_xa_prepare
Com_xa_recover
Com_xa_rollback
Com_xa_start
Com_stmt_reprepare
EOF
)

com_row_names=$(run_mysql "SHOW STATUS LIKE 'Com\\_%';" | cut -f1)
expect_value "command counter row names" "$expected_com_row_names" "$com_row_names"

session_com_row_names=$(run_mysql "SHOW SESSION STATUS LIKE 'Com\\_%';" | cut -f1)
expect_value "session command counter row names" \
    "$expected_com_row_names" \
    "$session_com_row_names"

global_com_row_names=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Com\\_%';" | cut -f1)
expect_value "global command counter row names" "$expected_com_row_names" "$global_com_row_names"

connections=$(run_mysql "SHOW STATUS LIKE 'Connections';" | normalize_tsv)
expect_positive_numeric_status_row "connections" "Connections" "$connections"

row_count_state=$(run_mysql \
    "SHOW STATUS LIKE 'Threads_connected'; SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    | tail -n 1 \
    | normalize_tsv)
expect_value "row count state" "-1|0|0" "$row_count_state"

show_where=$(run_mysql "SHOW STATUS WHERE Variable_name = 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "mysql supports status where" "Threads_connected" "$show_where"

expect_error \
    "like then where is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIKE 'Threads%' WHERE Variable_name = 'Threads_connected';"

expect_error \
    "order by is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS ORDER BY Variable_name;"

expect_error \
    "limit is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIMIT 1;"

expect_error \
    "full is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW FULL STATUS;"

expect_error \
    "non-string like is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIKE 1;"
