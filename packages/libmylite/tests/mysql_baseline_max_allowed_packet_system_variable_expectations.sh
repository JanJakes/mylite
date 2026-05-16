#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_max_allowed_packet_system_variable_expectations: $1" >&2
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

restore_default() {
    run_mysql "SET GLOBAL max_allowed_packet = 67108864;" >/dev/null || true
}

trap restore_default EXIT HUP INT TERM
restore_default

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        'SELECT @@max_allowed_packet, @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@LOCAL.max_allowed_packet;'
)
expect_value "scalar all scopes" "67108864${TAB}67108864${TAB}67108864${TAB}67108864" "$scalar"

case_scalar=$(run_mysql 'SELECT @@MAX_ALLOWED_PACKET, @@global.`max_allowed_packet`;')
expect_value "case-insensitive and quoted scalar name" "67108864${TAB}67108864" "$case_scalar"

show_default=$(run_mysql "SHOW VARIABLES LIKE 'max_allowed_packet';" | normalize_tsv)
expect_value "show default" "max_allowed_packet|67108864" "$show_default"

show_session=$(run_mysql "SHOW SESSION VARIABLES LIKE 'max_allowed_packet';" | normalize_tsv)
expect_value "show session" "max_allowed_packet|67108864" "$show_session"

show_local=$(run_mysql "SHOW LOCAL VARIABLES LIKE 'max_allowed_packet';" | normalize_tsv)
expect_value "show local" "max_allowed_packet|67108864" "$show_local"

show_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'max_allowed_packet';" | normalize_tsv)
expect_value "show global" "max_allowed_packet|67108864" "$show_global"

where_rows=$(
    run_mysql \
        "SHOW VARIABLES WHERE Value = '67108864' AND Variable_name IN ('max_allowed_packet','autocommit');" \
        | normalize_tsv
)
expect_value "show variables where value" "max_allowed_packet|67108864" "$where_rows"

show_status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'max_allowed_packet'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "show status" "0${TAB}0${TAB}-1" "$show_status"

expect_error \
    "set default scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET max_allowed_packet = 67108864;"

expect_error \
    "set session scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET SESSION max_allowed_packet = 67108864;"

expect_error \
    "set local scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET LOCAL max_allowed_packet = 67108864;"

expect_error \
    "set system variable default scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET @@max_allowed_packet = 67108864;"

expect_error \
    "set system variable session scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET @@SESSION.max_allowed_packet = 67108864;"

expect_error \
    "set system variable local scope" \
    1621 \
    HY000 \
    "SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value" \
    "SET @@LOCAL.max_allowed_packet = 67108864;"

global_same=$(
    run_mysql \
        "SET GLOBAL max_allowed_packet = 67108864; SELECT @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "global same value" "67108864${TAB}67108864${TAB}0${TAB}0${TAB}0" "$global_same"

global_system_same=$(
    run_mysql \
        "SET @@GLOBAL.max_allowed_packet = 67108864; SELECT @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "global system variable same value" "67108864${TAB}67108864${TAB}0${TAB}0${TAB}0" "$global_system_same"

global_default=$(
    run_mysql \
        "SET @@GLOBAL.max_allowed_packet = DEFAULT; SELECT @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "global default value" "67108864${TAB}67108864${TAB}0${TAB}0${TAB}0" "$global_default"

global_plus=$(
    run_mysql \
        "SET GLOBAL max_allowed_packet = +67108864; SELECT @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "global plus value" "67108864${TAB}67108864${TAB}0${TAB}0${TAB}0" "$global_plus"

rounded=$(
    run_mysql \
        "SET GLOBAL max_allowed_packet = 1025; SELECT @@GLOBAL.max_allowed_packet, @@SESSION.max_allowed_packet, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "upstream mutable global rounds" "1024${TAB}67108864${TAB}2${TAB}0${TAB}0" "$rounded"
restore_default

expect_error \
    "runtime suffix rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'max_allowed_packet'" \
    "SET GLOBAL max_allowed_packet = 16M;"
