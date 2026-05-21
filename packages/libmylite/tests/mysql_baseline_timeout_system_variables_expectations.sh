#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_timeout_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" \
                -uroot --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
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
    fi
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

restore_defaults() {
    run_mysql "SET GLOBAL wait_timeout = 28800; SET GLOBAL interactive_timeout = 28800;" \
        >/dev/null 2>&1 || true
}

trap restore_defaults EXIT HUP INT TERM
restore_defaults

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        "SELECT @@wait_timeout, @@GLOBAL.wait_timeout, @@SESSION.wait_timeout, "\
"@@LOCAL.wait_timeout, @@interactive_timeout, @@GLOBAL.interactive_timeout, "\
"@@SESSION.interactive_timeout, @@LOCAL.interactive_timeout;"
)
expect_value \
    "scalar all scopes" \
    "28800${TAB}28800${TAB}28800${TAB}28800${TAB}28800${TAB}28800${TAB}28800${TAB}28800" \
    "$scalar"

case_scalar=$(run_mysql 'SELECT @@WAIT_TIMEOUT, @@global.`interactive_timeout`;')
expect_value "case-insensitive and quoted scalar name" "28800${TAB}28800" "$case_scalar"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ('interactive_timeout','wait_timeout');" \
        | normalize_tsv
)
expect_value "show default" "interactive_timeout|28800
wait_timeout|28800" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ('interactive_timeout','wait_timeout');" \
        | normalize_tsv
)
expect_value "show global" "interactive_timeout|28800
wait_timeout|28800" "$show_global"

show_like=$(
    run_mysql "SHOW VARIABLES LIKE '%\\_timeout';" | normalize_tsv \
        | grep -E '^(interactive_timeout|wait_timeout)\|'
)
expect_value "show like subset" "interactive_timeout|28800
wait_timeout|28800" "$show_like"

session_set=$(
    run_mysql \
        "SET SESSION wait_timeout = 1; SET LOCAL interactive_timeout = 2; "\
"SELECT @@wait_timeout, @@GLOBAL.wait_timeout, @@SESSION.wait_timeout, "\
"@@interactive_timeout, @@GLOBAL.interactive_timeout, @@SESSION.interactive_timeout, "\
"@@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "session and local set" \
    "1${TAB}28800${TAB}1${TAB}2${TAB}28800${TAB}2${TAB}0${TAB}0${TAB}0" \
    "$session_set"

default_set=$(
    run_mysql \
        "SET SESSION wait_timeout = 1; SET SESSION wait_timeout = DEFAULT; "\
"SELECT @@wait_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "default reset" "28800${TAB}0${TAB}0${TAB}0" "$default_set"

direct_set=$(
    run_mysql \
        "SET @@wait_timeout = 3; SET @@SESSION.interactive_timeout = 4; "\
"SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "direct system variable set" "3${TAB}4${TAB}0${TAB}0${TAB}0" "$direct_set"

true_false_status=$(
    run_mysql \
        "SET SESSION wait_timeout = TRUE; SET SESSION interactive_timeout = FALSE; "\
"SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "true false clamp status" \
    "1${TAB}1${TAB}1${TAB}0${TAB}0" \
    "$true_false_status"
true_false_warnings=$(
    run_mysql \
        "SET SESSION wait_timeout = TRUE; SET SESSION interactive_timeout = FALSE; SHOW WARNINGS;"
)
expect_value \
    "true false clamp warnings" \
    "Warning${TAB}1292${TAB}Truncated incorrect interactive_timeout value: '0'" \
    "$true_false_warnings"

signed_set_status=$(
    run_mysql \
        "SET SESSION wait_timeout = +5; SET SESSION interactive_timeout = -1; "\
"SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "signed clamp status" \
    "5${TAB}1${TAB}1${TAB}0${TAB}0" \
    "$signed_set_status"
signed_set_warnings=$(
    run_mysql \
        "SET SESSION wait_timeout = +5; SET SESSION interactive_timeout = -1; SHOW WARNINGS;"
)
expect_value \
    "signed clamp warnings" \
    "Warning${TAB}1292${TAB}Truncated incorrect interactive_timeout value: '-1'" \
    "$signed_set_warnings"

zero_and_max_status=$(
    run_mysql \
        "SET SESSION wait_timeout = 0; SET SESSION interactive_timeout = 999999999999; "\
"SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "zero and high clamp status" \
    "1${TAB}31536000${TAB}1${TAB}0${TAB}0" \
    "$zero_and_max_status"
zero_and_max_warnings=$(
    run_mysql \
        "SET SESSION wait_timeout = 0; SET SESSION interactive_timeout = 999999999999; "\
"SHOW WARNINGS;"
)
expect_value \
    "zero and high clamp warnings" \
    "Warning${TAB}1292${TAB}Truncated incorrect interactive_timeout value: '999999999999'" \
    "$zero_and_max_warnings"
zero_warning=$(
    run_mysql "SET SESSION wait_timeout = 0; SHOW WARNINGS;"
)
expect_value \
    "zero clamp warning" \
    "Warning${TAB}1292${TAB}Truncated incorrect wait_timeout value: '0'" \
    "$zero_warning"

user_variable_integer_status=$(
    run_mysql \
        "SET @wt = 7; SET SESSION wait_timeout = @wt; "\
"SET @it = -2; SET SESSION interactive_timeout = @it; "\
"SELECT @@wait_timeout, @@interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "user variable integer status" \
    "7${TAB}1${TAB}1${TAB}0${TAB}0" \
    "$user_variable_integer_status"
user_variable_integer_warnings=$(
    run_mysql \
        "SET @wt = 7; SET SESSION wait_timeout = @wt; "\
"SET @it = -2; SET SESSION interactive_timeout = @it; SHOW WARNINGS;"
)
expect_value \
    "user variable integer warnings" \
    "Warning${TAB}1292${TAB}Truncated incorrect interactive_timeout value: '-2'" \
    "$user_variable_integer_warnings"

global_default=$(
    run_mysql \
        "SET GLOBAL wait_timeout = 28800; SET @@GLOBAL.interactive_timeout = DEFAULT; "\
"SELECT @@GLOBAL.wait_timeout, @@SESSION.wait_timeout, @@GLOBAL.interactive_timeout, "\
"@@SESSION.interactive_timeout, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "global no-op" \
    "28800${TAB}28800${TAB}28800${TAB}28800${TAB}0${TAB}0${TAB}0" \
    "$global_default"

expect_error \
    "string rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET SESSION wait_timeout = '5';"

expect_error \
    "decimal rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET SESSION wait_timeout = 1.5;"

expect_error \
    "null rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET SESSION wait_timeout = NULL;"

expect_error \
    "on rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET SESSION wait_timeout = ON;"

expect_error \
    "off rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET SESSION wait_timeout = OFF;"

expect_error \
    "user variable string rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET @wt = '7'; SET SESSION wait_timeout = @wt;"

expect_error \
    "user variable null rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'wait_timeout'" \
    "SET @wt = NULL; SET SESSION wait_timeout = @wt;"

restore_defaults
printf '%s\n' "mysql_baseline_timeout_system_variables_expectations: ok"
