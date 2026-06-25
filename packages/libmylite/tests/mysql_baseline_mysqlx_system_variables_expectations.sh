#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_mysqlx_system_variables_expectations: $1" >&2
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
        "SET GLOBAL mysqlx_compression_algorithms = DEFAULT;
         SET GLOBAL mysqlx_connect_timeout = DEFAULT;
         SET GLOBAL mysqlx_deflate_default_compression_level = DEFAULT;
         SET GLOBAL mysqlx_deflate_max_client_compression_level = DEFAULT;
         SET GLOBAL mysqlx_document_id_unique_prefix = DEFAULT;
         SET GLOBAL mysqlx_enable_hello_notice = DEFAULT;
         SET GLOBAL mysqlx_idle_worker_thread_timeout = DEFAULT;
         SET GLOBAL mysqlx_interactive_timeout = DEFAULT;
         SET GLOBAL mysqlx_lz4_default_compression_level = DEFAULT;
         SET GLOBAL mysqlx_lz4_max_client_compression_level = DEFAULT;
         SET GLOBAL mysqlx_max_allowed_packet = DEFAULT;
         SET GLOBAL mysqlx_max_connections = DEFAULT;
         SET GLOBAL mysqlx_min_worker_threads = DEFAULT;
         SET GLOBAL mysqlx_read_timeout = DEFAULT;
         SET GLOBAL mysqlx_wait_timeout = DEFAULT;
         SET GLOBAL mysqlx_write_timeout = DEFAULT;
         SET GLOBAL mysqlx_zstd_default_compression_level = DEFAULT;
         SET GLOBAL mysqlx_zstd_max_client_compression_level = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

while IFS='|' read -r variable scalar show_value scope mutability; do
    [ -n "$variable" ] || continue

    expected_scalar="${scalar}${TAB}${scalar}"
    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;")
    expect_value "$variable scalar/global" "$expected_scalar" "$actual_scalar"

    expected_show="$variable|$show_value"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"

    if [ "$scope" = "global" ]; then
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
    else
        actual_session=$(run_mysql "SELECT @@SESSION.$variable, @@LOCAL.$variable;")
        expect_value "$variable session/local" "$expected_scalar" "$actual_session"
    fi

    if [ "$mutability" = "readonly" ]; then
        expect_error \
            "$variable set readonly" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET $variable = DEFAULT;"
        expect_error \
            "$variable set global readonly" \
            1238 \
            HY000 \
            "Variable '$variable' is a read only variable" \
            "SET GLOBAL $variable = DEFAULT;"
    elif [ "$mutability" = "fixedglobal" ]; then
        expect_error \
            "$variable set global-only" \
            1229 \
            HY000 \
            "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
            "SET $variable = DEFAULT;"
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    else
        actual_session_changed=$(
            run_mysql \
                "SET SESSION $variable = 31;
                 SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable;
                 SET SESSION $variable = DEFAULT;
                 SET GLOBAL $variable = DEFAULT;"
        )
        expect_value "$variable session changed" "31${TAB}31${TAB}$scalar" "$actual_session_changed"
    fi
done <<'EOF'
mysqlx_bind_address|*|*|global|readonly
mysqlx_compression_algorithms|DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM|DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM|global|fixedglobal
mysqlx_connect_timeout|30|30|global|fixedglobal
mysqlx_deflate_default_compression_level|3|3|global|fixedglobal
mysqlx_deflate_max_client_compression_level|5|5|global|fixedglobal
mysqlx_document_id_unique_prefix|0|0|global|fixedglobal
mysqlx_enable_hello_notice|1|ON|global|fixedglobal
mysqlx_idle_worker_thread_timeout|60|60|global|fixedglobal
mysqlx_interactive_timeout|28800|28800|global|fixedglobal
mysqlx_lz4_default_compression_level|2|2|global|fixedglobal
mysqlx_lz4_max_client_compression_level|8|8|global|fixedglobal
mysqlx_max_allowed_packet|67108864|67108864|global|fixedglobal
mysqlx_max_connections|100|100|global|fixedglobal
mysqlx_min_worker_threads|2|2|global|fixedglobal
mysqlx_port|33060|33060|global|readonly
mysqlx_port_open_timeout|0|0|global|readonly
mysqlx_read_timeout|30|30|both|session
mysqlx_socket|/var/run/mysqld/mysqlx.sock|/var/run/mysqld/mysqlx.sock|global|readonly
mysqlx_ssl_ca|NULL||global|readonly
mysqlx_ssl_capath|NULL||global|readonly
mysqlx_ssl_cert|NULL||global|readonly
mysqlx_ssl_cipher|NULL||global|readonly
mysqlx_ssl_crl|NULL||global|readonly
mysqlx_ssl_crlpath|NULL||global|readonly
mysqlx_ssl_key|NULL||global|readonly
mysqlx_wait_timeout|28800|28800|both|session
mysqlx_write_timeout|60|60|both|session
mysqlx_zstd_default_compression_level|3|3|global|fixedglobal
mysqlx_zstd_max_client_compression_level|11|11|global|fixedglobal
EOF

reset_defaults

printf '%s\n' "mysql_baseline_mysqlx_system_variables_expectations: ok"
