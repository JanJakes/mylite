#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_identity_binary_log_system_variables_expectations: $1" >&2
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

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

normalize_dynamic_values() {
    sed \
        -e 's#^log_bin_basename|.*#log_bin_basename|<path>#' \
        -e 's#^log_bin_index|.*#log_bin_index|<path>#' \
        -e 's#^server_uuid|.*#server_uuid|<uuid>#'
}

trust_warning="Warning|1287|'@@log_bin_trust_function_creators' is deprecated and will be removed in a future release."

restore_defaults() {
    run_mysql \
        "SET GLOBAL server_id = 1; \
         SET GLOBAL server_id_bits = 32; \
         SET GLOBAL log_bin_trust_function_creators = DEFAULT;" >/dev/null || true
}

trap restore_defaults EXIT HUP INT TERM
restore_defaults

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        'SELECT @@server_id, @@GLOBAL.server_id,
                @@server_id_bits, @@GLOBAL.server_id_bits,
                @@log_bin, @@GLOBAL.log_bin,
                @@log_bin_trust_function_creators,
                @@GLOBAL.log_bin_trust_function_creators;'
)
expect_value \
    "scalar fixed numeric variables" \
    "1${TAB}1${TAB}32${TAB}32${TAB}1${TAB}1${TAB}0${TAB}0" \
    "$scalar"

path_scalar=$(
    run_mysql \
        "SELECT @@log_bin_basename REGEXP '.+', @@log_bin_index REGEXP '.+',
                @@server_uuid REGEXP
                '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$';"
)
expect_value "path and uuid scalar shapes" "1${TAB}1${TAB}1" "$path_scalar"

case_scalar=$(run_mysql 'SELECT @@SERVER_ID, @@global.`server_id_bits`, @@GLOBAL.LOG_BIN;')
expect_value "case-insensitive and quoted scalar names" "1${TAB}32${TAB}1" "$case_scalar"

trust_read_warning=$(
    run_mysql 'SELECT @@log_bin_trust_function_creators; SHOW WARNINGS;' | normalize_tsv
)
expect_value "log_bin_trust scalar warning" "0
$trust_warning" "$trust_read_warning"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN
         ('server_id','server_id_bits','server_uuid','log_bin','log_bin_basename',
          'log_bin_index','log_bin_trust_function_creators');" \
        | normalize_tsv \
        | normalize_dynamic_values
)
expect_value "show default rows" "log_bin|ON
log_bin_basename|<path>
log_bin_index|<path>
log_bin_trust_function_creators|OFF
server_id|1
server_id_bits|32
server_uuid|<uuid>" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN
         ('server_id','server_id_bits','server_uuid','log_bin','log_bin_basename',
          'log_bin_index','log_bin_trust_function_creators');" \
        | normalize_tsv \
        | normalize_dynamic_values
)
expect_value "show global rows" "$show_default" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN
         ('server_id','server_id_bits','server_uuid','log_bin','log_bin_basename',
          'log_bin_index','log_bin_trust_function_creators');" \
        | normalize_tsv \
        | normalize_dynamic_values
)
expect_value "show session rows" "$show_default" "$show_session"

show_like=$(run_mysql "SHOW VARIABLES LIKE 'log\\_bin%';" | normalize_tsv | normalize_dynamic_values)
expect_value "show like log bin rows" "log_bin|ON
log_bin_basename|<path>
log_bin_index|<path>
log_bin_trust_function_creators|OFF" "$show_like"

status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'server_id'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$status"

server_id_noop=$(
    run_mysql "SET GLOBAL server_id = 1; SELECT @@server_id, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "server_id global no-op" "1${TAB}0${TAB}0${TAB}0" "$server_id_noop"

server_id_default=$(
    run_mysql "SET GLOBAL server_id = DEFAULT; SELECT @@server_id, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "server_id global default" "1${TAB}0${TAB}0${TAB}0" "$server_id_default"

server_id_bits_noop=$(
    run_mysql "SET @@GLOBAL.server_id_bits = 32; SELECT @@server_id_bits, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "server_id_bits global no-op" "32${TAB}0${TAB}0${TAB}0" "$server_id_bits_noop"

trust_noop=$(
    run_mysql \
        "SET GLOBAL log_bin_trust_function_creators = OFF;
         SELECT @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "log_bin_trust global off no-op" "1${TAB}0${TAB}0" "$trust_noop"

trust_noop_warning=$(
    run_mysql \
        "SET GLOBAL log_bin_trust_function_creators = OFF;
         SHOW WARNINGS;" \
        | normalize_tsv
)
expect_value "log_bin_trust global off warning" "$trust_warning" "$trust_noop_warning"

trust_default=$(
    run_mysql \
        "SET @@GLOBAL.log_bin_trust_function_creators = DEFAULT;
         SELECT @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "log_bin_trust global default" "1${TAB}0${TAB}0" "$trust_default"

trust_default_warning=$(
    run_mysql \
        "SET @@GLOBAL.log_bin_trust_function_creators = DEFAULT;
         SHOW WARNINGS;" \
        | normalize_tsv
)
expect_value "log_bin_trust global default warning" "$trust_warning" "$trust_default_warning"

for variable in \
    server_id \
    server_id_bits \
    server_uuid \
    log_bin \
    log_bin_basename \
    log_bin_index \
    log_bin_trust_function_creators
do
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

for variable in server_id server_id_bits log_bin_trust_function_creators; do
    expect_error \
        "set default scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET ${variable} = DEFAULT;"

    expect_error \
        "set session scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET SESSION ${variable} = DEFAULT;"
done

for variable in server_uuid log_bin log_bin_basename log_bin_index; do
    expect_error \
        "read-only set ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a read only variable" \
        "SET GLOBAL ${variable} = DEFAULT;"
done

restore_defaults

printf '%s\n' "mysql_baseline_server_identity_binary_log_system_variables_expectations: ok"
