#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_session_tuning_system_variables_expectations: $1" >&2
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
        "SET GLOBAL lock_wait_timeout = DEFAULT;
         SET GLOBAL low_priority_updates = DEFAULT;
         SET GLOBAL slow_launch_time = DEFAULT;
         SET GLOBAL sort_buffer_size = DEFAULT;
         SET lock_wait_timeout = DEFAULT;
         SET low_priority_updates = DEFAULT;
         SET sort_buffer_size = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

defaults=$(
    run_mysql \
        "SELECT @@lock_wait_timeout, @@GLOBAL.lock_wait_timeout, @@SESSION.lock_wait_timeout,
                @@low_priority_updates, @@GLOBAL.low_priority_updates, @@SESSION.low_priority_updates,
                @@slow_launch_time, @@GLOBAL.slow_launch_time,
                @@sort_buffer_size, @@GLOBAL.sort_buffer_size, @@SESSION.sort_buffer_size;"
)
expect_value \
    "session tuning defaults" \
    "31536000${TAB}31536000${TAB}31536000${TAB}0${TAB}0${TAB}0${TAB}2${TAB}2${TAB}262144${TAB}262144${TAB}262144" \
    "$defaults"

while IFS='|' read -r variable show; do
    [ -n "$variable" ] || continue

    expected_show="$variable|$show"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"
done <<EOF
lock_wait_timeout|31536000
low_priority_updates|OFF
slow_launch_time|2
sort_buffer_size|262144
EOF

expect_error \
    "slow_launch_time session scalar" \
    1238 \
    HY000 \
    "Variable 'slow_launch_time' is a GLOBAL variable" \
    "SELECT @@SESSION.slow_launch_time;"
expect_error \
    "slow_launch_time local scalar" \
    1238 \
    HY000 \
    "Variable 'slow_launch_time' is a GLOBAL variable" \
    "SELECT @@LOCAL.slow_launch_time;"
expect_error \
    "slow_launch_time set global-only" \
    1229 \
    HY000 \
    "Variable 'slow_launch_time' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET slow_launch_time = DEFAULT;"

session_sets=$(
    run_mysql \
        "SET lock_wait_timeout = 10;
         SET low_priority_updates = ON;
         SET sort_buffer_size = 65536;
         SELECT @@lock_wait_timeout, @@GLOBAL.lock_wait_timeout,
                @@low_priority_updates, @@GLOBAL.low_priority_updates,
                @@sort_buffer_size, @@GLOBAL.sort_buffer_size,
                @@warning_count, @@error_count;"
)
expect_value \
    "session tuning mutable session values" \
    "10${TAB}31536000${TAB}1${TAB}0${TAB}65536${TAB}262144${TAB}0${TAB}0" \
    "$session_sets"

lock_clamps=$(
    run_mysql \
        "SET lock_wait_timeout = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@lock_wait_timeout, @@warning_count;
         SET lock_wait_timeout = 31536001;
         SHOW WARNINGS LIMIT 1;
         SELECT @@lock_wait_timeout, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "lock_wait_timeout clamp warnings" \
    "Warning|1292|Truncated incorrect lock_wait_timeout value: '0'
1|1
Warning|1292|Truncated incorrect lock_wait_timeout value: '31536001'
31536000|1" \
    "$lock_clamps"

sort_clamps=$(
    run_mysql \
        "SET sort_buffer_size = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@sort_buffer_size, @@warning_count;
         SET sort_buffer_size = 32767;
         SHOW WARNINGS LIMIT 1;
         SELECT @@sort_buffer_size, @@warning_count;
         SET sort_buffer_size = TRUE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@sort_buffer_size, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "sort_buffer_size clamp warnings" \
    "Warning|1292|Truncated incorrect sort_buffer_size value: '0'
32768|1
Warning|1292|Truncated incorrect sort_buffer_size value: '32767'
32768|1
Warning|1292|Truncated incorrect sort_buffer_size value: '1'
32768|1" \
    "$sort_clamps"

user_variable_sets=$(
    run_mysql \
        "SET @lock_value = 7;
         SET lock_wait_timeout = @lock_value;
         SET @low_text = 'OFF';
         SET low_priority_updates = @low_text;
         SET @sort_value = 65536;
         SET sort_buffer_size = @sort_value;
         SELECT @@lock_wait_timeout, @@low_priority_updates, @@sort_buffer_size,
                @@warning_count, @@error_count;"
)
expect_value \
    "session tuning user variables" \
    "7${TAB}0${TAB}65536${TAB}0${TAB}0" \
    "$user_variable_sets"

expect_error \
    "lock_wait_timeout string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'lock_wait_timeout'" \
    "SET lock_wait_timeout = '10';"
expect_error \
    "lock_wait_timeout ON assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'lock_wait_timeout'" \
    "SET lock_wait_timeout = ON;"
expect_error \
    "low_priority_updates invalid assignment" \
    1231 \
    42000 \
    "Variable 'low_priority_updates' can't be set to the value of '2'" \
    "SET low_priority_updates = 2;"
expect_error \
    "sort_buffer_size string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sort_buffer_size'" \
    "SET sort_buffer_size = '65536';"
expect_error \
    "sort_buffer_size ON assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sort_buffer_size'" \
    "SET sort_buffer_size = ON;"

global_mutation=$(
    run_mysql \
        "SET GLOBAL lock_wait_timeout = 20;
         SET GLOBAL low_priority_updates = ON;
         SET GLOBAL slow_launch_time = 3;
         SET GLOBAL sort_buffer_size = 65536;
         SELECT @@GLOBAL.lock_wait_timeout, @@GLOBAL.low_priority_updates,
                @@GLOBAL.slow_launch_time, @@GLOBAL.sort_buffer_size,
                @@warning_count, @@error_count;
         SET GLOBAL lock_wait_timeout = DEFAULT;
         SET GLOBAL low_priority_updates = DEFAULT;
         SET GLOBAL slow_launch_time = DEFAULT;
         SET GLOBAL sort_buffer_size = DEFAULT;"
)
expect_value \
    "mysql mutable global observation" \
    "20${TAB}1${TAB}3${TAB}65536${TAB}0${TAB}0" \
    "$global_mutation"

reset_defaults

printf '%s\n' "mysql_baseline_session_tuning_system_variables_expectations: ok"
