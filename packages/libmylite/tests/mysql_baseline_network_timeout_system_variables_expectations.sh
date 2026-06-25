#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_network_timeout_system_variables_expectations: $1" >&2
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
        "SET GLOBAL connect_timeout = DEFAULT;
         SET GLOBAL net_read_timeout = DEFAULT;
         SET GLOBAL net_write_timeout = DEFAULT;
         SET GLOBAL net_retry_count = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

defaults=$(
    run_mysql \
        "SELECT @@connect_timeout, @@GLOBAL.connect_timeout,
                @@net_read_timeout, @@GLOBAL.net_read_timeout, @@SESSION.net_read_timeout,
                @@LOCAL.net_read_timeout,
                @@net_retry_count, @@GLOBAL.net_retry_count, @@SESSION.net_retry_count,
                @@net_write_timeout, @@GLOBAL.net_write_timeout, @@SESSION.net_write_timeout;"
)
expect_value \
    "network timeout defaults" \
    "10${TAB}10${TAB}30${TAB}30${TAB}30${TAB}30${TAB}10${TAB}10${TAB}10${TAB}60${TAB}60${TAB}60" \
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
connect_timeout|10
net_read_timeout|30
net_retry_count|10
net_write_timeout|60
EOF

expect_error \
    "connect_timeout session scalar" \
    1238 \
    HY000 \
    "Variable 'connect_timeout' is a GLOBAL variable" \
    "SELECT @@SESSION.connect_timeout;"
expect_error \
    "connect_timeout local scalar" \
    1238 \
    HY000 \
    "Variable 'connect_timeout' is a GLOBAL variable" \
    "SELECT @@LOCAL.connect_timeout;"
expect_error \
    "connect_timeout set global-only" \
    1229 \
    HY000 \
    "Variable 'connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET connect_timeout = DEFAULT;"

session_sets=$(
    run_mysql \
        "SET SESSION net_read_timeout = 5;
         SET LOCAL net_write_timeout = 6;
         SET @@SESSION.net_retry_count = 7;
         SELECT @@net_read_timeout, @@GLOBAL.net_read_timeout, @@SESSION.net_read_timeout,
                @@net_write_timeout, @@GLOBAL.net_write_timeout, @@SESSION.net_write_timeout,
                @@net_retry_count, @@GLOBAL.net_retry_count, @@SESSION.net_retry_count,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "network timeout mutable session values" \
    "5${TAB}30${TAB}5${TAB}6${TAB}60${TAB}6${TAB}7${TAB}10${TAB}7${TAB}0${TAB}0${TAB}0" \
    "$session_sets"

default_sets=$(
    run_mysql \
        "SET SESSION net_read_timeout = 5;
         SET SESSION net_read_timeout = DEFAULT;
         SET @@net_write_timeout = 6;
         SET @@net_write_timeout = DEFAULT;
         SET @@SESSION.net_retry_count = 7;
         SET @@LOCAL.net_retry_count = DEFAULT;
         SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "network timeout default assignments" \
    "30${TAB}60${TAB}10${TAB}0${TAB}0${TAB}0" \
    "$default_sets"

clamps=$(
    run_mysql \
        "SET SESSION net_read_timeout = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@net_read_timeout, @@warning_count;
         SET SESSION net_read_timeout = 31536001;
         SHOW WARNINGS LIMIT 1;
         SELECT @@net_read_timeout, @@warning_count;
         SET SESSION net_write_timeout = -1;
         SHOW WARNINGS LIMIT 1;
         SELECT @@net_write_timeout, @@warning_count;
         SET SESSION net_retry_count = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@net_retry_count, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "network timeout clamp warnings" \
    "Warning|1292|Truncated incorrect net_read_timeout value: '0'
1|1
Warning|1292|Truncated incorrect net_read_timeout value: '31536001'
31536000|1
Warning|1292|Truncated incorrect net_write_timeout value: '-1'
1|1
Warning|1292|Truncated incorrect net_retry_count value: '0'
1|1" \
    "$clamps"

bool_sets=$(
    run_mysql \
        "SET SESSION net_read_timeout = TRUE;
         SET SESSION net_write_timeout = FALSE;
         SHOW WARNINGS LIMIT 1;
         SET SESSION net_retry_count = TRUE;
         SET SESSION net_retry_count = FALSE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "network timeout boolean assignments" \
    "Warning|1292|Truncated incorrect net_write_timeout value: '0'
Warning|1292|Truncated incorrect net_retry_count value: '0'
1|1|1|1" \
    "$bool_sets"

retry_max=$(
    run_mysql \
        "SET SESSION net_retry_count = 18446744073709551615;
         SELECT @@net_retry_count, @@warning_count;"
)
expect_value \
    "net_retry_count max assignment" \
    "18446744073709551615${TAB}0" \
    "$retry_max"

user_variable_sets=$(
    run_mysql \
        "SET @nr = 8;
         SET SESSION net_read_timeout = @nr;
         SET @nw = -2;
         SET SESSION net_write_timeout = @nw;
         SET @nt = 9;
         SET SESSION net_retry_count = @nt;
         SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "network timeout user variables" \
    "8${TAB}1${TAB}9${TAB}0${TAB}0${TAB}0" \
    "$user_variable_sets"

expect_error \
    "connect_timeout string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'connect_timeout'" \
    "SET GLOBAL connect_timeout = '10';"
expect_error \
    "net_read_timeout string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'net_read_timeout'" \
    "SET SESSION net_read_timeout = '5';"
expect_error \
    "net_read_timeout decimal assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'net_read_timeout'" \
    "SET SESSION net_read_timeout = 1.5;"
expect_error \
    "net_read_timeout ON assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'net_read_timeout'" \
    "SET SESSION net_read_timeout = ON;"
expect_error \
    "net_retry_count overflow assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'net_retry_count'" \
    "SET SESSION net_retry_count = 18446744073709551616;"

global_sets=$(
    run_mysql \
        "SET GLOBAL connect_timeout = 20;
         SET GLOBAL net_read_timeout = 21;
         SET GLOBAL net_write_timeout = 22;
         SET GLOBAL net_retry_count = 23;
         SELECT @@GLOBAL.connect_timeout, @@GLOBAL.net_read_timeout,
                @@GLOBAL.net_write_timeout, @@GLOBAL.net_retry_count,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "network timeout mutable MySQL globals" \
    "20${TAB}21${TAB}22${TAB}23${TAB}0${TAB}0${TAB}0" \
    "$global_sets"

reset_defaults

printf '%s\n' "mysql_baseline_network_timeout_system_variables_expectations: ok"
