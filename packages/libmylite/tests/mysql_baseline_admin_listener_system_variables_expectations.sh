#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_admin_listener_system_variables_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

reset_defaults() {
    run_mysql \
        "SET GLOBAL admin_ssl_ca = DEFAULT;
         SET GLOBAL admin_ssl_capath = DEFAULT;
         SET GLOBAL admin_ssl_cert = DEFAULT;
         SET GLOBAL admin_ssl_cipher = DEFAULT;
         SET GLOBAL admin_ssl_crl = DEFAULT;
         SET GLOBAL admin_ssl_crlpath = DEFAULT;
         SET GLOBAL admin_ssl_key = DEFAULT;
         SET GLOBAL admin_tls_ciphersuites = DEFAULT;
         SET GLOBAL admin_tls_version = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

scalar_values=$(
    run_mysql \
        "SELECT @@admin_address, @@GLOBAL.admin_address,
                @@admin_port, @@GLOBAL.admin_port,
                @@admin_ssl_ca, @@GLOBAL.admin_ssl_ca,
                @@admin_ssl_capath, @@GLOBAL.admin_ssl_capath,
                @@admin_ssl_cert, @@GLOBAL.admin_ssl_cert,
                @@admin_ssl_cipher, @@GLOBAL.admin_ssl_cipher,
                @@admin_ssl_crl, @@GLOBAL.admin_ssl_crl,
                @@admin_ssl_crlpath, @@GLOBAL.admin_ssl_crlpath,
                @@admin_ssl_key, @@GLOBAL.admin_ssl_key,
                @@admin_tls_ciphersuites, @@GLOBAL.admin_tls_ciphersuites,
                @@admin_tls_version, @@GLOBAL.admin_tls_version,
                @@create_admin_listener_thread, @@GLOBAL.create_admin_listener_thread;"
)
expect_value \
    "default/global scalar values" \
    "NULL${TAB}NULL${TAB}33062${TAB}33062${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}TLSv1.2,TLSv1.3${TAB}TLSv1.2,TLSv1.3${TAB}0${TAB}0" \
    "$scalar_values"

expected_show="admin_address|
admin_port|33062
admin_ssl_ca|
admin_ssl_capath|
admin_ssl_cert|
admin_ssl_cipher|
admin_ssl_crl|
admin_ssl_crlpath|
admin_ssl_key|
admin_tls_ciphersuites|
admin_tls_version|TLSv1.2,TLSv1.3
create_admin_listener_thread|OFF"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
           'admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',
           'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl','admin_ssl_crlpath',
           'admin_ssl_key','admin_tls_ciphersuites','admin_tls_version',
           'create_admin_listener_thread');" \
        | normalize_tsv
)
expect_value "show default rows" "$expected_show" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
           'admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',
           'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl','admin_ssl_crlpath',
           'admin_ssl_key','admin_tls_ciphersuites','admin_tls_version',
           'create_admin_listener_thread');" \
        | normalize_tsv
)
expect_value "show global rows" "$expected_show" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN (
           'admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',
           'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl','admin_ssl_crlpath',
           'admin_ssl_key','admin_tls_ciphersuites','admin_tls_version',
           'create_admin_listener_thread');" \
        | normalize_tsv
)
expect_value "show session rows" "$expected_show" "$show_session"

noop_values=$(
    run_mysql \
         "SET GLOBAL admin_ssl_ca = DEFAULT;
         SET GLOBAL admin_ssl_capath = DEFAULT;
         SET GLOBAL admin_ssl_cert = DEFAULT;
         SET GLOBAL admin_ssl_cipher = DEFAULT;
         SET GLOBAL admin_ssl_crl = DEFAULT;
         SET GLOBAL admin_ssl_crlpath = DEFAULT;
         SET GLOBAL admin_ssl_key = DEFAULT;
         SET GLOBAL admin_tls_ciphersuites = DEFAULT;
         SET GLOBAL admin_tls_version = DEFAULT;
         SELECT @@GLOBAL.admin_ssl_ca, @@GLOBAL.admin_ssl_capath,
                @@GLOBAL.admin_ssl_cert, @@GLOBAL.admin_ssl_cipher,
                @@GLOBAL.admin_ssl_crl, @@GLOBAL.admin_ssl_crlpath,
                @@GLOBAL.admin_ssl_key, @@GLOBAL.admin_tls_ciphersuites,
                @@GLOBAL.admin_tls_version,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "global no-op values" \
    "NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}TLSv1.2,TLSv1.3${TAB}0${TAB}0${TAB}0" \
    "$noop_values"

for variable in \
    admin_address \
    admin_port \
    admin_ssl_ca \
    admin_ssl_capath \
    admin_ssl_cert \
    admin_ssl_cipher \
    admin_ssl_crl \
    admin_ssl_crlpath \
    admin_ssl_key \
    admin_tls_ciphersuites \
    admin_tls_version \
    create_admin_listener_thread
do
    expect_error \
        "scalar session $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "scalar local $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@LOCAL.$variable;"
done

for variable in \
    admin_ssl_ca \
    admin_ssl_capath \
    admin_ssl_cert \
    admin_ssl_cipher \
    admin_ssl_crl \
    admin_ssl_crlpath \
    admin_ssl_key \
    admin_tls_ciphersuites \
    admin_tls_version
do
    expect_error \
        "set session global-only $variable" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

for variable in \
    admin_address \
    admin_port \
    create_admin_listener_thread
do
    expect_error \
        "set global read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
    expect_error \
        "set read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET $variable = DEFAULT;"
done

reset_defaults

printf '%s\n' "mysql_baseline_admin_listener_system_variables_expectations: ok"
