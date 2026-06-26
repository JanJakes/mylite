#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_storage_system_variables_expectations: $1" >&2
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
innodb_checksum_algorithm|crc32|crc32|'crc32'|dynamic_global
innodb_cmp_per_index_enabled|0|OFF|OFF|dynamic_global
innodb_commit_concurrency|0|0|0|dynamic_global
innodb_compression_failure_threshold_pct|5|5|5|dynamic_global
innodb_compression_level|6|6|6|dynamic_global
innodb_compression_pad_pct_max|50|50|50|dynamic_global
innodb_concurrency_tickets|5000|5000|5000|dynamic_global
innodb_data_file_path|ibdata1:12M:autoextend|ibdata1:12M:autoextend||read_only
innodb_data_home_dir|NULL|||read_only
innodb_ddl_buffer_size|1048576|1048576|1048576|dynamic_session
innodb_ddl_threads|4|4|4|dynamic_session
innodb_deadlock_detect|1|ON|ON|dynamic_global
innodb_dedicated_server|0|OFF||read_only
innodb_default_row_format|dynamic|dynamic|'dynamic'|dynamic_global
innodb_directories|NULL|||read_only
innodb_disable_sort_file_cache|0|OFF|OFF|dynamic_global
innodb_doublewrite|ON|ON|'ON'|dynamic_global
innodb_doublewrite_batch_size|0|0||read_only
innodb_doublewrite_dir|NULL|||read_only
innodb_doublewrite_files|2|2||read_only
innodb_doublewrite_pages|128|128||read_only
innodb_extend_and_initialize|1|ON|ON|dynamic_global
innodb_fast_shutdown|1|1|1|dynamic_global
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show _exact mode; do
        [ "$mode" != "read_only" ] || continue
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

    if [ "$mode" = "dynamic_session" ]; then
        actual_session_pair=$(run_mysql "SELECT @@SESSION.$variable, @@LOCAL.$variable;" \
            | normalize_tsv)
        expect_value "$variable session scalar" "$expected_pair" "$actual_session_pair"

        run_mysql "SET $variable = DEFAULT;" >/dev/null
        run_mysql "SET SESSION $variable = DEFAULT;" >/dev/null
        run_mysql "SET LOCAL $variable = DEFAULT;" >/dev/null
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
        run_mysql "SET $variable = $exact;" >/dev/null
        run_mysql "SET SESSION $variable = $exact;" >/dev/null
        run_mysql "SET LOCAL $variable = $exact;" >/dev/null
        run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
        continue
    fi

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
            "$variable read-only local SET" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET LOCAL $variable = DEFAULT;"
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
        expect_error \
            "$variable global-only local SET" \
            1229 \
            HY000 \
            "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
            "SET LOCAL $variable = DEFAULT;"

        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
        run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
    fi
done

printf '%s\n' "mysql_baseline_innodb_storage_system_variables_expectations: ok"
