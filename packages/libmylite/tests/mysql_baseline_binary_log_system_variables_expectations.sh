#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_binary_log_system_variables_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

reset_defaults() {
    run_mysql \
        "SET GLOBAL binlog_cache_size = DEFAULT;
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
         SET GLOBAL binlog_transaction_dependency_history_size = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

while IFS='|' read -r variable scalar show session_scope; do
    [ -n "$variable" ] || continue

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;")
    expect_value "$variable scalar/global" "$scalar${TAB}$scalar" "$actual_scalar"

    expected_show="$variable|$show"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"

    if [ "$session_scope" = "yes" ]; then
        actual_session_scalar=$(run_mysql "SELECT @@SESSION.$variable;")
        expect_value "$variable session scalar" "$scalar" "$actual_session_scalar"
    else
        expect_error \
            "$variable session scalar" \
            1238 \
            HY000 \
            "Variable '$variable' is a GLOBAL variable" \
            "SELECT @@SESSION.$variable;"
    fi
done <<EOF
binlog_cache_size|32768|32768|no
binlog_checksum|CRC32|CRC32|no
binlog_direct_non_transactional_updates|0|OFF|yes
binlog_encryption|0|OFF|no
binlog_error_action|ABORT_SERVER|ABORT_SERVER|no
binlog_expire_logs_auto_purge|1|ON|no
binlog_expire_logs_seconds|2592000|2592000|no
binlog_format|ROW|ROW|yes
binlog_group_commit_sync_delay|0|0|no
binlog_group_commit_sync_no_delay_count|0|0|no
binlog_gtid_simple_recovery|1|ON|no
binlog_max_flush_queue_time|0|0|no
binlog_order_commits|1|ON|no
binlog_rotate_encryption_master_key_at_startup|0|OFF|no
binlog_row_event_max_size|8192|8192|no
binlog_row_image|FULL|FULL|yes
binlog_row_metadata|MINIMAL|MINIMAL|no
binlog_row_value_options|||yes
binlog_rows_query_log_events|0|OFF|yes
binlog_stmt_cache_size|32768|32768|no
binlog_transaction_compression|0|OFF|yes
binlog_transaction_compression_level_zstd|3|3|yes
binlog_transaction_dependency_history_size|25000|25000|no
EOF

expect_error \
    "binlog_cache_size set global-only" \
    1229 \
    HY000 \
    "Variable 'binlog_cache_size' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET binlog_cache_size = DEFAULT;"
expect_error \
    "binlog_gtid_simple_recovery read-only SET" \
    1238 \
    HY000 \
    "Variable 'binlog_gtid_simple_recovery' is a read only variable" \
    "SET GLOBAL binlog_gtid_simple_recovery = DEFAULT;"
expect_error \
    "binlog_rotate_encryption_master_key_at_startup read-only SET" \
    1238 \
    HY000 \
    "Variable 'binlog_rotate_encryption_master_key_at_startup' is a read only variable" \
    "SET GLOBAL binlog_rotate_encryption_master_key_at_startup = DEFAULT;"
expect_error \
    "binlog_row_event_max_size read-only SET" \
    1238 \
    HY000 \
    "Variable 'binlog_row_event_max_size' is a read only variable" \
    "SET GLOBAL binlog_row_event_max_size = DEFAULT;"

default_noops=$(
    run_mysql \
        "SET GLOBAL binlog_cache_size = DEFAULT;
         SET GLOBAL binlog_checksum = CRC32;
         SET GLOBAL binlog_encryption = OFF;
         SET GLOBAL binlog_expire_logs_auto_purge = ON;
         SET SESSION binlog_row_image = FULL;
         SET SESSION binlog_row_value_options = '';
         SET SESSION binlog_rows_query_log_events = OFF;
         SET SESSION binlog_transaction_compression = OFF;
         SET SESSION binlog_transaction_compression_level_zstd = 3;
         SELECT @@GLOBAL.binlog_cache_size,
                @@GLOBAL.binlog_checksum,
                @@GLOBAL.binlog_encryption,
                @@GLOBAL.binlog_expire_logs_auto_purge,
                @@SESSION.binlog_row_image,
                @@SESSION.binlog_row_value_options,
                @@SESSION.binlog_rows_query_log_events,
                @@SESSION.binlog_transaction_compression,
                @@SESSION.binlog_transaction_compression_level_zstd,
                @@warning_count;"
)
expect_value \
    "binary-log default-compatible SET values" \
    "32768${TAB}CRC32${TAB}0${TAB}1${TAB}FULL${TAB}${TAB}0${TAB}0${TAB}3${TAB}0" \
    "$default_noops"

mutable_session=$(
    run_mysql \
        "SET SESSION binlog_format = STATEMENT;
         SET SESSION binlog_row_image = MINIMAL;
         SET SESSION binlog_rows_query_log_events = ON;
         SET SESSION binlog_transaction_compression = ON;
         SET SESSION binlog_transaction_compression_level_zstd = 4;
         SELECT @@SESSION.binlog_format,
                @@SESSION.binlog_row_image,
                @@SESSION.binlog_rows_query_log_events,
                @@SESSION.binlog_transaction_compression,
                @@SESSION.binlog_transaction_compression_level_zstd,
                @@warning_count;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
expected_mutable_session="STATEMENT|MINIMAL|1|1|4|1
Warning|1287|'@@binlog_format' is deprecated and will be removed in a future release."
expect_value "MySQL mutable session binary-log values" "$expected_mutable_session" "$mutable_session"

deprecated_reads=$(
    run_mysql \
        "SELECT @@binlog_format, @@warning_count;
         SHOW WARNINGS LIMIT 1;
         SELECT @@GLOBAL.binlog_max_flush_queue_time, @@warning_count;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
expected_deprecated_reads="ROW|1
Warning|1287|'@@binlog_format' is deprecated and will be removed in a future release.
0|1
Warning|1287|'@@binlog_max_flush_queue_time' is deprecated and will be removed in a future release."
expect_value "deprecated binary-log scalar read warnings" "$expected_deprecated_reads" "$deprecated_reads"

deprecated_set=$(
    run_mysql \
        "SET SESSION binlog_format = DEFAULT;
         SHOW WARNINGS LIMIT 1;
         SET GLOBAL binlog_max_flush_queue_time = DEFAULT;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
expected_deprecated_set="Warning|1287|'@@binlog_format' is deprecated and will be removed in a future release.
Warning|1287|'@@binlog_max_flush_queue_time' is deprecated and will be removed in a future release."
expect_value "deprecated binary-log SET warnings" "$expected_deprecated_set" "$deprecated_set"

reset_defaults

printf '%s\n' "mysql_baseline_binary_log_system_variables_expectations: ok"
