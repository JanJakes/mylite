#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_tls_system_variables_expectations: $1" >&2
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
        "SET GLOBAL ssl_ca = DEFAULT;
         SET GLOBAL ssl_capath = DEFAULT;
         SET GLOBAL ssl_cert = DEFAULT;
         SET GLOBAL ssl_cipher = DEFAULT;
         SET GLOBAL ssl_crl = DEFAULT;
         SET GLOBAL ssl_crlpath = DEFAULT;
         SET GLOBAL ssl_key = DEFAULT;
         SET GLOBAL tls_ciphersuites = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

while IFS='|' read -r variable; do
    [ -n "$variable" ] || continue

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;")
    expect_value "$variable scalar/global" "NULL${TAB}NULL" "$actual_scalar"

    expected_show="$variable|"
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
    expect_error \
        "$variable set global-only" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"

    actual_default=$(
        run_mysql \
            "SET GLOBAL $variable = DEFAULT;
             SELECT @@GLOBAL.$variable, @@warning_count, @@error_count, ROW_COUNT();"
    )
    expect_value "$variable default no-op" "NULL${TAB}0${TAB}0${TAB}0" "$actual_default"
    actual_null=$(
        run_mysql \
            "SET GLOBAL $variable = NULL;
             SELECT @@GLOBAL.$variable, @@warning_count, @@error_count, ROW_COUNT();"
    )
    expect_value "$variable NULL no-op" "NULL${TAB}0${TAB}0${TAB}0" "$actual_null"
done <<'EOF'
ssl_ca
ssl_capath
ssl_cert
ssl_cipher
ssl_crl
ssl_crlpath
ssl_key
tls_ciphersuites
EOF

reset_defaults

printf '%s\n' "mysql_baseline_server_tls_system_variables_expectations: ok"
