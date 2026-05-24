#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_server_build_system_variables_expectations: $1" >&2
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

normalize_build_values() {
    sed \
        -e 's#^version_compile_machine|.*#version_compile_machine|<machine>#' \
        -e 's#^version_compile_os|.*#version_compile_os|<os>#' \
        -e 's#^version_compile_zlib|.*#version_compile_zlib|<zlib>#'
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        "SELECT @@protocol_version, @@GLOBAL.protocol_version,
                HEX(@@protocol_version), @@version_compile_machine REGEXP '.+',
                @@version_compile_os REGEXP '.+',
                @@version_compile_zlib REGEXP '.+';"
)
expect_value "scalar values and shapes" "10${TAB}10${TAB}A${TAB}1${TAB}1${TAB}1" "$scalar"

case_scalar=$(
    run_mysql \
        "SELECT @@PROTOCOL_VERSION, @@global.\`version_compile_machine\` REGEXP '.+',
                @@\`VERSION_COMPILE_OS\` REGEXP '.+',
                @@GLOBAL.VERSION_COMPILE_ZLIB REGEXP '.+';"
)
expect_value "case-insensitive and quoted scalar names" "10${TAB}1${TAB}1${TAB}1" "$case_scalar"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN
         ('protocol_version','version_compile_machine',
          'version_compile_os','version_compile_zlib');" \
        | normalize_tsv \
        | normalize_build_values
)
expect_value "show default rows" "protocol_version|10
version_compile_machine|<machine>
version_compile_os|<os>
version_compile_zlib|<zlib>" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN
         ('protocol_version','version_compile_machine',
          'version_compile_os','version_compile_zlib');" \
        | normalize_tsv \
        | normalize_build_values
)
expect_value "show global rows" "$show_default" "$show_global"

show_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN
         ('protocol_version','version_compile_machine',
          'version_compile_os','version_compile_zlib');" \
        | normalize_tsv \
        | normalize_build_values
)
expect_value "show session rows" "$show_default" "$show_session"

show_compile=$(
    run_mysql "SHOW VARIABLES LIKE 'version_compile_%';" \
        | normalize_tsv \
        | normalize_build_values
)
expect_value "show version_compile filter rows" "version_compile_machine|<machine>
version_compile_os|<os>
version_compile_zlib|<zlib>" "$show_compile"

status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'protocol_version'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$status"

for variable in protocol_version version_compile_machine version_compile_os version_compile_zlib; do
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
    "direct session protocol_version set" \
    1238 \
    HY000 \
    "Variable 'protocol_version' is a read only variable" \
    "SET @@SESSION.protocol_version = 10;"

expect_error \
    "direct global version_compile_os set" \
    1238 \
    HY000 \
    "Variable 'version_compile_os' is a read only variable" \
    "SET @@GLOBAL.version_compile_os = 'Linux';"

printf '%s\n' "mysql_baseline_server_build_system_variables_expectations: ok"
