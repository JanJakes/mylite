#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_binary_log_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

status_header=$(run_mysql_with_headers 'SHOW BINARY LOG STATUS;' | sed -n '1p')
expect_value \
    "SHOW BINARY LOG STATUS header" \
    "$(printf '%b' 'File\tPosition\tBinlog_Do_DB\tBinlog_Ignore_DB\tExecuted_Gtid_Set')" \
    "$status_header"

status_row_count=$(run_mysql_with_headers 'SHOW BINARY LOG STATUS;' | awk 'NR > 1 { count++ } END { print count + 0 }')
if [ "$status_row_count" -lt 1 ]; then
    fail "SHOW BINARY LOG STATUS expected at least one live MySQL row"
fi

logs_header=$(run_mysql_with_headers 'SHOW BINARY LOGS;' | sed -n '1p')
expect_value \
    "SHOW BINARY LOGS header" \
    "$(printf '%b' 'Log_name\tFile_size\tEncrypted')" \
    "$logs_header"

logs_row_count=$(run_mysql_with_headers 'SHOW BINARY LOGS;' | awk 'NR > 1 { count++ } END { print count + 0 }')
if [ "$logs_row_count" -lt 1 ]; then
    fail "SHOW BINARY LOGS expected at least one live MySQL row"
fi

events_header=$(run_mysql_with_headers 'SHOW BINLOG EVENTS LIMIT 1;' | sed -n '1p')
expect_value \
    "SHOW BINLOG EVENTS header" \
    "$(printf '%b' 'Log_name\tPos\tEvent_type\tServer_id\tEnd_log_pos\tInfo')" \
    "$events_header"

first_event=$(run_mysql 'SHOW BINLOG EVENTS LIMIT 1;')
expect_value \
    "SHOW BINLOG EVENTS first event" \
    "$(printf '%b' 'binlog.000001\t4\tFormat_desc\t1\t127\tServer ver: 8.4.9, Binlog ver: 4')" \
    "$first_event"

qualified_first_event=$(
    run_mysql "SHOW BINLOG EVENTS IN 'binlog.000001' FROM 4 LIMIT 0, 1;"
)
expect_value \
    "SHOW BINLOG EVENTS qualified first event" \
    "$(printf '%b' 'binlog.000001\t4\tFormat_desc\t1\t127\tServer ver: 8.4.9, Binlog ver: 4')" \
    "$qualified_first_event"

offset_empty_count=$(
    run_mysql_with_headers 'SHOW BINLOG EVENTS LIMIT 0 OFFSET 1;' \
        | awk 'NR > 1 { count++ } END { print count + 0 }'
)
expect_value "SHOW BINLOG EVENTS skipped by offset" "0" "$offset_empty_count"

status_diagnostics=$(run_mysql 'SHOW BINARY LOG STATUS; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW BINARY LOG STATUS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$status_diagnostics"

logs_diagnostics=$(run_mysql 'SHOW BINARY LOGS; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW BINARY LOGS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$logs_diagnostics"

events_diagnostics=$(run_mysql 'SHOW BINLOG EVENTS LIMIT 1; SELECT ROW_COUNT(), @@warning_count, @@error_count;' | tail -n 1)
expect_value \
    "SHOW BINLOG EVENTS diagnostics" \
    "$(printf '%b' '-1\t0\t0')" \
    "$events_diagnostics"

expect_error \
    "SHOW BINLOG EVENTS missing log" \
    1220 \
    HY000 \
    "Could not find target log" \
    "SHOW BINLOG EVENTS IN 'missing' LIMIT 1;"

expect_error \
    "SHOW BINARY LOG STATUS LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE '%''" \
    "SHOW BINARY LOG STATUS LIKE '%';"

expect_error \
    "SHOW BINARY LOG STATUS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE File IS NOT NULL'" \
    "SHOW BINARY LOG STATUS WHERE File IS NOT NULL;"

expect_error \
    "SHOW BINARY LOG STATUS LIMIT syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW BINARY LOG STATUS LIMIT 1;"

expect_error \
    "SHOW BINARY LOGS LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE '%''" \
    "SHOW BINARY LOGS LIKE '%';"

expect_error \
    "SHOW BINARY LOGS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Log_name IS NOT NULL'" \
    "SHOW BINARY LOGS WHERE Log_name IS NOT NULL;"

expect_error \
    "SHOW BINARY LOGS LIMIT syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW BINARY LOGS LIMIT 1;"

expect_error \
    "SHOW BINLOG EVENTS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Log_name IS NOT NULL'" \
    "SHOW BINLOG EVENTS WHERE Log_name IS NOT NULL;"

expect_error \
    "SHOW BINLOG EVENTS channel syntax" \
    1064 \
    42000 \
    "near 'FOR CHANNEL '''" \
    "SHOW BINLOG EVENTS FOR CHANNEL '';"

expect_error \
    "SHOW BINLOG EVENTS string FROM syntax" \
    1064 \
    42000 \
    "near ''4''" \
    "SHOW BINLOG EVENTS FROM '4';"

expect_error \
    "SHOW BINLOG EVENTS string LIMIT syntax" \
    1064 \
    42000 \
    "near ''1''" \
    "SHOW BINLOG EVENTS LIMIT '1';"

expect_error \
    "SHOW BINLOG EVENTS option order syntax" \
    1064 \
    42000 \
    "near 'IN 'binlog.000001' LIMIT 1'" \
    "SHOW BINLOG EVENTS FROM 4 IN 'binlog.000001' LIMIT 1;"

expect_error \
    "SHOW FULL BINARY LOG STATUS syntax" \
    1064 \
    42000 \
    "near 'BINARY LOG STATUS'" \
    "SHOW FULL BINARY LOG STATUS;"

expect_error \
    "SHOW FULL BINARY LOGS syntax" \
    1064 \
    42000 \
    "near 'BINARY LOGS'" \
    "SHOW FULL BINARY LOGS;"

expect_error \
    "SHOW MASTER STATUS removed syntax" \
    1064 \
    42000 \
    "near 'MASTER STATUS'" \
    "SHOW MASTER STATUS;"

printf '%s\n' "mysql_baseline_show_binary_log_metadata_expectations: ok"
