#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_core_system_variables_expectations: $1" >&2
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
innodb_adaptive_flushing|1|ON|ON|dynamic
innodb_adaptive_flushing_lwm|10|10|10|dynamic
innodb_adaptive_hash_index|0|OFF|OFF|dynamic
innodb_adaptive_hash_index_parts|8|8|8|read_only
innodb_adaptive_max_sleep_delay|150000|150000|150000|dynamic
innodb_autoextend_increment|64|64|64|dynamic
innodb_autoinc_lock_mode|2|2|2|read_only
innodb_buffer_pool_chunk_size|134217728|134217728|134217728|read_only
innodb_buffer_pool_dump_at_shutdown|1|ON|ON|dynamic
innodb_buffer_pool_dump_now|0|OFF|OFF|dynamic
innodb_buffer_pool_dump_pct|25|25|25|dynamic
innodb_buffer_pool_filename|ib_buffer_pool|ib_buffer_pool|'ib_buffer_pool'|dynamic
innodb_buffer_pool_in_core_file|0|OFF|OFF|dynamic
innodb_buffer_pool_instances|1|1|1|read_only
innodb_buffer_pool_load_abort|0|OFF|OFF|dynamic
innodb_buffer_pool_load_at_startup|1|ON|ON|read_only
innodb_buffer_pool_load_now|0|OFF|OFF|dynamic
innodb_buffer_pool_size|134217728|134217728|134217728|dynamic
innodb_change_buffer_max_size|25|25|25|dynamic
innodb_change_buffering|none|none|'none'|dynamic
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show _exact mode; do
        [ "$mode" = "dynamic" ] || continue
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    done
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

variables | while IFS='|' read -r variable scalar show exact mode; do
    expected_pair="$scalar|$scalar"
    actual_pair=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar" "$expected_pair" "$actual_pair"

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

    if [ "$mode" = "read_only" ]; then
        expect_error \
            "$variable read-only unscoped SET" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET $variable = DEFAULT;"
        expect_error \
            "$variable read-only session SET" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET SESSION $variable = DEFAULT;"
        expect_error \
            "$variable read-only global SET" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET GLOBAL $variable = DEFAULT;"
    else
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

        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
        run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
    fi
done

printf '%s\n' "mysql_baseline_innodb_core_system_variables_expectations: ok"
