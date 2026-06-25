#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_bootstrap_system_variables_expectations: $1" >&2
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
        "SET GLOBAL activate_all_roles_on_login = DEFAULT;
         SET GLOBAL automatic_sp_privileges = DEFAULT;
         SET GLOBAL block_encryption_mode = DEFAULT;
         SET SESSION block_encryption_mode = DEFAULT;
         SET GLOBAL bulk_insert_buffer_size = DEFAULT;
         SET SESSION bulk_insert_buffer_size = DEFAULT;
         SET GLOBAL check_proxy_users = DEFAULT;" >/dev/null
}

cleanup() {
    status=$?
    trap - EXIT HUP INT TERM
    set +e
    reset_defaults >/dev/null 2>&1
    exit "$status"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults
trap cleanup EXIT HUP INT TERM

while IFS='|' read -r variable scalar show session_scope; do
    [ -n "$variable" ] || continue

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;")
    expect_value "$variable scalar/global" "$scalar${TAB}$scalar" "$actual_scalar"

    expected_show="$variable|$show"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"

    if [ "$session_scope" = "yes" ]; then
        actual_session_scalar=$(run_mysql "SELECT @@SESSION.$variable;")
        expect_value "$variable session scalar" "$scalar" "$actual_session_scalar"
    else
        expect_error \
            "$variable session scalar" \
            1238 \
            HY000 \
            "Variable '$variable' is a GLOBAL variable" \
            "SELECT @@SESSION.$variable;"
    fi
done <<EOF
activate_all_roles_on_login|0|OFF|no
auto_generate_certs|1|ON|no
automatic_sp_privileges|1|ON|no
block_encryption_mode|aes-128-ecb|aes-128-ecb|yes
build_id|66e221b3840955d27f740799b5b2c6eb0baf3283|66e221b3840955d27f740799b5b2c6eb0baf3283|no
bulk_insert_buffer_size|8388608|8388608|yes
character_sets_dir|/usr/share/mysql-8.4/charsets/|/usr/share/mysql-8.4/charsets/|no
check_proxy_users|0|OFF|no
EOF

expect_error \
    "activate_all_roles_on_login set global-only" \
    1229 \
    HY000 \
    "Variable 'activate_all_roles_on_login' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET activate_all_roles_on_login = DEFAULT;"
expect_error \
    "auto_generate_certs read-only SET" \
    1238 \
    HY000 \
    "Variable 'auto_generate_certs' is a read only variable" \
    "SET GLOBAL auto_generate_certs = DEFAULT;"
expect_error \
    "build_id read-only SET" \
    1238 \
    HY000 \
    "Variable 'build_id' is a read only variable" \
    "SET SESSION build_id = DEFAULT;"
expect_error \
    "character_sets_dir read-only SET" \
    1238 \
    HY000 \
    "Variable 'character_sets_dir' is a read only variable" \
    "SET SESSION character_sets_dir = DEFAULT;"
expect_error \
    "check_proxy_users set global-only" \
    1229 \
    HY000 \
    "Variable 'check_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET check_proxy_users = DEFAULT;"

default_noops=$(
    run_mysql \
        "SET GLOBAL activate_all_roles_on_login = DEFAULT;
         SET GLOBAL automatic_sp_privileges = DEFAULT;
         SET SESSION block_encryption_mode = DEFAULT;
         SET SESSION block_encryption_mode = 'aes-128-ecb';
         SET GLOBAL block_encryption_mode = 'aes-128-ecb';
         SET SESSION bulk_insert_buffer_size = DEFAULT;
         SET SESSION bulk_insert_buffer_size = 8388608;
         SET GLOBAL bulk_insert_buffer_size = 8388608;
         SET GLOBAL check_proxy_users = DEFAULT;
         SELECT @@GLOBAL.activate_all_roles_on_login,
                @@GLOBAL.automatic_sp_privileges,
                @@SESSION.block_encryption_mode,
                @@SESSION.bulk_insert_buffer_size,
                @@GLOBAL.check_proxy_users,
                @@warning_count;"
)
expect_value \
    "bootstrap default-compatible SET values" \
    "0${TAB}1${TAB}aes-128-ecb${TAB}8388608${TAB}0${TAB}0" \
    "$default_noops"

mutable_session=$(
    run_mysql \
        "SET SESSION block_encryption_mode = 'aes-256-cbc';
         SET SESSION bulk_insert_buffer_size = 1048576;
         SELECT @@SESSION.block_encryption_mode,
                @@SESSION.bulk_insert_buffer_size,
                @@warning_count;" \
        | normalize_tsv
)
expect_value "MySQL mutable bootstrap session values" "aes-256-cbc|1048576|0" "$mutable_session"

mutable_global=$(
    run_mysql \
        "SET GLOBAL activate_all_roles_on_login = ON;
         SET GLOBAL automatic_sp_privileges = OFF;
         SET GLOBAL check_proxy_users = ON;
         SELECT @@GLOBAL.activate_all_roles_on_login,
                @@GLOBAL.automatic_sp_privileges,
                @@GLOBAL.check_proxy_users,
                @@warning_count;" \
        | normalize_tsv
)
expect_value "MySQL mutable bootstrap global values" "1|0|1|0" "$mutable_global"

printf '%s\n' "mysql_baseline_bootstrap_system_variables_expectations: ok"
