#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_security_system_variables_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar_values=$(
    run_mysql \
        "SELECT @@require_secure_transport, @@GLOBAL.require_secure_transport,
                @@secure_file_priv, @@GLOBAL.secure_file_priv,
                @@skip_external_locking, @@GLOBAL.skip_external_locking,
                @@skip_name_resolve, @@GLOBAL.skip_name_resolve,
                @@skip_networking, @@GLOBAL.skip_networking,
                @@skip_show_database, @@GLOBAL.skip_show_database,
                @@ssl_fips_mode, @@GLOBAL.ssl_fips_mode,
                @@ssl_session_cache_mode, @@GLOBAL.ssl_session_cache_mode,
                @@ssl_session_cache_timeout, @@GLOBAL.ssl_session_cache_timeout,
                @@thread_handling, @@GLOBAL.thread_handling,
                @@tls_certificates_enforced_validation,
                @@GLOBAL.tls_certificates_enforced_validation,
                @@tls_version, @@GLOBAL.tls_version,
                @@tmpdir, @@GLOBAL.tmpdir;"
)
expect_value \
    "default/global scalar values" \
    "0${TAB}0${TAB}/var/lib/mysql-files/${TAB}/var/lib/mysql-files/${TAB}1${TAB}1${TAB}1${TAB}1${TAB}0${TAB}0${TAB}0${TAB}0${TAB}OFF${TAB}OFF${TAB}1${TAB}1${TAB}300${TAB}300${TAB}one-thread-per-connection${TAB}one-thread-per-connection${TAB}0${TAB}0${TAB}TLSv1.2,TLSv1.3${TAB}TLSv1.2,TLSv1.3${TAB}/tmp${TAB}/tmp" \
    "$scalar_values"

expected_show="require_secure_transport|OFF
secure_file_priv|/var/lib/mysql-files/
skip_external_locking|ON
skip_name_resolve|ON
skip_networking|OFF
skip_show_database|OFF
ssl_fips_mode|OFF
ssl_session_cache_mode|ON
ssl_session_cache_timeout|300
thread_handling|one-thread-per-connection
tls_certificates_enforced_validation|OFF
tls_version|TLSv1.2,TLSv1.3
tmpdir|/tmp"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
           'require_secure_transport','secure_file_priv','skip_external_locking',
           'skip_name_resolve','skip_networking','skip_show_database',
           'ssl_fips_mode','ssl_session_cache_mode','ssl_session_cache_timeout',
           'thread_handling','tls_certificates_enforced_validation',
           'tls_version','tmpdir');" \
        | normalize_tsv
)
expect_value "show session rows" "$expected_show" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
           'require_secure_transport','secure_file_priv','skip_external_locking',
           'skip_name_resolve','skip_networking','skip_show_database',
           'ssl_fips_mode','ssl_session_cache_mode','ssl_session_cache_timeout',
           'thread_handling','tls_certificates_enforced_validation',
           'tls_version','tmpdir');" \
        | normalize_tsv
)
expect_value "show global rows" "$expected_show" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN (
           'require_secure_transport','secure_file_priv','skip_external_locking',
           'skip_name_resolve','skip_networking','skip_show_database',
           'ssl_fips_mode','ssl_session_cache_mode','ssl_session_cache_timeout',
           'thread_handling','tls_certificates_enforced_validation',
           'tls_version','tmpdir');" \
        | normalize_tsv
)
expect_value "show session-scope rows" "$expected_show" "$show_session"

noop_values=$(
    run_mysql \
        "SET GLOBAL require_secure_transport = OFF;
         SET GLOBAL require_secure_transport = DEFAULT;
         SET GLOBAL ssl_session_cache_mode = DEFAULT;
         SET GLOBAL ssl_session_cache_mode = ON;
         SET GLOBAL ssl_session_cache_timeout = DEFAULT;
         SET GLOBAL ssl_session_cache_timeout = 300;
         SET GLOBAL tls_version = DEFAULT;
         SET GLOBAL tls_version = 'TLSv1.2,TLSv1.3';
         SELECT @@GLOBAL.require_secure_transport, @@GLOBAL.ssl_session_cache_mode,
                @@GLOBAL.ssl_session_cache_timeout, @@GLOBAL.tls_version,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "global no-op values" \
    "0${TAB}1${TAB}300${TAB}TLSv1.2,TLSv1.3${TAB}0${TAB}0${TAB}0" \
    "$noop_values"

for variable in \
    require_secure_transport \
    secure_file_priv \
    skip_external_locking \
    skip_name_resolve \
    skip_networking \
    skip_show_database \
    ssl_fips_mode \
    ssl_session_cache_mode \
    ssl_session_cache_timeout \
    thread_handling \
    tls_certificates_enforced_validation \
    tls_version \
    tmpdir
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
    secure_file_priv \
    skip_external_locking \
    skip_name_resolve \
    skip_networking \
    skip_show_database \
    ssl_fips_mode \
    thread_handling \
    tls_certificates_enforced_validation \
    tmpdir
do
    expect_error \
        "set global read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
done

for variable in \
    require_secure_transport \
    ssl_session_cache_mode \
    ssl_session_cache_timeout \
    tls_version
do
    expect_error \
        "set session global-only $variable" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

printf '%s\n' "mysql_baseline_server_security_system_variables_expectations: ok"
