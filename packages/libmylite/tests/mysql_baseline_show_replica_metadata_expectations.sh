#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_replica_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_metadata() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info -vvv "$@" 2>&1 \
        | awk -F'`' '/^Field[[:space:]]+[0-9]+:/ {
            printf "%s%s", separator, $2
            separator = "\t"
        } END {
            print ""
        }'
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

replica_status_columns=$(run_mysql_metadata 'SHOW REPLICA STATUS;')
expect_value \
    "SHOW REPLICA STATUS columns" \
    "$(printf '%b' 'Replica_IO_State\tSource_Host\tSource_User\tSource_Port\tConnect_Retry\tSource_Log_File\tRead_Source_Log_Pos\tRelay_Log_File\tRelay_Log_Pos\tRelay_Source_Log_File\tReplica_IO_Running\tReplica_SQL_Running\tReplicate_Do_DB\tReplicate_Ignore_DB\tReplicate_Do_Table\tReplicate_Ignore_Table\tReplicate_Wild_Do_Table\tReplicate_Wild_Ignore_Table\tLast_Errno\tLast_Error\tSkip_Counter\tExec_Source_Log_Pos\tRelay_Log_Space\tUntil_Condition\tUntil_Log_File\tUntil_Log_Pos\tSource_SSL_Allowed\tSource_SSL_CA_File\tSource_SSL_CA_Path\tSource_SSL_Cert\tSource_SSL_Cipher\tSource_SSL_Key\tSeconds_Behind_Source\tSource_SSL_Verify_Server_Cert\tLast_IO_Errno\tLast_IO_Error\tLast_SQL_Errno\tLast_SQL_Error\tReplicate_Ignore_Server_Ids\tSource_Server_Id\tSource_UUID\tSource_Info_File\tSQL_Delay\tSQL_Remaining_Delay\tReplica_SQL_Running_State\tSource_Retry_Count\tSource_Bind\tLast_IO_Error_Timestamp\tLast_SQL_Error_Timestamp\tSource_SSL_Crl\tSource_SSL_Crlpath\tRetrieved_Gtid_Set\tExecuted_Gtid_Set\tAuto_Position\tReplicate_Rewrite_DB\tChannel_Name\tSource_TLS_Version\tSource_public_key_path\tGet_Source_public_key\tNetwork_Namespace')" \
    "$replica_status_columns"

replica_status_rows=$(run_mysql 'SHOW REPLICA STATUS;' | awk 'END { print NR + 0 }')
expect_value "SHOW REPLICA STATUS row count" "0" "$replica_status_rows"

replicas_columns=$(run_mysql_metadata 'SHOW REPLICAS;')
expect_value \
    "SHOW REPLICAS columns" \
    "$(printf '%b' 'Server_Id\tHost\tPort\tSource_Id\tReplica_UUID')" \
    "$replicas_columns"

replicas_rows=$(run_mysql 'SHOW REPLICAS;' | awk 'END { print NR + 0 }')
expect_value "SHOW REPLICAS row count" "0" "$replicas_rows"

