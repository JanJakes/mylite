#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_read_only_system_variables_expectations: $1" >&2
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
    output=$(run_mysql "SET GLOBAL super_read_only = OFF; SET GLOBAL read_only = OFF; $sql" "$@" 2>&1)
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql 'SET GLOBAL super_read_only = OFF; SET GLOBAL read_only = OFF;'

scalar=$(
    run_mysql \
        'SELECT @@read_only, @@GLOBAL.read_only, @@super_read_only, @@GLOBAL.super_read_only, @@innodb_read_only, @@GLOBAL.innodb_read_only;'
)
expect_value "scalar default and global" "0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0" "$scalar"

case_scalar=$(
    run_mysql \
        'SELECT @@READ_ONLY, @@global.`read_only`, @@Super_Read_Only, @@GLOBAL.`super_read_only`, @@INNODB_READ_ONLY;'
)
expect_value "case-insensitive and quoted scalar names" "0${TAB}0${TAB}0${TAB}0${TAB}0" "$case_scalar"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN ('read_only','super_read_only','innodb_read_only');" \
        | normalize_tsv
)
expect_value "show default" "innodb_read_only|OFF
read_only|OFF
super_read_only|OFF" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN ('read_only','super_read_only','innodb_read_only');" \
        | normalize_tsv
)
expect_value "show global" "innodb_read_only|OFF
read_only|OFF
super_read_only|OFF" "$show_global"

show_like=$(run_mysql "SHOW VARIABLES LIKE 'super\\_read\\_only';" | normalize_tsv)
expect_value "show like" "super_read_only|OFF" "$show_like"

show_status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'read_only'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$show_status"

read_only_noop=$(
    run_mysql \
        "SET GLOBAL read_only = OFF; SELECT @@read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "read_only global off no-op" "0${TAB}0${TAB}0${TAB}0" "$read_only_noop"

read_only_system_noop=$(
    run_mysql \
        "SET @@GLOBAL.read_only = 0; SELECT @@read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "read_only system global zero no-op" "0${TAB}0${TAB}0${TAB}0" "$read_only_system_noop"

read_only_default=$(
    run_mysql \
        "SET GLOBAL read_only = DEFAULT; SELECT @@read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "read_only global default" "0${TAB}0${TAB}0${TAB}0" "$read_only_default"

super_read_only_noop=$(
    run_mysql \
        "SET GLOBAL super_read_only = OFF; SELECT @@super_read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "super_read_only global off no-op" "0${TAB}0${TAB}0${TAB}0" "$super_read_only_noop"

super_read_only_system_noop=$(
    run_mysql \
        "SET @@GLOBAL.super_read_only = 0; SELECT @@super_read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "super_read_only system global zero no-op" "0${TAB}0${TAB}0${TAB}0" "$super_read_only_system_noop"

super_read_only_default=$(
    run_mysql \
        "SET GLOBAL super_read_only = DEFAULT; SELECT @@super_read_only, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "super_read_only global default" "0${TAB}0${TAB}0${TAB}0" "$super_read_only_default"

for variable in read_only super_read_only innodb_read_only; do
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

for variable in read_only super_read_only; do
    expect_error \
        "set default scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET ${variable} = 0;"

    expect_error \
        "set session scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET SESSION ${variable} = 0;"

    expect_error \
        "set local scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET LOCAL ${variable} = 0;"

    expect_error \
        "set system default scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET @@${variable} = 0;"

    expect_error \
        "set system session scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET @@SESSION.${variable} = 0;"

    expect_error \
        "set system local scope ${variable}" \
        1229 \
        HY000 \
        "Variable '${variable}' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET @@LOCAL.${variable} = 0;"
done

for sql in \
    'SET innodb_read_only = 0;' \
    'SET SESSION innodb_read_only = 0;' \
    'SET LOCAL innodb_read_only = 0;' \
    'SET GLOBAL innodb_read_only = 0;' \
    'SET @@innodb_read_only = 0;' \
    'SET @@SESSION.innodb_read_only = 0;' \
    'SET @@LOCAL.innodb_read_only = 0;' \
    'SET @@GLOBAL.innodb_read_only = 0;'
do
    expect_error \
        "set innodb_read_only read-only ${sql}" \
        1238 \
        HY000 \
        "Variable 'innodb_read_only' is a read only variable" \
        "$sql"
done

run_mysql 'SET GLOBAL super_read_only = OFF; SET GLOBAL read_only = OFF;'
