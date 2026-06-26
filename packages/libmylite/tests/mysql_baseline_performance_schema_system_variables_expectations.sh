#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_system_variables_expectations: $1" >&2
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
performance_schema|1|ON|read_only
performance_schema_accounts_size|-1|-1|read_only
performance_schema_digests_size|10000|10000|read_only
performance_schema_error_size|5556|5556|read_only
performance_schema_events_stages_history_long_size|10000|10000|read_only
performance_schema_events_stages_history_size|10|10|read_only
performance_schema_events_statements_history_long_size|10000|10000|read_only
performance_schema_events_statements_history_size|10|10|read_only
performance_schema_events_transactions_history_long_size|10000|10000|read_only
performance_schema_events_transactions_history_size|10|10|read_only
performance_schema_events_waits_history_long_size|10000|10000|read_only
performance_schema_events_waits_history_size|10|10|read_only
performance_schema_hosts_size|-1|-1|read_only
performance_schema_max_cond_classes|150|150|read_only
performance_schema_max_cond_instances|-1|-1|read_only
performance_schema_max_digest_length|1024|1024|read_only
performance_schema_max_digest_sample_age|60|60|dynamic_numeric
performance_schema_max_file_classes|80|80|read_only
performance_schema_max_file_handles|32768|32768|read_only
performance_schema_max_file_instances|-1|-1|read_only
performance_schema_max_index_stat|-1|-1|read_only
performance_schema_max_memory_classes|470|470|read_only
performance_schema_max_metadata_locks|-1|-1|read_only
performance_schema_max_meter_classes|30|30|read_only
performance_schema_max_metric_classes|600|600|read_only
performance_schema_max_mutex_classes|350|350|read_only
performance_schema_max_mutex_instances|-1|-1|read_only
performance_schema_max_prepared_statements_instances|-1|-1|read_only
performance_schema_max_program_instances|-1|-1|read_only
performance_schema_max_rwlock_classes|100|100|read_only
performance_schema_max_rwlock_instances|-1|-1|read_only
performance_schema_max_socket_classes|10|10|read_only
performance_schema_max_socket_instances|-1|-1|read_only
performance_schema_max_sql_text_length|1024|1024|read_only
performance_schema_max_stage_classes|175|175|read_only
performance_schema_max_statement_classes|220|220|read_only
performance_schema_max_statement_stack|10|10|read_only
performance_schema_max_table_handles|-1|-1|read_only
performance_schema_max_table_instances|-1|-1|read_only
performance_schema_max_table_lock_stat|-1|-1|read_only
performance_schema_max_thread_classes|100|100|read_only
performance_schema_max_thread_instances|-1|-1|read_only
performance_schema_session_connect_attrs_size|512|512|read_only
performance_schema_setup_actors_size|-1|-1|read_only
performance_schema_setup_objects_size|-1|-1|read_only
performance_schema_show_processlist|0|OFF|dynamic_boolean
performance_schema_users_size|-1|-1|read_only
EOF
}

variable_names_in_clause() {
    variables | awk -F'|' '{printf sep "'\''" $1 "'\''"; sep=","}'
}

expected_show_rows() {
    variables | awk -F'|' '{print $1 "|" $3}'
}

expected_scalar_row() {
    variables | awk -F'|' '{
        printf sep "%s", $2
        sep = "|"
    }'
}

scalar_select_list() {
    variables | awk -F'|' '{
        printf sep "@@" $1
        sep = ", "
    }'
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "SET GLOBAL performance_schema_max_digest_sample_age = DEFAULT;" >/dev/null
run_mysql "SET GLOBAL performance_schema_show_processlist = DEFAULT;" >/dev/null

scalar_values=$(run_mysql "SELECT $(scalar_select_list);" | normalize_tsv)
expect_value "default scalar values" "$(expected_scalar_row)" "$scalar_values"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show default rows" "$(expected_show_rows)" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show global rows" "$(expected_show_rows)" "$show_global"

show_session=$(
    run_mysql "SHOW SESSION VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show session rows" "$(expected_show_rows)" "$show_session"

variables | while IFS='|' read -r variable _scalar _show mutation; do
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

    case "$mutation" in
        read_only)
            for scope in "" "SESSION " "LOCAL " "GLOBAL "; do
                expect_error \
                    "$variable read-only SET ${scope:-unscoped}" \
                    1238 \
                    HY000 \
                    "Variable '$variable' is a read only variable" \
                    "SET ${scope}$variable = DEFAULT;"
            done
            ;;
        dynamic_numeric|dynamic_boolean)
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

            case "$mutation" in
                dynamic_numeric)
                    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
                    run_mysql "SET GLOBAL $variable = 60;" >/dev/null
                    run_mysql "SET GLOBAL $variable = 61; SET GLOBAL $variable = DEFAULT;" >/dev/null
                    run_mysql "SET @pfs_dynamic_value = 60; SET GLOBAL $variable = @pfs_dynamic_value;" >/dev/null
                    run_mysql "SET @pfs_dynamic_value = 61; SET GLOBAL $variable = @pfs_dynamic_value; SET GLOBAL $variable = DEFAULT;" >/dev/null

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
                dynamic_boolean)
                    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
                    run_mysql "SET GLOBAL $variable = OFF;" >/dev/null
                    run_mysql "SET GLOBAL $variable = ON; SET GLOBAL $variable = DEFAULT;" >/dev/null
                    run_mysql "SET @pfs_dynamic_value = 0; SET GLOBAL $variable = @pfs_dynamic_value;" >/dev/null
                    run_mysql "SET @pfs_dynamic_value = 1; SET GLOBAL $variable = @pfs_dynamic_value; SET GLOBAL $variable = DEFAULT;" >/dev/null

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
            esac
            ;;
    esac
done

printf '%s\n' "mysql_baseline_performance_schema_system_variables_expectations: ok"
