#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_replication_global_system_variables_expectations: $1" >&2
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

normalize_rows() {
    sed \
        -e 's#^relay_log|.*#relay_log|<relay_log>#' \
        -e 's#^relay_log_basename|.*#relay_log_basename|<relay_log_basename>#' \
        -e 's#^relay_log_index|.*#relay_log_index|<relay_log_index>#'
}

variables() {
    cat <<'EOF'
relay_log|<relay_log>
relay_log_basename|<relay_log_basename>
relay_log_index|<relay_log_index>
relay_log_purge|ON
relay_log_recovery|OFF
relay_log_space_limit|0
replication_optimize_for_static_plugin_config|OFF
replication_sender_observe_commit_only|OFF
report_host|
report_password|
report_port|3306
report_user|
rpl_read_size|8192
rpl_stop_replica_timeout|31536000
rpl_stop_slave_timeout|31536000
skip_replica_start|OFF
skip_slave_start|OFF
source_verify_checksum|OFF
sync_master_info|10000
sync_relay_log|10000
sync_relay_log_info|10000
sync_source_info|10000
EOF
}

variable_names_in_clause() {
    variables | awk -F'|' '{printf sep "'\''" $1 "'\''"; sep=","}'
}

dynamic_variables() {
    cat <<'EOF'
relay_log_purge
replication_optimize_for_static_plugin_config
replication_sender_observe_commit_only
rpl_read_size
rpl_stop_replica_timeout
rpl_stop_slave_timeout
source_verify_checksum
sync_master_info
sync_relay_log
sync_relay_log_info
sync_source_info
EOF
}

read_only_variables() {
    cat <<'EOF'
relay_log
relay_log_basename
relay_log_index
relay_log_recovery
relay_log_space_limit
report_host
report_password
report_port
report_user
skip_replica_start
skip_slave_start
EOF
}

deprecated_warning() {
    variable=$1
    replacement=$2

    if [ "$replacement" = "" ]; then
        printf "'@@%s' is deprecated and will be removed in a future release." "$variable"
    else
        printf "'@@%s' is deprecated and will be removed in a future release. Please use %s instead." \
            "$variable" \
            "$replacement"
    fi
}

expect_warning_after_select() {
    variable=$1
    replacement=$2
    expected=$(deprecated_warning "$variable" "$replacement")
    output=$(
        run_mysql "SELECT @@GLOBAL.${variable}; SHOW WARNINGS LIMIT 1;" \
            | normalize_tsv
    )

    case "$output" in
        *"Warning|1287|${expected}"*) ;;
        *) fail "scalar warning ${variable}: expected [$expected], got [$output]" ;;
    esac
}

expect_warning_after_default_set() {
    variable=$1
    replacement=$2
    expected=$(deprecated_warning "$variable" "$replacement")
    output=$(
        run_mysql "SET GLOBAL ${variable} = DEFAULT; SHOW WARNINGS LIMIT 1;" \
            | normalize_tsv
    )

    expect_value "default SET warning ${variable}" "Warning|1287|${expected}" "$output"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar_shapes=$(
    run_mysql \
        "SELECT @@relay_log REGEXP '.+', @@GLOBAL.relay_log_basename REGEXP '.+',
                @@relay_log_index REGEXP '.+', @@relay_log_purge, @@relay_log_recovery,
                @@relay_log_space_limit,
                @@replication_optimize_for_static_plugin_config,
                @@replication_sender_observe_commit_only,
                @@report_host IS NULL, @@report_password IS NULL, @@report_port,
                @@report_user IS NULL, @@rpl_read_size,
                @@rpl_stop_replica_timeout, @@source_verify_checksum,
                @@sync_relay_log, @@sync_source_info;" \
        | normalize_tsv
)
expect_value \
    "scalar shapes" \
    "1|1|1|1|0|0|0|0|1|1|3306|1|8192|31536000|0|10000|10000" \
    "$scalar_shapes"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv \
        | normalize_rows
)
expect_value "show default rows" "$(variables)" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv \
        | normalize_rows
)
expect_value "show global rows" "$(variables)" "$show_global"

show_session=$(
    run_mysql "SHOW SESSION VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv \
        | normalize_rows
)
expect_value "show session rows" "$(variables)" "$show_session"

for variable in $(variables | awk -F'|' '{print $1}'); do
    expect_error \
        "session scalar ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable" \
        "SELECT @@SESSION.${variable};"

    expect_error \
        "local scalar ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable" \
        "SELECT @@LOCAL.${variable};"
done

for variable in $(read_only_variables); do
    expect_error \
        "read-only ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a read only variable" \
        "SET GLOBAL ${variable} = DEFAULT;"
done

for variable in $(dynamic_variables); do
    run_mysql "SET GLOBAL ${variable} = DEFAULT;" >/dev/null
done

expect_warning_after_select rpl_stop_slave_timeout rpl_stop_replica_timeout
expect_warning_after_select skip_slave_start skip_replica_start
expect_warning_after_select sync_master_info sync_source_info
expect_warning_after_select sync_relay_log_info ""

expect_warning_after_default_set rpl_stop_slave_timeout rpl_stop_replica_timeout
expect_warning_after_default_set sync_master_info sync_source_info
expect_warning_after_default_set sync_relay_log_info ""

printf '%s\n' "mysql_baseline_replication_global_system_variables_expectations: ok"
