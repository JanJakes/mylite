#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_replica_applier_system_variables_expectations: $1" >&2
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
replica_allow_batching|ON
replica_checkpoint_group|512
replica_checkpoint_period|300
replica_compressed_protocol|OFF
replica_exec_mode|STRICT
replica_load_tmpdir|/tmp
replica_max_allowed_packet|1073741824
replica_net_timeout|60
replica_parallel_type|LOGICAL_CLOCK
replica_parallel_workers|4
replica_pending_jobs_size_max|134217728
replica_preserve_commit_order|ON
replica_skip_errors|OFF
replica_sql_verify_checksum|ON
replica_transaction_retries|10
replica_type_conversions|
slave_allow_batching|ON
slave_checkpoint_group|512
slave_checkpoint_period|300
slave_compressed_protocol|OFF
slave_exec_mode|STRICT
slave_load_tmpdir|/tmp
slave_max_allowed_packet|1073741824
slave_net_timeout|60
slave_parallel_type|LOGICAL_CLOCK
slave_parallel_workers|4
slave_pending_jobs_size_max|134217728
slave_preserve_commit_order|ON
slave_skip_errors|OFF
slave_sql_verify_checksum|ON
slave_transaction_retries|10
slave_type_conversions|
EOF
}

variable_names_in_clause() {
    variables | awk -F'|' '{printf sep "'\''" $1 "'\''"; sep=","}'
}

dynamic_variables() {
    cat <<'EOF'
replica_allow_batching
replica_checkpoint_group
replica_checkpoint_period
replica_compressed_protocol
replica_exec_mode
replica_max_allowed_packet
replica_net_timeout
replica_parallel_type
replica_parallel_workers
replica_pending_jobs_size_max
replica_preserve_commit_order
replica_sql_verify_checksum
replica_transaction_retries
replica_type_conversions
slave_allow_batching
slave_checkpoint_group
slave_checkpoint_period
slave_compressed_protocol
slave_exec_mode
slave_max_allowed_packet
slave_net_timeout
slave_parallel_type
slave_parallel_workers
slave_pending_jobs_size_max
slave_preserve_commit_order
slave_sql_verify_checksum
slave_transaction_retries
slave_type_conversions
EOF
}

read_only_variables() {
    cat <<'EOF'
replica_load_tmpdir
replica_skip_errors
slave_load_tmpdir
slave_skip_errors
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

deprecated_scalar_variables() {
    cat <<'EOF'
replica_parallel_type|
slave_allow_batching|replica_allow_batching
slave_checkpoint_group|replica_checkpoint_group
slave_checkpoint_period|replica_checkpoint_period
slave_compressed_protocol|replica_compressed_protocol
slave_exec_mode|replica_exec_mode
slave_load_tmpdir|replica_load_tmpdir
slave_max_allowed_packet|replica_max_allowed_packet
slave_net_timeout|replica_net_timeout
slave_parallel_type|replica_parallel_type
slave_parallel_workers|replica_parallel_workers
slave_pending_jobs_size_max|replica_pending_jobs_size_max
slave_preserve_commit_order|replica_preserve_commit_order
slave_skip_errors|replica_skip_errors
slave_sql_verify_checksum|replica_sql_verify_checksum
slave_transaction_retries|replica_transaction_retries
slave_type_conversions|replica_type_conversions
EOF
}

deprecated_set_variables() {
    cat <<'EOF'
replica_parallel_type|
slave_allow_batching|replica_allow_batching
slave_checkpoint_group|replica_checkpoint_group
slave_checkpoint_period|replica_checkpoint_period
slave_compressed_protocol|replica_compressed_protocol
slave_exec_mode|replica_exec_mode
slave_max_allowed_packet|replica_max_allowed_packet
slave_net_timeout|replica_net_timeout
slave_parallel_type|replica_parallel_type
slave_parallel_workers|replica_parallel_workers
slave_pending_jobs_size_max|replica_pending_jobs_size_max
slave_preserve_commit_order|replica_preserve_commit_order
slave_sql_verify_checksum|replica_sql_verify_checksum
slave_transaction_retries|replica_transaction_retries
slave_type_conversions|replica_type_conversions
EOF
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
        "SELECT @@replica_allow_batching, @@replica_checkpoint_group,
                @@replica_checkpoint_period, @@replica_compressed_protocol,
                @@replica_exec_mode, @@replica_load_tmpdir,
                @@replica_max_allowed_packet, @@replica_net_timeout,
                @@replica_parallel_type, @@replica_parallel_workers,
                @@replica_pending_jobs_size_max, @@replica_preserve_commit_order,
                @@replica_skip_errors, @@replica_sql_verify_checksum,
                @@replica_transaction_retries, @@replica_type_conversions,
                @@slave_allow_batching, @@slave_checkpoint_group,
                @@slave_checkpoint_period, @@slave_compressed_protocol,
                @@slave_exec_mode, @@slave_load_tmpdir,
                @@slave_max_allowed_packet, @@slave_net_timeout,
                @@slave_parallel_type, @@slave_parallel_workers,
                @@slave_pending_jobs_size_max, @@slave_preserve_commit_order,
                @@slave_skip_errors, @@slave_sql_verify_checksum,
                @@slave_transaction_retries, @@slave_type_conversions,
                @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "scalar shapes" \
    "1|512|300|0|STRICT|/tmp|1073741824|60|LOGICAL_CLOCK|4|134217728|1|OFF|1|10||1|512|300|0|STRICT|/tmp|1073741824|60|LOGICAL_CLOCK|4|134217728|1|OFF|1|10||17" \
    "$scalar_shapes"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show default rows" "$(variables)" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show global rows" "$(variables)" "$show_global"

show_session=$(
    run_mysql "SHOW SESSION VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
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

deprecated_scalar_variables | while IFS='|' read -r variable replacement; do
    expect_warning_after_select "$variable" "$replacement"
done

deprecated_set_variables | while IFS='|' read -r variable replacement; do
    expect_warning_after_default_set "$variable" "$replacement"
done

printf '%s\n' "mysql_baseline_replica_applier_system_variables_expectations: ok"
