#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')
MASTER_WARNING="'@@master_verify_checksum' is deprecated and will be removed in a future release. Please use source_verify_checksum instead."

fail() {
    printf '%s\n' "mysql_baseline_m_server_limit_system_variables_expectations: $1" >&2
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
        "SET GLOBAL master_verify_checksum = DEFAULT;
         SET GLOBAL max_binlog_cache_size = DEFAULT;
         SET GLOBAL max_binlog_size = DEFAULT;
         SET GLOBAL max_binlog_stmt_cache_size = DEFAULT;
         SET GLOBAL max_connect_errors = DEFAULT;
         SET GLOBAL max_connections = DEFAULT;
         SET GLOBAL max_prepared_stmt_count = DEFAULT;
         SET GLOBAL max_relay_log_size = DEFAULT;
         SET GLOBAL max_write_lock_count = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

defaults=$(
    run_mysql \
        "SELECT @@master_verify_checksum, @@GLOBAL.master_verify_checksum,
                @@max_binlog_cache_size, @@max_binlog_size,
                @@max_binlog_stmt_cache_size, @@max_connect_errors,
                @@max_connections, @@max_digest_length,
                @@max_prepared_stmt_count, @@max_relay_log_size,
                @@max_write_lock_count;" \
        | normalize_tsv
)
expect_value \
    "M server limit defaults" \
    "0|0|18446744073709547520|1073741824|18446744073709547520|100|151|1024|16382|0|18446744073709551615" \
    "$defaults"

master_read_warning=$(
    run_mysql \
        "SELECT @@master_verify_checksum;
         SHOW WARNINGS;" \
        | normalize_tsv
)
expect_value \
    "master_verify_checksum read warning" \
    "0
Warning|1287|$MASTER_WARNING" \
    "$master_read_warning"

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
master_verify_checksum|OFF
max_binlog_cache_size|18446744073709547520
max_binlog_size|1073741824
max_binlog_stmt_cache_size|18446744073709547520
max_connect_errors|100
max_connections|151
max_digest_length|1024
max_prepared_stmt_count|16382
max_relay_log_size|0
max_write_lock_count|18446744073709551615
EOF

for variable in \
    master_verify_checksum \
    max_binlog_cache_size \
    max_binlog_size \
    max_binlog_stmt_cache_size \
    max_connect_errors \
    max_connections \
    max_digest_length \
    max_prepared_stmt_count \
    max_relay_log_size \
    max_write_lock_count
do
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
done

for variable in \
    master_verify_checksum \
    max_binlog_cache_size \
    max_binlog_size \
    max_binlog_stmt_cache_size \
    max_connect_errors \
    max_connections \
    max_prepared_stmt_count \
    max_relay_log_size \
    max_write_lock_count
do
    expect_error \
        "$variable set global-only" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

expect_error \
    "max_digest_length read-only SET" \
    1238 \
    HY000 \
    "Variable 'max_digest_length' is a read only variable" \
    "SET GLOBAL max_digest_length = DEFAULT;"

global_noops=$(
    run_mysql \
        "SET GLOBAL master_verify_checksum = DEFAULT;
         SHOW WARNINGS;
         SET GLOBAL master_verify_checksum = OFF;
         SHOW WARNINGS;
         SET GLOBAL max_binlog_cache_size = DEFAULT;
         SET GLOBAL max_binlog_cache_size = 18446744073709547520;
         SET GLOBAL max_binlog_size = DEFAULT;
         SET GLOBAL max_binlog_size = 1073741824;
         SET GLOBAL max_binlog_stmt_cache_size = DEFAULT;
         SET GLOBAL max_binlog_stmt_cache_size = 18446744073709547520;
         SET GLOBAL max_connect_errors = DEFAULT;
         SET GLOBAL max_connect_errors = 100;
         SET GLOBAL max_connections = DEFAULT;
         SET GLOBAL max_connections = 151;
         SET GLOBAL max_prepared_stmt_count = DEFAULT;
         SET GLOBAL max_prepared_stmt_count = 16382;
         SET GLOBAL max_relay_log_size = DEFAULT;
         SET GLOBAL max_relay_log_size = 0;
         SET GLOBAL max_write_lock_count = DEFAULT;
         SET GLOBAL max_write_lock_count = 18446744073709551615;
         SELECT @@GLOBAL.master_verify_checksum, @@GLOBAL.max_binlog_cache_size,
                @@GLOBAL.max_binlog_size, @@GLOBAL.max_binlog_stmt_cache_size,
                @@GLOBAL.max_connect_errors, @@GLOBAL.max_connections,
                @@GLOBAL.max_prepared_stmt_count, @@GLOBAL.max_relay_log_size,
                @@GLOBAL.max_write_lock_count;" \
        | normalize_tsv
)
expect_value \
    "fixed M server limit global no-ops" \
    "Warning|1287|$MASTER_WARNING
Warning|1287|$MASTER_WARNING
0|18446744073709547520|1073741824|18446744073709547520|100|151|16382|0|18446744073709551615" \
    "$global_noops"

global_mutation=$(
    run_mysql \
        "SET GLOBAL max_connect_errors = 101;
         SET GLOBAL max_connections = 152;
         SELECT @@GLOBAL.max_connect_errors, @@GLOBAL.max_connections;" \
        | normalize_tsv
)
expect_value "mysql mutable global observation" "101|152" "$global_mutation"

reset_defaults

printf '%s\n' "mysql_baseline_m_server_limit_system_variables_expectations: ok"
