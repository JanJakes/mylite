#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_connection_system_variables_expectations: $1" >&2
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

ensure_plugin_active() {
    plugin_name=$1
    plugin_library=$2

    plugin_status=$(run_mysql \
        "SELECT COALESCE(MAX(PLUGIN_STATUS), '') FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = '${plugin_name}';")
    case "$plugin_status" in
        ACTIVE) return 0 ;;
        "") ;;
        *) fail "${plugin_name} plugin status is ${plugin_status}" ;;
    esac

    run_mysql "INSTALL PLUGIN ${plugin_name} SONAME '${plugin_library}';" >/dev/null
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

reset_mutable_globals() {
    run_mysql \
        "SET GLOBAL host_cache_size = 0;
         SET GLOBAL global_connection_memory_limit = DEFAULT;
         SET GLOBAL global_connection_memory_tracking = DEFAULT;
         SET GLOBAL connection_control_failed_connections_threshold = DEFAULT;
         SET GLOBAL connection_control_max_connection_delay = DEFAULT;
         SET GLOBAL connection_control_min_connection_delay = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

ensure_plugin_active "CONNECTION_CONTROL" "connection_control.so"
reset_mutable_globals

defaults=$(
    run_mysql \
        "SELECT @@back_log, @@GLOBAL.back_log,
                @@bind_address, @@GLOBAL.bind_address,
                @@host_cache_size, @@GLOBAL.host_cache_size,
                @@connection_memory_chunk_size, @@GLOBAL.connection_memory_chunk_size,
                @@SESSION.connection_memory_chunk_size,
                @@connection_memory_limit, @@GLOBAL.connection_memory_limit,
                @@SESSION.connection_memory_limit,
                @@global_connection_memory_limit, @@GLOBAL.global_connection_memory_limit,
                @@global_connection_memory_tracking, @@GLOBAL.global_connection_memory_tracking,
                @@SESSION.global_connection_memory_tracking,
                @@connection_control_failed_connections_threshold,
                @@GLOBAL.connection_control_failed_connections_threshold,
                @@connection_control_max_connection_delay,
                @@GLOBAL.connection_control_max_connection_delay,
                @@connection_control_min_connection_delay,
                @@GLOBAL.connection_control_min_connection_delay;"
)
expect_value \
    "connection system defaults" \
    "151${TAB}151${TAB}*${TAB}*${TAB}0${TAB}0${TAB}8192${TAB}8192${TAB}8192${TAB}18446744073709551615${TAB}18446744073709551615${TAB}18446744073709551615${TAB}18446744073709551615${TAB}18446744073709551615${TAB}0${TAB}0${TAB}0${TAB}3${TAB}3${TAB}2147483647${TAB}2147483647${TAB}1000${TAB}1000" \
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
back_log|151
bind_address|*
host_cache_size|0
connection_memory_chunk_size|8192
connection_memory_limit|18446744073709551615
global_connection_memory_limit|18446744073709551615
global_connection_memory_tracking|OFF
connection_control_failed_connections_threshold|3
connection_control_max_connection_delay|2147483647
connection_control_min_connection_delay|1000
EOF

expect_error \
    "back_log session scalar" \
    1238 \
    HY000 \
    "Variable 'back_log' is a GLOBAL variable" \
    "SELECT @@SESSION.back_log;"
expect_error \
    "bind_address session scalar" \
    1238 \
    HY000 \
    "Variable 'bind_address' is a GLOBAL variable" \
    "SELECT @@SESSION.bind_address;"
expect_error \
    "host_cache_size session scalar" \
    1238 \
    HY000 \
    "Variable 'host_cache_size' is a GLOBAL variable" \
    "SELECT @@SESSION.host_cache_size;"
expect_error \
    "global_connection_memory_limit session scalar" \
    1238 \
    HY000 \
    "Variable 'global_connection_memory_limit' is a GLOBAL variable" \
    "SELECT @@SESSION.global_connection_memory_limit;"
expect_error \
    "connection control session scalar" \
    1238 \
    HY000 \
    "Variable 'connection_control_failed_connections_threshold' is a GLOBAL variable" \
    "SELECT @@SESSION.connection_control_failed_connections_threshold;"

expect_error \
    "back_log read-only SET" \
    1238 \
    HY000 \
    "Variable 'back_log' is a read only variable" \
    "SET GLOBAL back_log = DEFAULT;"
expect_error \
    "bind_address read-only SET" \
    1238 \
    HY000 \
    "Variable 'bind_address' is a read only variable" \
    "SET GLOBAL bind_address = DEFAULT;"
expect_error \
    "host_cache_size global-only SET" \
    1229 \
    HY000 \
    "Variable 'host_cache_size' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET host_cache_size = DEFAULT;"

mutable_globals=$(
    run_mysql \
        "SET GLOBAL host_cache_size = 1;
         SET GLOBAL connection_control_failed_connections_threshold = 4;
         SET GLOBAL connection_control_min_connection_delay = 1001;
         SET GLOBAL connection_control_max_connection_delay = 1002;
         SET GLOBAL global_connection_memory_tracking = ON;
         SELECT @@GLOBAL.host_cache_size,
                @@GLOBAL.connection_control_failed_connections_threshold,
                @@GLOBAL.connection_control_min_connection_delay,
                @@GLOBAL.connection_control_max_connection_delay,
                @@GLOBAL.global_connection_memory_tracking,
                @@warning_count;"
)
expect_value \
    "connection mutable MySQL globals" \
    "1${TAB}4${TAB}1001${TAB}1002${TAB}1${TAB}0" \
    "$mutable_globals"
reset_mutable_globals

session_sets=$(
    run_mysql \
        "SET SESSION connection_memory_chunk_size = 4096;
         SET SESSION connection_memory_limit = 2097152;
         SET SESSION global_connection_memory_tracking = ON;
         SELECT @@connection_memory_chunk_size, @@GLOBAL.connection_memory_chunk_size,
                @@SESSION.connection_memory_chunk_size,
                @@connection_memory_limit, @@GLOBAL.connection_memory_limit,
                @@SESSION.connection_memory_limit,
                @@global_connection_memory_tracking, @@GLOBAL.global_connection_memory_tracking,
                @@SESSION.global_connection_memory_tracking;"
)
expect_value \
    "connection memory mutable session values" \
    "4096${TAB}8192${TAB}4096${TAB}2097152${TAB}18446744073709551615${TAB}2097152${TAB}1${TAB}0${TAB}1" \
    "$session_sets"

clamps=$(
    run_mysql \
        "SET SESSION connection_memory_chunk_size = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@connection_memory_chunk_size, @@warning_count;
         SET SESSION connection_memory_chunk_size = 536870913;
         SHOW WARNINGS LIMIT 1;
         SELECT @@connection_memory_chunk_size, @@warning_count;
         SET SESSION connection_memory_limit = 1;
         SHOW WARNINGS LIMIT 1;
         SELECT @@connection_memory_limit, @@warning_count;
         SET SESSION connection_memory_limit = TRUE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@connection_memory_limit, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "connection memory clamp warnings" \
    "Warning|1292|Truncated incorrect connection_memory_chunk_size value: '0'
1|1
Warning|1292|Truncated incorrect connection_memory_chunk_size value: '536870913'
536870912|1
Warning|1292|Truncated incorrect connection_memory_limit value: '1'
2097152|1
Warning|1292|Truncated incorrect connection_memory_limit value: '1'
2097152|1" \
    "$clamps"

user_variable_sets=$(
    run_mysql \
        "SET @chunk = 16;
         SET SESSION connection_memory_chunk_size = @chunk;
         SET @limit = 2097153;
         SET SESSION connection_memory_limit = @limit;
         SELECT @@connection_memory_chunk_size, @@connection_memory_limit,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "connection memory user variables" \
    "16${TAB}2097153${TAB}0${TAB}0${TAB}0" \
    "$user_variable_sets"

default_sets=$(
    run_mysql \
        "SET SESSION connection_memory_chunk_size = 4096;
         SET SESSION connection_memory_chunk_size = DEFAULT;
         SET SESSION connection_memory_limit = 2097152;
         SET SESSION connection_memory_limit = DEFAULT;
         SET SESSION global_connection_memory_tracking = ON;
         SET SESSION global_connection_memory_tracking = DEFAULT;
         SELECT @@connection_memory_chunk_size, @@connection_memory_limit,
                @@global_connection_memory_tracking,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "connection memory default assignments" \
    "8192${TAB}18446744073709551615${TAB}0${TAB}0${TAB}0${TAB}0" \
    "$default_sets"

expect_error \
    "connection memory chunk string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'connection_memory_chunk_size'" \
    "SET SESSION connection_memory_chunk_size = '4096';"
expect_error \
    "connection memory limit overflow assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'connection_memory_limit'" \
    "SET SESSION connection_memory_limit = 18446744073709551616;"

reset_mutable_globals

printf '%s\n' "mysql_baseline_connection_system_variables_expectations: ok"
