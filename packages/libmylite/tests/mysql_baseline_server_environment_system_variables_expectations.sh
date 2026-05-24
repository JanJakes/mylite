#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_environment_system_variables_expectations: $1" >&2
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

normalize_environment_values() {
    sed \
        -e 's#^basedir|.*#basedir|<basedir>#' \
        -e 's#^datadir|.*#datadir|<datadir>#' \
        -e 's#^hostname|.*#hostname|<hostname>#' \
        -e 's#^pid_file|.*#pid_file|<pid_file>#' \
        -e 's#^plugin_dir|.*#plugin_dir|<plugin_dir>#' \
        -e 's#^socket|.*#socket|<socket>#'
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        "SELECT @@basedir REGEXP '.+', @@GLOBAL.basedir REGEXP '.+',
                @@datadir REGEXP '.+', @@hostname REGEXP '.+',
                @@license, @@pid_file REGEXP '.+', @@plugin_dir REGEXP '.+',
                @@port, @@GLOBAL.port, HEX(@@port), @@socket REGEXP '.+';"
)
expect_value \
    "scalar values and shapes" \
    "1${TAB}1${TAB}1${TAB}1${TAB}GPL${TAB}1${TAB}1${TAB}3306${TAB}3306${TAB}CEA${TAB}1" \
    "$scalar"

case_scalar=$(run_mysql 'SELECT @@BASEDIR REGEXP ".+", @@global.`dataDir` REGEXP ".+", @@`HOSTNAME` REGEXP ".+", @@GLOBAL.PORT, @@`license`;')
expect_value "case-insensitive and quoted scalar names" "1${TAB}1${TAB}1${TAB}3306${TAB}GPL" "$case_scalar"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN
         ('basedir','datadir','hostname','license','pid_file','plugin_dir','port','socket');" \
        | normalize_tsv \
        | normalize_environment_values
)
expect_value "show default rows" "basedir|<basedir>
datadir|<datadir>
hostname|<hostname>
license|GPL
pid_file|<pid_file>
plugin_dir|<plugin_dir>
port|3306
socket|<socket>" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN
         ('basedir','datadir','hostname','license','pid_file','plugin_dir','port','socket');" \
        | normalize_tsv \
        | normalize_environment_values
)
expect_value "show global rows" "$show_default" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN
         ('basedir','datadir','hostname','license','pid_file','plugin_dir','port','socket');" \
        | normalize_tsv \
        | normalize_environment_values
)
expect_value "show session rows" "$show_default" "$show_session"

show_dir=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name LIKE '%dir' AND
         Variable_name IN ('basedir','datadir','plugin_dir');" \
        | normalize_tsv \
        | normalize_environment_values
)
expect_value "show dir filter rows" "basedir|<basedir>
datadir|<datadir>
plugin_dir|<plugin_dir>" "$show_dir"

show_socket=$(run_mysql "SHOW VARIABLES LIKE 'socket';" | normalize_tsv | normalize_environment_values)
expect_value "show socket like row" "socket|<socket>" "$show_socket"

status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'port'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$status"

for variable in basedir datadir hostname license pid_file plugin_dir port socket; do
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

    expect_error \
        "set default scope ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a read only variable" \
        "SET ${variable} = DEFAULT;"

    expect_error \
        "set global scope ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a read only variable" \
        "SET GLOBAL ${variable} = DEFAULT;"

    expect_error \
        "set session scope ${variable}" \
        1238 \
        HY000 \
        "Variable '${variable}' is a read only variable" \
        "SET SESSION ${variable} = DEFAULT;"
done

expect_error \
    "direct session port set" \
    1238 \
    HY000 \
    "Variable 'port' is a read only variable" \
    "SET @@SESSION.port = 3306;"

expect_error \
    "direct global license set" \
    1238 \
    HY000 \
    "Variable 'license' is a read only variable" \
    "SET @@GLOBAL.license = 'GPL';"

printf '%s\n' "mysql_baseline_server_environment_system_variables_expectations: ok"
