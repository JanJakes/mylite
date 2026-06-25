#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_logging_system_variables_expectations: $1" >&2
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
        "SET GLOBAL general_log = OFF;
         SET GLOBAL general_log_file = DEFAULT;
         SET GLOBAL log_error_services = DEFAULT;
         SET GLOBAL log_error_suppression_list = DEFAULT;
         SET GLOBAL log_error_verbosity = DEFAULT;
         SET GLOBAL log_output = DEFAULT;
         SET GLOBAL log_queries_not_using_indexes = OFF;
         SET GLOBAL log_raw = OFF;
         SET GLOBAL log_slow_admin_statements = OFF;
         SET GLOBAL log_slow_extra = OFF;
         SET GLOBAL log_slow_replica_statements = OFF;
         SET GLOBAL log_slow_slave_statements = OFF;
         SET GLOBAL log_statements_unsafe_for_binlog = DEFAULT;
         SET GLOBAL log_throttle_queries_not_using_indexes = DEFAULT;
         SET GLOBAL log_timestamps = DEFAULT;
         SET GLOBAL long_query_time = DEFAULT;
         SET GLOBAL slow_query_log = OFF;
         SET GLOBAL slow_query_log_file = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

paths=$(run_mysql "SELECT CONCAT(@@datadir, @@hostname, '.log'), CONCAT(@@datadir, @@hostname, '-slow.log');")
general_log_file=$(printf '%s\n' "$paths" | cut -f1)
slow_query_log_file=$(printf '%s\n' "$paths" | cut -f2)

while IFS='|' read -r variable scalar show; do
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

    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "$variable local scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@LOCAL.$variable;"
done <<EOF
general_log|0|OFF
general_log_file|$general_log_file|$general_log_file
log_error|stderr|stderr
log_error_services|log_filter_internal; log_sink_internal|log_filter_internal; log_sink_internal
log_error_suppression_list||
log_error_verbosity|2|2
log_output|FILE|FILE
log_queries_not_using_indexes|0|OFF
log_raw|0|OFF
log_replica_updates|1|ON
log_slave_updates|1|ON
log_slow_admin_statements|0|OFF
log_slow_extra|0|OFF
log_slow_replica_statements|0|OFF
log_slow_slave_statements|0|OFF
log_statements_unsafe_for_binlog|1|ON
log_throttle_queries_not_using_indexes|0|0
log_timestamps|UTC|UTC
slow_query_log|0|OFF
slow_query_log_file|$slow_query_log_file|$slow_query_log_file
EOF

for variable in \
    general_log \
    general_log_file \
    log_error_services \
    log_error_suppression_list \
    log_error_verbosity \
    log_output \
    log_queries_not_using_indexes \
    log_raw \
    log_slow_admin_statements \
    log_slow_extra \
    log_slow_replica_statements \
    log_slow_slave_statements \
    log_statements_unsafe_for_binlog \
    log_throttle_queries_not_using_indexes \
    log_timestamps \
    slow_query_log \
    slow_query_log_file
do
    expect_error \
        "$variable set global-only" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"

    actual_default=$(
        run_mysql \
            "SET GLOBAL $variable = DEFAULT;
             SELECT @@GLOBAL.$variable, @@warning_count, @@error_count, ROW_COUNT();"
    )
    case "$variable" in
        log_slow_slave_statements|log_statements_unsafe_for_binlog)
            expected_warning_count=1
            ;;
        *)
            expected_warning_count=0
            ;;
    esac
    case "$variable" in
        general_log_file) expected_scalar=$general_log_file ;;
        slow_query_log_file) expected_scalar=$slow_query_log_file ;;
        log_error_services) expected_scalar="log_filter_internal; log_sink_internal" ;;
        log_error_suppression_list) expected_scalar="" ;;
        log_error_verbosity) expected_scalar=2 ;;
        log_output) expected_scalar=FILE ;;
        log_statements_unsafe_for_binlog) expected_scalar=1 ;;
        log_timestamps) expected_scalar=UTC ;;
        *) expected_scalar=0 ;;
    esac
    expect_value \
        "$variable global default no-op" \
        "$expected_scalar${TAB}$expected_warning_count${TAB}0${TAB}0" \
        "$actual_default"
done

for variable in log_error log_replica_updates log_slave_updates; do
    expect_error \
        "$variable set global read-only" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
    expect_error \
        "$variable set read-only" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET $variable = DEFAULT;"
done

deprecated_reads=$(
    run_mysql \
        "SELECT @@log_slave_updates, @@warning_count;
         SHOW WARNINGS LIMIT 1;
         SELECT @@log_slow_slave_statements, @@warning_count;
         SHOW WARNINGS LIMIT 1;
         SELECT @@log_statements_unsafe_for_binlog, @@warning_count;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
expected_deprecated_reads="1|1
Warning|1287|'@@log_slave_updates' is deprecated and will be removed in a future release. Please use log_replica_updates instead.
0|1
Warning|1287|'@@log_slow_slave_statements' is deprecated and will be removed in a future release. Please use log_slow_replica_statements instead.
1|1
Warning|1287|'@@log_statements_unsafe_for_binlog' is deprecated and will be removed in a future release."
expect_value "deprecated scalar read warnings" "$expected_deprecated_reads" "$deprecated_reads"

long_query_defaults=$(
    run_mysql \
        "SET GLOBAL long_query_time = DEFAULT;
         SELECT @@long_query_time, @@SESSION.long_query_time, @@GLOBAL.long_query_time;"
)
expect_value \
    "long_query_time defaults" \
    "10.000000${TAB}10.000000${TAB}10.000000" \
    "$long_query_defaults"

long_query_session=$(
    run_mysql \
        "SET long_query_time = 1.2345678;
         SELECT @@long_query_time, @@GLOBAL.long_query_time;
         SET LOCAL long_query_time = 0;
         SELECT @@LOCAL.long_query_time;"
)
expect_value \
    "long_query_time session/local" \
    "1.234568${TAB}10.000000
0.000000" \
    "$long_query_session"

long_query_clamp=$(
    run_mysql \
        "SET long_query_time = -1;
         SELECT @@long_query_time, @@warning_count;
         SET long_query_time = 31536001;
         SELECT @@long_query_time, @@warning_count;"
)
expect_value \
    "long_query_time clamp warnings" \
    "0.000000${TAB}1
31536000.000000${TAB}1" \
    "$long_query_clamp"

long_query_user_vars=$(
    run_mysql \
        "SET @lqt_int = 3;
         SET long_query_time = @lqt_int;
         SELECT @@long_query_time;
         SET @lqt_decimal = 3.5;
         SET long_query_time = @lqt_decimal;
         SELECT @@long_query_time;"
)
expect_value \
    "long_query_time user variables" \
    "3.000000
3.500000" \
    "$long_query_user_vars"

expect_error \
    "long_query_time string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'long_query_time'" \
    "SET long_query_time = '1.5';"
expect_error \
    "long_query_time null assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'long_query_time'" \
    "SET long_query_time = NULL;"
expect_error \
    "long_query_time boolean assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'long_query_time'" \
    "SET long_query_time = ON;"

global_mutation=$(
    run_mysql \
        "SET GLOBAL long_query_time = 2.5;
         SELECT @@GLOBAL.long_query_time, @@warning_count, @@error_count, ROW_COUNT();
         SET GLOBAL long_query_time = DEFAULT;"
)
expect_value \
    "long_query_time mutable global observation" \
    "2.500000${TAB}0${TAB}0${TAB}0" \
    "$global_mutation"

reset_defaults

printf '%s\n' "mysql_baseline_server_logging_system_variables_expectations: ok"
