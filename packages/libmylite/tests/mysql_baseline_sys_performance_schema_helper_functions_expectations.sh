#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_performance_schema_helper_functions_expectations: $1" >&2
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

expect_error() {
    label=$1
    sql=$2
    expected=$3

    set +e
    output=$(run_mysql "$sql" --show-warnings 2>&1)
    rc=$?
    set -e
    if [ "$rc" -eq 0 ]; then
        fail "$label: expected error, got success"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "sys ps consumer defaults" \
    "$(printf '%b' 'YES\tYES\tNO\tNO\tNULL')" \
    "SELECT
        sys.ps_is_consumer_enabled('thread_instrumentation'),
        sys.ps_is_consumer_enabled('events_statements_history'),
        sys.ps_is_consumer_enabled('events_waits_current'),
        sys.ps_is_consumer_enabled('events_transactions_history_long'),
        sys.ps_is_consumer_enabled(NULL);"

expect_output \
    "sys ps instrument defaults" \
    "$(printf '%b' 'NO\tNO\tYES\tNO\tYES\tYES')" \
    "SELECT
        sys.ps_is_instrument_default_enabled('wait/synch/mutex/pfs/LOCK_pfs_share_list'),
        sys.ps_is_instrument_default_timed('wait/synch/mutex/pfs/LOCK_pfs_share_list'),
        sys.ps_is_instrument_default_enabled('statement/sql/select'),
        sys.ps_is_instrument_default_timed('memory/%'),
        sys.ps_is_instrument_default_enabled(NULL),
        sys.ps_is_instrument_default_timed(NULL);"

expect_output \
    "sys ps account and thread predicates" \
    "$(printf '%b' 'YES\tYES\t0\t0\tYES\tNULL\tUNKNOWN')" \
    "SELECT
        sys.ps_is_account_enabled('127.0.0.1','root'),
        sys.ps_is_account_enabled(NULL,NULL),
        sys.ps_thread_id(CONNECTION_ID()) IS NULL,
        sys.ps_thread_id(NULL) IS NULL,
        sys.ps_is_thread_instrumented(CONNECTION_ID()),
        sys.ps_is_thread_instrumented(NULL),
        sys.ps_is_thread_instrumented(999999);"

expect_output \
    "sys ps thread account" \
    "$(printf '%b' '0\tNULL')" \
    "SELECT
        sys.ps_thread_account(sys.ps_thread_id(CONNECTION_ID())) IS NULL,
        sys.ps_thread_account(999999);"

expect_output \
    "sys ps trace placeholders" \
    "$(printf '%b' '0\t1\tNULL\tNULL')" \
    "SELECT
        sys.ps_thread_stack(NULL, 0) IS NULL,
        JSON_VALID(sys.ps_thread_stack(NULL, 0)),
        sys.ps_thread_trx_info(NULL),
        sys.ps_thread_trx_info(999999);"

expect_output \
    "native ps helper values" \
    "$(printf '%b' '0\t1\tNULL\tNULL\tNULL\t0\t1')" \
    "SELECT
        PS_CURRENT_THREAD_ID() IS NULL,
        PS_THREAD_ID(CONNECTION_ID()) = PS_CURRENT_THREAD_ID(),
        PS_THREAD_ID(NULL),
        PS_THREAD_ID(999999),
        PS_THREAD_ID(-1),
        sys.ps_thread_id(NULL) IS NULL,
        sys.ps_thread_id(CONNECTION_ID()) = PS_CURRENT_THREAD_ID();"

native_ps_warnings_expected=$(
    printf '%b' \
        "NULL\tNULL\nWarning\t1292\tTruncated incorrect INTEGER value: 'abc'\nWarning\t1292\tTruncated incorrect INTEGER value: '1abc'"
)
expect_output \
    "native ps helper warnings" \
    "$native_ps_warnings_expected" \
    "SELECT PS_THREAD_ID('abc'), PS_THREAD_ID('1abc');
     SHOW WARNINGS;"

expect_error \
    "native PS_CURRENT_THREAD_ID arity diagnostic" \
    "SELECT PS_CURRENT_THREAD_ID(1);" \
    "ERROR 1582 (42000)"
expect_error \
    "native PS_THREAD_ID zero-arity diagnostic" \
    "SELECT PS_THREAD_ID();" \
    "ERROR 1582 (42000)"
expect_error \
    "native PS_THREAD_ID two-arity diagnostic" \
    "SELECT PS_THREAD_ID(1,2);" \
    "ERROR 1582 (42000)"
expect_error \
    "native PS_CURRENT_THREAD_ID qualified diagnostic" \
    "SELECT sys.PS_CURRENT_THREAD_ID();" \
    "ERROR 1305 (42000)"
expect_error \
    "native FORMAT_PICO_TIME qualified diagnostic" \
    "SELECT sys.FORMAT_PICO_TIME(1);" \
    "ERROR 1305 (42000)"

expect_error \
    "sys ps invalid consumer diagnostic" \
    "SELECT sys.ps_is_consumer_enabled('not_a_consumer');" \
    "ERROR 3047 (HY000)"

expect_error \
    "sys ps invalid integer diagnostic" \
    "SELECT sys.ps_is_thread_instrumented('abc');" \
    "ERROR 1366 (HY000)"

expect_error \
    "sys ps out-of-range integer diagnostic" \
    "SELECT sys.ps_thread_id(-1);" \
    "ERROR 1264 (22003)"

printf '%s\n' "mysql_baseline_sys_performance_schema_helper_functions_expectations: ok"
