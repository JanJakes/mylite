#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_authentication_password_system_variables_expectations: $1" >&2
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
        "SELECT @@authentication_policy, @@GLOBAL.authentication_policy,
                @@caching_sha2_password_auto_generate_rsa_keys,
                @@GLOBAL.caching_sha2_password_auto_generate_rsa_keys,
                @@caching_sha2_password_digest_rounds,
                @@GLOBAL.caching_sha2_password_digest_rounds,
                @@caching_sha2_password_private_key_path,
                @@GLOBAL.caching_sha2_password_private_key_path,
                @@caching_sha2_password_public_key_path,
                @@GLOBAL.caching_sha2_password_public_key_path,
                @@default_password_lifetime, @@GLOBAL.default_password_lifetime,
                @@disconnect_on_expired_password, @@GLOBAL.disconnect_on_expired_password,
                @@mysql_native_password_proxy_users, @@GLOBAL.mysql_native_password_proxy_users,
                @@password_history, @@GLOBAL.password_history,
                @@password_require_current, @@GLOBAL.password_require_current,
                @@password_reuse_interval, @@GLOBAL.password_reuse_interval,
                @@sha256_password_auto_generate_rsa_keys,
                @@GLOBAL.sha256_password_auto_generate_rsa_keys,
                @@sha256_password_private_key_path, @@GLOBAL.sha256_password_private_key_path,
                @@sha256_password_proxy_users, @@GLOBAL.sha256_password_proxy_users,
                @@sha256_password_public_key_path, @@GLOBAL.sha256_password_public_key_path;"
)
expect_value \
    "default/global scalar values" \
    "*,,${TAB}*,,${TAB}1${TAB}1${TAB}5000${TAB}5000${TAB}private_key.pem${TAB}private_key.pem${TAB}public_key.pem${TAB}public_key.pem${TAB}0${TAB}0${TAB}1${TAB}1${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}1${TAB}1${TAB}private_key.pem${TAB}private_key.pem${TAB}0${TAB}0${TAB}public_key.pem${TAB}public_key.pem" \
    "$scalar_values"

expected_show="authentication_policy|*,,
caching_sha2_password_auto_generate_rsa_keys|ON
caching_sha2_password_digest_rounds|5000
caching_sha2_password_private_key_path|private_key.pem
caching_sha2_password_public_key_path|public_key.pem
default_password_lifetime|0
disconnect_on_expired_password|ON
mysql_native_password_proxy_users|OFF
password_history|0
password_require_current|OFF
password_reuse_interval|0
sha256_password_auto_generate_rsa_keys|ON
sha256_password_private_key_path|private_key.pem
sha256_password_proxy_users|OFF
sha256_password_public_key_path|public_key.pem"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
           'authentication_policy',
           'caching_sha2_password_auto_generate_rsa_keys',
           'caching_sha2_password_digest_rounds',
           'caching_sha2_password_private_key_path',
           'caching_sha2_password_public_key_path',
           'default_password_lifetime','disconnect_on_expired_password',
           'mysql_native_password_proxy_users','password_history',
           'password_require_current','password_reuse_interval',
           'sha256_password_auto_generate_rsa_keys',
           'sha256_password_private_key_path','sha256_password_proxy_users',
           'sha256_password_public_key_path');" \
        | normalize_tsv
)
expect_value "show session rows" "$expected_show" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
           'authentication_policy',
           'caching_sha2_password_auto_generate_rsa_keys',
           'caching_sha2_password_digest_rounds',
           'caching_sha2_password_private_key_path',
           'caching_sha2_password_public_key_path',
           'default_password_lifetime','disconnect_on_expired_password',
           'mysql_native_password_proxy_users','password_history',
           'password_require_current','password_reuse_interval',
           'sha256_password_auto_generate_rsa_keys',
           'sha256_password_private_key_path','sha256_password_proxy_users',
           'sha256_password_public_key_path');" \
        | normalize_tsv
)
expect_value "show global rows" "$expected_show" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN (
           'authentication_policy',
           'caching_sha2_password_auto_generate_rsa_keys',
           'caching_sha2_password_digest_rounds',
           'caching_sha2_password_private_key_path',
           'caching_sha2_password_public_key_path',
           'default_password_lifetime','disconnect_on_expired_password',
           'mysql_native_password_proxy_users','password_history',
           'password_require_current','password_reuse_interval',
           'sha256_password_auto_generate_rsa_keys',
           'sha256_password_private_key_path','sha256_password_proxy_users',
           'sha256_password_public_key_path');" \
        | normalize_tsv
)
expect_value "show session-scope rows" "$expected_show" "$show_session"

noop_values=$(
    run_mysql \
         "SET GLOBAL authentication_policy = DEFAULT;
         SET GLOBAL authentication_policy = '*,,';
         SET GLOBAL default_password_lifetime = DEFAULT;
         SET GLOBAL default_password_lifetime = 0;
         SET GLOBAL mysql_native_password_proxy_users = DEFAULT;
         SET GLOBAL mysql_native_password_proxy_users = OFF;
         SET GLOBAL password_history = DEFAULT;
         SET GLOBAL password_history = 0;
         SET GLOBAL password_require_current = DEFAULT;
         SET GLOBAL password_require_current = OFF;
         SET GLOBAL password_reuse_interval = DEFAULT;
         SET GLOBAL password_reuse_interval = 0;
         SET GLOBAL sha256_password_proxy_users = DEFAULT;
         SET GLOBAL sha256_password_proxy_users = OFF;
         SELECT @@GLOBAL.authentication_policy, @@GLOBAL.default_password_lifetime,
                @@GLOBAL.mysql_native_password_proxy_users, @@GLOBAL.password_history,
                @@GLOBAL.password_require_current, @@GLOBAL.password_reuse_interval,
                @@GLOBAL.sha256_password_proxy_users,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "global no-op values" \
    "*,,${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0" \
    "$noop_values"

for variable in \
    authentication_policy \
    caching_sha2_password_auto_generate_rsa_keys \
    caching_sha2_password_digest_rounds \
    caching_sha2_password_private_key_path \
    caching_sha2_password_public_key_path \
    default_password_lifetime \
    disconnect_on_expired_password \
    mysql_native_password_proxy_users \
    password_history \
    password_require_current \
    password_reuse_interval \
    sha256_password_auto_generate_rsa_keys \
    sha256_password_private_key_path \
    sha256_password_proxy_users \
    sha256_password_public_key_path
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
    caching_sha2_password_auto_generate_rsa_keys \
    caching_sha2_password_digest_rounds \
    caching_sha2_password_private_key_path \
    caching_sha2_password_public_key_path \
    disconnect_on_expired_password \
    sha256_password_auto_generate_rsa_keys \
    sha256_password_private_key_path \
    sha256_password_public_key_path
do
    expect_error \
        "set global read-only $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
done

for variable in \
    authentication_policy \
    default_password_lifetime \
    mysql_native_password_proxy_users \
    password_history \
    password_require_current \
    password_reuse_interval \
    sha256_password_proxy_users
do
    expect_error \
        "set session global-only $variable" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

printf '%s\n' "mysql_baseline_authentication_password_system_variables_expectations: ok"
