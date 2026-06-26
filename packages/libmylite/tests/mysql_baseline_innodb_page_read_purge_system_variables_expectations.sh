#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_page_read_purge_system_variables_expectations: $1" >&2
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

variables() {
    cat <<'EOF'
innodb_old_blocks_pct|37|37|dynamic_numeric|37|38
innodb_old_blocks_time|1000|1000|dynamic_numeric|1000|1001
innodb_online_alter_log_max_size|134217728|134217728|dynamic_numeric|134217728|134217729
innodb_open_files|4000|4000|read_only||
innodb_optimize_fulltext_only|0|OFF|dynamic_boolean|OFF|ON
innodb_page_cleaners|1|1|read_only||
innodb_page_size|16384|16384|read_only||
innodb_parallel_read_threads|4|4|dynamic_session|4|5
innodb_print_all_deadlocks|0|OFF|dynamic_boolean|OFF|ON
innodb_print_ddl_logs|0|OFF|dynamic_boolean|OFF|ON
innodb_purge_batch_size|300|300|dynamic_numeric|300|301
innodb_purge_rseg_truncate_frequency|128|128|dynamic_numeric|128|127
innodb_purge_threads|4|4|read_only||
innodb_random_read_ahead|0|OFF|dynamic_boolean|OFF|ON
innodb_read_ahead_threshold|56|56|dynamic_numeric|56|57
innodb_read_io_threads|9|9|read_only||
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show mutation _exact _alternate; do
        case "$mutation" in
            dynamic_numeric|dynamic_boolean)
                run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null 2>&1 || true
                ;;
            dynamic_session)
                run_mysql \
                    "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT;" \
                    >/dev/null 2>&1 || true
                ;;
            *) ;;
        esac
    done
}

check_default_readbacks() {
    variable=$1
    scalar_value=$2
    show_value=$3
    mutation=$4
    expected_show="$variable|$show_value"

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar default" "$scalar_value|$scalar_value" "$actual_scalar"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"

    if [ "$mutation" = dynamic_session ]; then
        actual_session_scalar=$(run_mysql "SELECT @@SESSION.$variable, @@LOCAL.$variable;" | normalize_tsv)
        expect_value \
            "$variable session scalar" \
            "$scalar_value|$scalar_value" \
            "$actual_session_scalar"
    else
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
    fi
}

check_read_only_assignments() {
    variable=$1

    for scope in "" "SESSION " "LOCAL " "GLOBAL "; do
        expect_error \
            "$variable read-only SET ${scope:-unscoped}" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET ${scope}$variable = DEFAULT;"
    done
    expect_error \
        "$variable read-only NULL" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = NULL;"
}

check_dynamic_global_assignments() {
    variable=$1
    mutation=$2
    exact=$3
    alternate=$4

    expect_error \
        "$variable global-only unscoped SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
    expect_error \
        "$variable global-only session SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET SESSION $variable = DEFAULT;"
    expect_error \
        "$variable global-only local SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET LOCAL $variable = DEFAULT;"

    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
    run_mysql "SET GLOBAL $variable = $alternate;" >/dev/null
    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null

    if [ "$mutation" = dynamic_boolean ]; then
        expect_error \
            "$variable null SET" \
            1231 \
            42000 \
            "Variable '$variable' can't be set to the value of 'NULL'" \
            "SET GLOBAL $variable = NULL;"
        expect_error \
            "$variable bogus SET" \
            1231 \
            42000 \
            "Variable '$variable' can't be set to the value of 'bogus'" \
            "SET GLOBAL $variable = 'bogus';"
    else
        expect_error \
            "$variable null SET" \
            1232 \
            42000 \
            "Incorrect argument type to variable '$variable'" \
            "SET GLOBAL $variable = NULL;"
        expect_error \
            "$variable bogus SET" \
            1232 \
            42000 \
            "Incorrect argument type to variable '$variable'" \
            "SET GLOBAL $variable = 'bogus';"
    fi
}

check_parallel_read_threads_assignments() {
    variable=innodb_parallel_read_threads

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET $variable = 5; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable; SHOW VARIABLES LIKE '$variable'; SHOW GLOBAL VARIABLES LIKE '$variable';" \
        | normalize_tsv)
    expect_value "$variable session SET" "5|5|4
innodb_parallel_read_threads|5
innodb_parallel_read_threads|4" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET LOCAL $variable = 6; SELECT @@$variable, @@LOCAL.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable local SET" "6|6|4" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = 6; SET GLOBAL $variable = 7; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable global SET" "6|6|7" "$actual"

    actual=$(run_mysql "SET SESSION $variable = DEFAULT; SET $variable = 0; SHOW WARNINGS; SELECT @@$variable;" \
        | normalize_tsv)
    expect_value "$variable low clamp" \
        "Warning|1292|Truncated incorrect innodb_parallel_read_threads value: '0'
1" \
        "$actual"
    actual=$(run_mysql "SET SESSION $variable = DEFAULT; SET $variable = 257; SHOW WARNINGS; SELECT @@$variable;" \
        | normalize_tsv)
    expect_value "$variable high clamp" \
        "Warning|1292|Truncated incorrect innodb_parallel_read_threads value: '257'
256" \
        "$actual"

    expect_error \
        "$variable bad type" \
        1232 \
        42000 \
        "Incorrect argument type to variable '$variable'" \
        "SET $variable = 'bad';"
    run_mysql "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

variables | while IFS='|' read -r variable scalar_value show_value mutation exact alternate; do
    check_default_readbacks "$variable" "$scalar_value" "$show_value" "$mutation"
    case "$mutation" in
        read_only)
            check_read_only_assignments "$variable"
            ;;
        dynamic_numeric|dynamic_boolean)
            check_dynamic_global_assignments "$variable" "$mutation" "$exact" "$alternate"
            ;;
        dynamic_session)
            check_parallel_read_threads_assignments
            ;;
        *) fail "$variable: unknown mutation class [$mutation]" ;;
    esac
    check_default_readbacks "$variable" "$scalar_value" "$show_value" "$mutation"
done

printf '%s\n' "mysql_baseline_innodb_page_read_purge_system_variables_expectations: ok"
