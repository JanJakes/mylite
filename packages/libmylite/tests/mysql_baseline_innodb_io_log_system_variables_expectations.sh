#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_io_log_system_variables_expectations: $1" >&2
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

global_dynamic_variables() {
    cat <<'EOF'
innodb_idle_flush_pct|100|100|100|101|integer
innodb_io_capacity|10000|10000|10000|10001|integer
innodb_io_capacity_max|20000|20000|20000|20001|integer
innodb_log_buffer_size|67108864|67108864|67108864|67108865|integer
innodb_log_checksums|1|ON|ON|OFF|boolean
innodb_log_compressed_pages|1|ON|ON|OFF|boolean
innodb_log_spin_cpu_abs_lwm|80|80|80|81|integer
innodb_log_spin_cpu_pct_hwm|50|50|50|51|integer
innodb_log_wait_for_flush_spin_hwm|400|400|400|401|integer
innodb_log_write_ahead_size|8192|8192|8192|8193|integer
innodb_log_writer_threads|1|ON|ON|OFF|boolean
innodb_lru_scan_depth|1024|1024|1024|1025|integer
EOF
}

read_only_variables() {
    cat <<'EOF'
innodb_log_file_size|50331648|50331648
innodb_log_files_in_group|2|2
innodb_log_group_home_dir|./|./
EOF
}

reset_defaults() {
    global_dynamic_variables | while IFS='|' read -r variable _scalar _show _exact _bad _kind; do
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    done
    run_mysql "SET GLOBAL innodb_lock_wait_timeout = DEFAULT; \
               SET SESSION innodb_lock_wait_timeout = DEFAULT;" >/dev/null
}

check_show_values() {
    variable=$1
    scalar=$2
    show=$3
    expected_pair="$scalar|$scalar"
    expected_show="$variable|$show"

    actual_pair=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar" "$expected_pair" "$actual_pair"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

global_dynamic_variables | while IFS='|' read -r variable scalar show exact bad kind; do
    check_show_values "$variable" "$scalar" "$show"
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
    run_mysql "SET GLOBAL $variable = $bad; SET GLOBAL $variable = DEFAULT;" >/dev/null
    if [ "$kind" = "boolean" ]; then
        actual_toggle=$(run_mysql "SET GLOBAL $variable = OFF; \
                                   SELECT @@GLOBAL.$variable; \
                                   SET GLOBAL $variable = DEFAULT;" | normalize_tsv)
        expect_value "$variable global boolean toggle" "0" "$actual_toggle"
    fi
done

read_only_variables | while IFS='|' read -r variable scalar show; do
    check_show_values "$variable" "$scalar" "$show"
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "$variable read-only SET" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
done

check_show_values "innodb_lock_wait_timeout" "50" "50"
actual_session=$(
    run_mysql "SELECT @@SESSION.innodb_lock_wait_timeout, @@LOCAL.innodb_lock_wait_timeout;" \
        | normalize_tsv
)
expect_value "innodb_lock_wait_timeout session scalar" "50|50" "$actual_session"
run_mysql "SET innodb_lock_wait_timeout = DEFAULT; \
           SET SESSION innodb_lock_wait_timeout = DEFAULT; \
           SET LOCAL innodb_lock_wait_timeout = DEFAULT;" >/dev/null
actual_mutation=$(
    run_mysql "SET SESSION innodb_lock_wait_timeout = 49; \
               SELECT @@innodb_lock_wait_timeout, @@SESSION.innodb_lock_wait_timeout, \
                      @@GLOBAL.innodb_lock_wait_timeout; \
               SHOW VARIABLES LIKE 'innodb_lock_wait_timeout'; \
               SHOW GLOBAL VARIABLES LIKE 'innodb_lock_wait_timeout'; \
               SET SESSION innodb_lock_wait_timeout = DEFAULT;" \
        | normalize_tsv
)
expect_value \
    "innodb_lock_wait_timeout session mutation" \
    "49|49|50
innodb_lock_wait_timeout|49
innodb_lock_wait_timeout|50" \
    "$actual_mutation"
run_mysql "SET GLOBAL innodb_lock_wait_timeout = 49; SET GLOBAL innodb_lock_wait_timeout = DEFAULT;" \
    >/dev/null

printf '%s\n' "mysql_baseline_innodb_io_log_system_variables_expectations: ok"
