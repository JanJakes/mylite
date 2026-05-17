#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_variables_expectations: $1" >&2
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

headers=$(run_mysql_with_headers "SHOW VARIABLES LIKE 'autocommit';" | sed -n '1p')
expect_value "headers" "Variable_name${TAB}Value" "$headers"

autocommit=$(run_mysql "SHOW VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "default autocommit" "autocommit|ON" "$autocommit"

session_autocommit=$(run_mysql "SHOW SESSION VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "session autocommit" "autocommit|ON" "$session_autocommit"

local_autocommit=$(run_mysql "SHOW LOCAL VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "local autocommit" "autocommit|ON" "$local_autocommit"

global_autocommit=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'autocommit';" | normalize_tsv)
expect_value "global autocommit" "autocommit|ON" "$global_autocommit"

supported_session_rows=$(run_mysql "
    SHOW VARIABLES WHERE Variable_name IN (
      'warning_count',
      'error_count',
      'character_set_client',
      'character_set_connection',
      'character_set_results',
      'collation_connection',
      'version',
      'version_comment',
      'character_set_server',
      'collation_server',
      'character_set_database',
      'collation_database',
      'default_storage_engine',
      'character_set_system',
      'character_set_filesystem',
      'autocommit',
      'sql_quote_show_create',
      'foreign_key_checks',
      'lower_case_file_system',
      'lower_case_table_names',
      'max_allowed_packet',
      'transaction_isolation',
      'transaction_read_only',
      'unique_checks',
      'updatable_views_with_limit',
      'sql_auto_is_null',
      'sql_big_selects',
      'sql_generate_invisible_primary_key',
      'sql_safe_updates',
      'sql_warnings',
      'sql_select_limit',
      'sql_notes',
      'sql_buffer_result',
      'sql_log_bin',
      'sql_log_off',
      'sql_mode',
      'sql_require_primary_key',
      'sql_replica_skip_counter',
      'sql_slave_skip_counter'
    );" | normalize_tsv)
expect_value "supported session rows" "autocommit|ON
character_set_client|utf8mb4
character_set_connection|utf8mb4
character_set_database|utf8mb4
character_set_filesystem|binary
character_set_results|utf8mb4
character_set_server|utf8mb4
character_set_system|utf8mb3
collation_connection|utf8mb4_0900_ai_ci
collation_database|utf8mb4_0900_ai_ci
collation_server|utf8mb4_0900_ai_ci
default_storage_engine|InnoDB
error_count|0
foreign_key_checks|ON
lower_case_file_system|OFF
lower_case_table_names|0
max_allowed_packet|67108864
sql_auto_is_null|OFF
sql_big_selects|ON
sql_buffer_result|OFF
sql_generate_invisible_primary_key|OFF
sql_log_bin|ON
sql_log_off|OFF
sql_mode|ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
sql_notes|ON
sql_quote_show_create|ON
sql_replica_skip_counter|0
sql_require_primary_key|OFF
sql_safe_updates|OFF
sql_select_limit|18446744073709551615
sql_slave_skip_counter|0
sql_warnings|OFF
transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF
unique_checks|ON
updatable_views_with_limit|YES
version|8.4.9
version_comment|MySQL Community Server - GPL
warning_count|0" "$supported_session_rows"

supported_global_rows=$(run_mysql "
    SHOW GLOBAL VARIABLES WHERE Variable_name IN (
      'warning_count',
      'error_count',
      'character_set_client',
      'character_set_connection',
      'character_set_results',
      'collation_connection',
      'version',
      'version_comment',
      'character_set_server',
      'collation_server',
      'character_set_database',
      'collation_database',
      'default_storage_engine',
      'character_set_system',
      'character_set_filesystem',
      'autocommit',
      'sql_quote_show_create',
      'foreign_key_checks',
      'lower_case_file_system',
      'lower_case_table_names',
      'max_allowed_packet',
      'transaction_isolation',
      'transaction_read_only',
      'unique_checks',
      'updatable_views_with_limit',
      'sql_auto_is_null',
      'sql_big_selects',
      'sql_generate_invisible_primary_key',
      'sql_safe_updates',
      'sql_warnings',
      'sql_select_limit',
      'sql_notes',
      'sql_buffer_result',
      'sql_log_bin',
      'sql_log_off',
      'sql_mode',
      'sql_require_primary_key',
      'sql_replica_skip_counter',
      'sql_slave_skip_counter'
    );" | normalize_tsv)
expect_value "supported global rows" "autocommit|ON
character_set_client|utf8mb4
character_set_connection|utf8mb4
character_set_database|utf8mb4
character_set_filesystem|binary
character_set_results|utf8mb4
character_set_server|utf8mb4
character_set_system|utf8mb3
collation_connection|utf8mb4_0900_ai_ci
collation_database|utf8mb4_0900_ai_ci
collation_server|utf8mb4_0900_ai_ci
default_storage_engine|InnoDB
foreign_key_checks|ON
lower_case_file_system|OFF
lower_case_table_names|0
max_allowed_packet|67108864
sql_auto_is_null|OFF
sql_big_selects|ON
sql_buffer_result|OFF
sql_generate_invisible_primary_key|OFF
sql_log_off|OFF
sql_mode|ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
sql_notes|ON
sql_quote_show_create|ON
sql_replica_skip_counter|0
sql_require_primary_key|OFF
sql_safe_updates|OFF
sql_select_limit|18446744073709551615
sql_slave_skip_counter|0
sql_warnings|OFF
transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF
unique_checks|ON
updatable_views_with_limit|YES
version|8.4.9
version_comment|MySQL Community Server - GPL" "$supported_global_rows"

sql_log_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'sql_log_bin';")
expect_value "session-only sql_log_bin omitted globally" "" "$sql_log_global"

warning_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'warning_count';")
expect_value "session-only warning_count omitted globally" "" "$warning_global"

character_set_system_session=$(
    run_mysql "SHOW SESSION VARIABLES LIKE 'character_set_system';" | normalize_tsv
)
expect_value "global variable visible through session show" \
    "character_set_system|utf8mb3" \
    "$character_set_system_session"

log_pattern=$(run_mysql "SHOW VARIABLES LIKE 'sql\\_log\\_%';" | normalize_tsv)
expect_value "escaped underscore like" "sql_log_bin|ON
sql_log_off|OFF" "$log_pattern"

upper_pattern=$(run_mysql "SHOW VARIABLES LIKE 'SQL\\_LOG\\_%';" | normalize_tsv)
expect_value "case-insensitive like" "sql_log_bin|ON
sql_log_off|OFF" "$upper_pattern"

no_match_status=$(
    run_mysql "SHOW VARIABLES LIKE 'missing%'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "no-match status" "0${TAB}0${TAB}-1" "$no_match_status"

match_status=$(
    run_mysql "SHOW VARIABLES LIKE 'autocommit'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "match status" "0${TAB}0${TAB}-1" "$match_status"

deprecation_status=$(
    run_mysql \
        "SHOW VARIABLES LIKE 'sql_slave_skip_counter'; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1
)
expect_value "deprecated variable show status" "0${TAB}0${TAB}-1" "$deprecation_status"

where_output=$(run_mysql "SHOW VARIABLES WHERE Variable_name = 'autocommit';" | normalize_tsv)
expect_value "mysql where accepted" "autocommit|ON" "$where_output"

expect_error \
    "numeric like" \
    1064 \
    42000 \
    "near '1'" \
    "SHOW VARIABLES LIKE 1;"

expect_error \
    "null like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "SHOW VARIABLES LIKE NULL;"

expect_error \
    "national like" \
    1064 \
    42000 \
    "near 'N'autocommit''" \
    "SHOW VARIABLES LIKE N'autocommit';"

expect_error \
    "introducer like" \
    1064 \
    42000 \
    "near '_utf8mb4'autocommit''" \
    "SHOW VARIABLES LIKE _utf8mb4'autocommit';"

expect_error \
    "from clause" \
    1064 \
    42000 \
    "near 'FROM mysql'" \
    "SHOW VARIABLES FROM mysql;"

expect_error \
    "full variables" \
    1064 \
    42000 \
    "near 'VARIABLES'" \
    "SHOW FULL VARIABLES;"

expect_error \
    "limit clause" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW VARIABLES LIKE 'sql_%' LIMIT 1;"