replica_status_diagnostics=$(run_mysql 'SHOW REPLICA STATUS; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW REPLICA STATUS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$replica_status_diagnostics"

replicas_diagnostics=$(run_mysql 'SHOW REPLICAS; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW REPLICAS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$replicas_diagnostics"

relaylog_events_columns=$(run_mysql_metadata 'SHOW RELAYLOG EVENTS;')
expect_value \
    "SHOW RELAYLOG EVENTS columns" \
    "$(printf '%b' 'Log_name\tPos\tEvent_type\tServer_id\tEnd_log_pos\tInfo')" \
    "$relaylog_events_columns"

relaylog_events_rows=$(run_mysql 'SHOW RELAYLOG EVENTS;' | awk 'END { print NR + 0 }')
expect_value "SHOW RELAYLOG EVENTS row count" "0" "$relaylog_events_rows"

relaylog_events_in_rows=$(run_mysql "SHOW RELAYLOG EVENTS IN 'x';" | awk 'END { print NR + 0 }')
expect_value "SHOW RELAYLOG EVENTS IN row count" "0" "$relaylog_events_in_rows"

relaylog_events_from_rows=$(run_mysql 'SHOW RELAYLOG EVENTS FROM 4;' | awk 'END { print NR + 0 }')
expect_value "SHOW RELAYLOG EVENTS FROM row count" "0" "$relaylog_events_from_rows"

relaylog_events_limit_rows=$(run_mysql 'SHOW RELAYLOG EVENTS LIMIT 1;' | awk 'END { print NR + 0 }')
expect_value "SHOW RELAYLOG EVENTS LIMIT row count" "0" "$relaylog_events_limit_rows"

relaylog_events_comma_limit_rows=$(run_mysql 'SHOW RELAYLOG EVENTS LIMIT 1, 2;' | awk 'END { print NR + 0 }')
expect_value \
    "SHOW RELAYLOG EVENTS comma LIMIT row count" \
    "0" \
    "$relaylog_events_comma_limit_rows"

relaylog_events_offset_limit_rows=$(run_mysql 'SHOW RELAYLOG EVENTS LIMIT 1 OFFSET 2;' | awk 'END { print NR + 0 }')
expect_value \
    "SHOW RELAYLOG EVENTS OFFSET LIMIT row count" \
    "0" \
    "$relaylog_events_offset_limit_rows"

relaylog_events_options_rows=$(run_mysql "SHOW RELAYLOG EVENTS IN 'x' FROM 4 LIMIT 1 OFFSET 2;" | awk 'END { print NR + 0 }')
expect_value \
    "SHOW RELAYLOG EVENTS option row count" \
    "0" \
    "$relaylog_events_options_rows"

relaylog_events_empty_channel_rows=$(run_mysql "SHOW RELAYLOG EVENTS FOR CHANNEL '';" | awk 'END { print NR + 0 }')
expect_value \
    "SHOW RELAYLOG EVENTS empty channel row count" \
    "0" \
    "$relaylog_events_empty_channel_rows"

relaylog_events_diagnostics=$(run_mysql 'SHOW RELAYLOG EVENTS; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW RELAYLOG EVENTS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$relaylog_events_diagnostics"

expect_error \
    "SHOW REPLICA STATUS FOR CHANNEL missing channel" \
    3074 \
    HY000 \
    "Replica channel 'default' does not exist" \
    "SHOW REPLICA STATUS FOR CHANNEL 'default';"

expect_error \
    "SHOW REPLICA STATUS LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE '%''" \
    "SHOW REPLICA STATUS LIKE '%';"

expect_error \
    "SHOW REPLICA STATUS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Channel_Name = '''" \
    "SHOW REPLICA STATUS WHERE Channel_Name = '';"

expect_error \
    "SHOW REPLICA STATUS LIMIT syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW REPLICA STATUS LIMIT 1;"

expect_error \
    "SHOW FULL REPLICA STATUS syntax" \
    1064 \
    42000 \
    "near 'REPLICA STATUS'" \
    "SHOW FULL REPLICA STATUS;"

expect_error \
    "SHOW REPLICAS LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE '%''" \
    "SHOW REPLICAS LIKE '%';"

expect_error \
    "SHOW REPLICAS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Host IS NOT NULL'" \
    "SHOW REPLICAS WHERE Host IS NOT NULL;"

expect_error \
    "SHOW REPLICAS LIMIT syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW REPLICAS LIMIT 1;"

expect_error \
    "SHOW FULL REPLICAS syntax" \
    1064 \
    42000 \
    "near 'REPLICAS'" \
    "SHOW FULL REPLICAS;"

expect_error \
    "SHOW RELAYLOG EVENTS named channel missing" \
    3074 \
    HY000 \
    "Replica channel 'default' does not exist" \
    "SHOW RELAYLOG EVENTS FOR CHANNEL 'default';"

expect_error \
    "SHOW RELAYLOG EVENTS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Log_name IS NOT NULL'" \
    "SHOW RELAYLOG EVENTS WHERE Log_name IS NOT NULL;"

expect_error \
    "SHOW FULL RELAYLOG EVENTS syntax" \
    1064 \
    42000 \
    "near 'RELAYLOG EVENTS'" \
    "SHOW FULL RELAYLOG EVENTS;"

expect_error \
    "SHOW RELAYLOG EVENTS string FROM syntax" \
    1064 \
    42000 \
    "near ''4''" \
    "SHOW RELAYLOG EVENTS FROM '4';"

expect_error \
    "SHOW RELAYLOG EVENTS string LIMIT syntax" \
    1064 \
    42000 \
    "near ''1''" \
    "SHOW RELAYLOG EVENTS LIMIT '1';"

expect_error \
    "SHOW SLAVE STATUS removed syntax" \
    1064 \
    42000 \
    "near 'SLAVE STATUS'" \
    "SHOW SLAVE STATUS;"

expect_error \
    "SHOW SLAVE HOSTS removed syntax" \
    1064 \
    42000 \
    "near 'SLAVE HOSTS'" \
    "SHOW SLAVE HOSTS;"

expect_error \
    "SHOW PARSE_TREE production syntax" \
    1064 \
    42000 \
    "near 'PARSE_TREE SELECT 1'" \
    "SHOW PARSE_TREE SELECT 1;"

printf '%s\n' "mysql_baseline_show_replica_metadata_expectations: ok"
