#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_variables_where_expectations: $1" >&2
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

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

headers=$(run_mysql_with_headers "SHOW VARIABLES WHERE Variable_name = 'autocommit';" | sed -n '1p')
expect_value "headers" "Variable_name${TAB}Value" "$headers"

eq_case=$(run_mysql "SHOW VARIABLES WHERE Variable_name = 'AUTOCOMMIT';" | normalize_tsv)
expect_value "case-insensitive equality" "autocommit|ON" "$eq_case"

backtick_col=$(run_mysql "SHOW VARIABLES WHERE \`Variable_name\` = 'autocommit';" | normalize_tsv)
expect_value "backticked output column" "autocommit|ON" "$backtick_col"

value_case=$(
    run_mysql \
        "SHOW VARIABLES WHERE Value = 'on' AND Variable_name IN ('autocommit','sql_log_bin','sql_log_off');" \
        | normalize_tsv
)
expect_value "case-insensitive value equality" "autocommit|ON
sql_log_bin|ON" "$value_case"

like_not_in=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name NOT LIKE 'sql\\_%' AND Variable_name IN ('autocommit','sql_mode','sql_log_bin');" \
        | normalize_tsv
)
expect_value "not like with in" "autocommit|ON" "$like_not_in"

or_and=$(
    run_mysql \
        "SHOW VARIABLES WHERE (Variable_name = 'autocommit' OR Variable_name = 'sql_log_bin') AND Value = 'ON';" \
        | normalize_tsv
)
expect_value "or and value filter" "autocommit|ON
sql_log_bin|ON" "$or_and"

null_safe=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name <=> 'autocommit'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | normalize_tsv
)
expect_value "null-safe equality status" "autocommit|ON
0|0|-1" "$null_safe"

not_equal=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name <> 'autocommit' AND Variable_name IN ('autocommit','sql_mode');" \
        | normalize_tsv
)
expect_value "not equal" \
    "sql_mode|ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" \
    "$not_equal"

less_than=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name < 'b' AND Variable_name IN ('autocommit','version');" \
        | normalize_tsv
)
expect_value "less than" "autocommit|ON" "$less_than"

greater_than=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name > 's' AND Variable_name IN ('autocommit','version');" \
        | normalize_tsv
)
expect_value "greater than" "version|8.4.9" "$greater_than"

in_null=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (NULL, 'autocommit'); SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | normalize_tsv
)
expect_value "in with null" "autocommit|ON
0|0|-1" "$in_null"

not_in_null_status=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name NOT IN (NULL, 'autocommit') AND Variable_name IN ('autocommit','sql_mode'); SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "not in with null status" "0${TAB}0${TAB}-1" "$not_in_null_status"

is_null_status=$(
    run_mysql \
        "SHOW VARIABLES WHERE Value IS NULL OR Variable_name IS NULL; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "is null status" "0${TAB}0${TAB}-1" "$is_null_status"

is_not_null=$(run_mysql "SHOW VARIABLES WHERE Variable_name IS NOT NULL AND Variable_name = 'autocommit';" | normalize_tsv)
expect_value "is not null" "autocommit|ON" "$is_not_null"

gtid_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN ('autocommit','gtid_purged','gtid_executed','gtid_owned','gtid_mode');" \
        | normalize_tsv
)
expect_value "default gtid show rows" "autocommit|ON
gtid_executed|
gtid_mode|OFF
gtid_owned|
gtid_purged|" "$gtid_default"

gtid_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN ('sql_log_bin','warning_count','gtid_purged','gtid_executed','gtid_owned','gtid_mode');" \
        | normalize_tsv
)
expect_value "global gtid show rows" "gtid_executed|
gtid_mode|OFF
gtid_owned|
gtid_purged|" "$gtid_global"

gtid_session=$(
    run_mysql \
        "SHOW SESSION VARIABLES WHERE Variable_name IN ('sql_log_bin','warning_count','gtid_purged','gtid_executed','gtid_owned','gtid_mode');" \
        | normalize_tsv
)
expect_value "session gtid show rows" "gtid_executed|
gtid_mode|OFF
gtid_owned|
gtid_purged|
sql_log_bin|ON
warning_count|0" "$gtid_session"

empty_gtid_values=$(
    run_mysql \
        "SHOW VARIABLES WHERE Value = '' AND Variable_name IN ('gtid_purged','gtid_executed','gtid_owned','gtid_mode');" \
        | normalize_tsv
)
expect_value "empty gtid value filter" "gtid_executed|
gtid_owned|
gtid_purged|" "$empty_gtid_values"

gtid_purged_scalar=$(run_mysql "SELECT @@gtid_purged, @@GLOBAL.gtid_purged;" | normalize_tsv)
expect_value "gtid_purged scalar" "|" "$gtid_purged_scalar"

gtid_executed_scalar=$(run_mysql "SELECT @@gtid_executed, @@GLOBAL.gtid_executed;" | normalize_tsv)
expect_value "gtid_executed scalar" "|" "$gtid_executed_scalar"

gtid_mode_scalar=$(run_mysql "SELECT @@gtid_mode, @@GLOBAL.gtid_mode;" | normalize_tsv)
expect_value "gtid_mode scalar" "OFF|OFF" "$gtid_mode_scalar"

gtid_owned_scalar=$(run_mysql "SELECT @@gtid_owned, @@GLOBAL.gtid_owned, @@SESSION.gtid_owned, @@LOCAL.gtid_owned;" | normalize_tsv)
expect_value "gtid_owned scalar" "|||" "$gtid_owned_scalar"

expect_error \
    "gtid_purged session scalar" \
    1238 \
    HY000 \
    "Variable 'gtid_purged' is a GLOBAL variable" \
    "SELECT @@SESSION.gtid_purged;"

expect_error \
    "gtid_executed local scalar" \
    1238 \
    HY000 \
    "Variable 'gtid_executed' is a GLOBAL variable" \
    "SELECT @@LOCAL.gtid_executed;"

expect_error \
    "gtid_mode session scalar" \
    1238 \
    HY000 \
    "Variable 'gtid_mode' is a GLOBAL variable" \
    "SELECT @@SESSION.gtid_mode;"

expect_error \
    "gtid_owned set" \
    1238 \
    HY000 \
    "Variable 'gtid_owned' is a read only variable" \
    "SET SESSION gtid_owned = '';"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW VARIABLES WHERE missing = 'x';"

expect_error \
    "qualified where column" \
    1054 \
    42S22 \
    "Unknown column 'variables.Variable_name' in 'where clause'" \
    "SHOW VARIABLES WHERE variables.Variable_name = 'autocommit';"

expect_error \
    "order by clause" \
    1064 \
    42000 \
    "near 'ORDER BY Variable_name'" \
    "SHOW VARIABLES WHERE Variable_name = 'autocommit' ORDER BY Variable_name;"

expect_error \
    "like and where" \
    1064 \
    42000 \
    "near 'WHERE Variable_name = 'autocommit''" \
    "SHOW VARIABLES LIKE 'a%' WHERE Variable_name = 'autocommit';"

numeric_status=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name = 1; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
set -- $numeric_status
if [ "$1" -le 0 ] || [ "$2" != "0" ] || [ "$3" != "-1" ]; then
    fail "numeric comparison should be warning-producing with clean error count and ROW_COUNT -1, got [$numeric_status]"
fi
