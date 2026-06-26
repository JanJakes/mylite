#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_redo_rollback_spin_system_variables_expectations: $1" >&2
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
innodb_redo_log_archive_dirs|NULL||dynamic_null|NULL|'bogus'
innodb_redo_log_capacity|104857600|104857600|dynamic_numeric|104857600|104857601
innodb_redo_log_encrypt|0|OFF|dynamic_boolean|OFF|ON
innodb_replication_delay|0|0|dynamic_numeric|0|1
innodb_rollback_on_timeout|0|OFF|read_only||
innodb_rollback_segments|128|128|dynamic_numeric|128|127
innodb_segment_reserve_factor|12.500000|12.500000|dynamic_decimal|12.5|13.5
innodb_sort_buffer_size|1048576|1048576|read_only||
innodb_spin_wait_delay|6|6|dynamic_numeric|6|7
innodb_spin_wait_pause_multiplier|50|50|dynamic_numeric|50|51
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show mutation _exact _alternate; do
        case "$mutation" in
            dynamic_null|dynamic_numeric|dynamic_boolean|dynamic_decimal)
                run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null 2>&1 || true
                ;;
            *) ;;
        esac
    done
}

check_default_readbacks() {
    variable=$1
    scalar_value=$2
    show_value=$3
    expected_show="$variable|$show_value"

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar default" "$scalar_value|$scalar_value" "$actual_scalar"
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
    case "$mutation" in
        dynamic_null)
            expect_error \
                "$variable bogus SET" \
                1231 \
                42000 \
                "Variable '$variable' can't be set to the value of 'bogus'" \
                "SET GLOBAL $variable = $alternate;"
            ;;
        dynamic_boolean)
            run_mysql "SET GLOBAL $variable = $alternate; SET GLOBAL $variable = DEFAULT;" >/dev/null
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
        dynamic_numeric|dynamic_decimal)
            run_mysql "SET GLOBAL $variable = $alternate; SET GLOBAL $variable = DEFAULT;" >/dev/null
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

variables | while IFS='|' read -r variable scalar show mutation exact alternate; do
    check_default_readbacks "$variable" "$scalar" "$show"
    case "$mutation" in
        read_only)
            check_read_only_assignments "$variable"
            ;;
        *)
            check_dynamic_global_assignments "$variable" "$mutation" "$exact" "$alternate"
            ;;
    esac
done

printf '%s\n' "mysql_baseline_innodb_redo_rollback_spin_system_variables_expectations: ok"
