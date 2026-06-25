#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_replication_functions_expectations: $1" >&2
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

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    sql=$2
    code=$3
    state=$4
    message=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, got success [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

async_expected=$(
    printf '%b' \
        'The UDF asynchronous_connection_failover_reset() executed successfully.\nThe UDF asynchronous_connection_failover_add_source() executed successfully.\nThe UDF asynchronous_connection_failover_delete_source() executed successfully.\nThe UDF asynchronous_connection_failover_add_managed() executed successfully.\nThe UDF asynchronous_connection_failover_delete_managed() executed successfully.\nThe UDF asynchronous_connection_failover_reset() executed successfully.'
)
expect_output \
    "asynchronous failover success placeholders" \
    "$async_expected" \
    "SELECT asynchronous_connection_failover_reset();
     SELECT asynchronous_connection_failover_add_source(
        'mylite_rf_channel', '127.0.0.1', 3310, '', 80);
     SELECT asynchronous_connection_failover_delete_source(
        'mylite_rf_channel', '127.0.0.1', 3310, '');
     SELECT asynchronous_connection_failover_add_managed(
        'mylite_rf_channel',
        'GroupReplication',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa',
        '127.0.0.1',
        3310,
        '',
        80,
        60);
     SELECT asynchronous_connection_failover_delete_managed(
        'mylite_rf_channel',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa');
     SELECT asynchronous_connection_failover_reset();"

position_expected=$(
    printf '%b' 'NULL\tNULL'
)
expect_output \
    "position waits without replica state" \
    "$position_expected" \
    "SELECT MASTER_POS_WAIT('binlog.000001', 4, 0),
            SOURCE_POS_WAIT('binlog.000001', 4, 0);"

expect_error \
    "gtid wait with fixed gtid_mode off" \
    "SELECT WAIT_FOR_EXECUTED_GTID_SET('', 0);" \
    3062 \
    "HY000" \
    "Cannot use WAIT_FOR_EXECUTED_GTID_SET when GTID_MODE = OFF."

expect_error \
    "group replication function without selected database" \
    "SELECT group_replication_get_communication_protocol();" \
    1046 \
    "3D000" \
    "No database selected"

expect_error \
    "group replication target-runtime absence" \
    "CREATE DATABASE IF NOT EXISTS mylite_probe;
     USE mylite_probe;
     SELECT group_replication_get_communication_protocol();" \
    1305 \
    "42000" \
    "FUNCTION mylite_probe.group_replication_get_communication_protocol does not exist"

printf '%s\n' "mysql_baseline_replication_functions_expectations: ok"
