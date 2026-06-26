#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_stats_status_thread_undo_system_variables_expectations: $1" >&2
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
innodb_stats_auto_recalc|1|ON|dynamic_boolean|ON|OFF
innodb_stats_include_delete_marked|0|OFF|dynamic_boolean|OFF|ON
innodb_stats_method|nulls_equal|nulls_equal|dynamic_text|'nulls_equal'|'nulls_unequal'
innodb_stats_on_metadata|0|OFF|dynamic_boolean|OFF|ON
innodb_stats_persistent|1|ON|dynamic_boolean|ON|OFF
innodb_stats_persistent_sample_pages|20|20|dynamic_numeric|20|21
innodb_stats_transient_sample_pages|8|8|dynamic_numeric|8|9
innodb_status_output|0|OFF|dynamic_boolean|OFF|ON
innodb_status_output_locks|0|OFF|dynamic_boolean|OFF|ON
innodb_strict_mode|1|ON|dynamic_session_boolean|ON|OFF
innodb_sync_array_size|1|1|read_only||
innodb_sync_spin_loops|30|30|dynamic_numeric|30|31
innodb_table_locks|1|ON|dynamic_session_boolean|ON|OFF
innodb_temp_data_file_path|ibtmp1:12M:autoextend|ibtmp1:12M:autoextend|read_only||
innodb_temp_tablespaces_dir|./#innodb_temp/|./#innodb_temp/|read_only||
innodb_thread_concurrency|0|0|dynamic_numeric|0|1
innodb_thread_sleep_delay|10000|10000|dynamic_numeric|10000|10001
innodb_tmpdir|NULL||dynamic_session_null_text|NULL|'/tmp'
innodb_undo_directory|./|./|read_only||
innodb_undo_log_encrypt|0|OFF|dynamic_boolean|OFF|ON
innodb_undo_log_truncate|1|ON|dynamic_boolean|ON|OFF
innodb_undo_tablespaces|2|2|dynamic_numeric|2|3
innodb_use_fdatasync|1|ON|dynamic_boolean|ON|OFF
innodb_use_native_aio|1|ON|read_only||
innodb_validate_tablespace_paths|1|ON|read_only||
innodb_version|8.4.9|8.4.9|read_only||
innodb_write_io_threads|4|4|read_only||
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show mutation _exact _alternate; do
        case "$mutation" in
            dynamic_boolean|dynamic_text|dynamic_numeric)
                run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null 2>&1 || true
                ;;
            dynamic_session_boolean|dynamic_session_null_text)
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

    case "$mutation" in
        dynamic_session_boolean|dynamic_session_null_text)
            actual_session_scalar=$(run_mysql "SELECT @@SESSION.$variable, @@LOCAL.$variable;" | normalize_tsv)
            expect_value \
                "$variable session scalar" \
                "$scalar_value|$scalar_value" \
                "$actual_session_scalar"
            ;;
        *)
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
            ;;
    esac
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
    run_mysql "SET GLOBAL $variable = $alternate; SET GLOBAL $variable = DEFAULT;" >/dev/null

    case "$mutation" in
        dynamic_boolean|dynamic_text)
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
            ;;
        dynamic_numeric)
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
            ;;
        *) ;;
    esac
}

check_session_boolean_assignments() {
    variable=$1
    scalar_value=$2
    show_value=$3
    exact=$4
    alternate=$5
    alternate_scalar=0
    alternate_show=OFF

    if [ "$alternate" = "ON" ]; then
        alternate_scalar=1
        alternate_show=ON
    fi

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET $variable = $alternate; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable; SHOW VARIABLES LIKE '$variable'; SHOW GLOBAL VARIABLES LIKE '$variable';" \
        | normalize_tsv)
    expect_value "$variable session SET" \
        "$alternate_scalar|$alternate_scalar|$scalar_value
$variable|$alternate_show
$variable|$show_value" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET LOCAL $variable = $alternate; SELECT @@$variable, @@LOCAL.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable local SET" \
        "$alternate_scalar|$alternate_scalar|$scalar_value" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = $alternate; SET GLOBAL $variable = $alternate; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable global SET" \
        "$alternate_scalar|$alternate_scalar|$alternate_scalar" "$actual"

    run_mysql "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT;" >/dev/null
    run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null

    expect_error \
        "$variable null SET" \
        1231 \
        42000 \
        "Variable '$variable' can't be set to the value of 'NULL'" \
        "SET $variable = NULL;"
    expect_error \
        "$variable bogus SET" \
        1231 \
        42000 \
        "Variable '$variable' can't be set to the value of 'bogus'" \
        "SET $variable = 'bogus';"
}

check_tmpdir_assignments() {
    variable=innodb_tmpdir

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET $variable = '/tmp'; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable; SHOW VARIABLES LIKE '$variable'; SHOW GLOBAL VARIABLES LIKE '$variable';" \
        | normalize_tsv)
    expect_value "$variable session SET" \
        "/tmp|/tmp|NULL
$variable|/tmp
$variable|" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET LOCAL $variable = '/tmp'; SELECT @@$variable, @@LOCAL.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable local SET" "/tmp|/tmp|NULL" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = '/tmp'; SET GLOBAL $variable = '/tmp'; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable;" \
        | normalize_tsv)
    expect_value "$variable global SET" "/tmp|/tmp|/tmp" "$actual"

    actual=$(run_mysql \
        "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = '/tmp'; SET SESSION $variable = NULL; SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable; SHOW VARIABLES LIKE '$variable';" \
        | normalize_tsv)
    expect_value "$variable null SET" "NULL|NULL|NULL
$variable|" "$actual"

    expect_error \
        "$variable bogus SET" \
        1231 \
        42000 \
        "Variable '$variable' can't be set to the value of 'bogus'" \
        "SET $variable = 'bogus';"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

variables | while IFS='|' read -r variable scalar show mutation exact alternate; do
    check_default_readbacks "$variable" "$scalar" "$show" "$mutation"
    case "$mutation" in
        read_only)
            check_read_only_assignments "$variable"
            ;;
        dynamic_boolean|dynamic_text|dynamic_numeric)
            check_dynamic_global_assignments "$variable" "$mutation" "$exact" "$alternate"
            ;;
        dynamic_session_boolean)
            check_session_boolean_assignments "$variable" "$scalar" "$show" "$exact" "$alternate"
            ;;
        dynamic_session_null_text)
            check_tmpdir_assignments
            ;;
        *) fail "unknown mutation [$mutation] for $variable" ;;
    esac
done

printf '%s\n' "mysql_baseline_innodb_stats_status_thread_undo_system_variables_expectations: ok"
