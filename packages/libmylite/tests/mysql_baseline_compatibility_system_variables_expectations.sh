#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_compatibility_system_variables_expectations: $1" >&2
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
        "SET GLOBAL completion_type = DEFAULT;
         SET SESSION completion_type = DEFAULT;
         SET GLOBAL concurrent_insert = DEFAULT;
         SET GLOBAL cte_max_recursion_depth = DEFAULT;
         SET SESSION cte_max_recursion_depth = DEFAULT;
         SET GLOBAL default_table_encryption = DEFAULT;
         SET SESSION default_table_encryption = DEFAULT;
         SET GLOBAL default_week_format = DEFAULT;
         SET SESSION default_week_format = DEFAULT;
         SET GLOBAL delay_key_write = DEFAULT;
         SET GLOBAL delayed_insert_limit = DEFAULT;
         SET GLOBAL delayed_insert_timeout = DEFAULT;
         SET GLOBAL delayed_queue_size = DEFAULT;
         SET GLOBAL div_precision_increment = DEFAULT;
         SET SESSION div_precision_increment = DEFAULT;
         SET GLOBAL enforce_gtid_consistency = DEFAULT;
         SET GLOBAL eq_range_index_dive_limit = DEFAULT;
         SET SESSION eq_range_index_dive_limit = DEFAULT;
         SET GLOBAL event_scheduler = DEFAULT;
         SET GLOBAL explain_format = DEFAULT;
         SET SESSION explain_format = DEFAULT;
         SET GLOBAL explain_json_format_version = DEFAULT;
         SET SESSION explain_json_format_version = DEFAULT;
         SET GLOBAL flush = DEFAULT;
         SET GLOBAL flush_time = DEFAULT;
         SET GLOBAL ft_boolean_syntax = DEFAULT;" >/dev/null
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

while IFS='|' read -r variable scalar show session_scope warning_message; do
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

    if [ -n "$warning_message" ]; then
        read_warning=$(
            run_mysql \
                "SELECT @@$variable;
                 SHOW WARNINGS LIMIT 1;
                 SELECT @@warning_count;" \
                | normalize_tsv
        )
        expect_value \
            "$variable read warning" \
            "$scalar
Warning|1287|$warning_message
1" \
            "$read_warning"
    fi
done <<EOF
completion_type|NO_CHAIN|NO_CHAIN|yes|
concurrent_insert|AUTO|AUTO|no|
core_file|0|OFF|no|
cte_max_recursion_depth|1000|1000|yes|
default_table_encryption|0|OFF|yes|
default_week_format|0|0|yes|
delay_key_write|ON|ON|no|
delayed_insert_limit|100|100|no|'@@delayed_insert_limit' is deprecated and will be removed in a future release.
delayed_insert_timeout|300|300|no|'@@delayed_insert_timeout' is deprecated and will be removed in a future release.
delayed_queue_size|1000|1000|no|'@@delayed_queue_size' is deprecated and will be removed in a future release.
disabled_storage_engines|||no|
div_precision_increment|4|4|yes|
enforce_gtid_consistency|OFF|OFF|no|
eq_range_index_dive_limit|200|200|yes|
event_scheduler|ON|ON|no|
explain_format|TRADITIONAL|TRADITIONAL|yes|
explain_json_format_version|1|1|yes|
flush|0|OFF|no|
flush_time|0|0|no|
ft_max_word_len|84|84|no|
ft_min_word_len|4|4|no|
ft_query_expansion_limit|20|20|no|
ft_stopword_file|(built-in)|(built-in)|no|
EOF

ft_boolean_scalar=$(run_mysql "SELECT @@ft_boolean_syntax, @@GLOBAL.ft_boolean_syntax;")
expect_value \
    "ft_boolean_syntax scalar/global" \
    "+ -><()~*:\"\"&|${TAB}+ -><()~*:\"\"&|" \
    "$ft_boolean_scalar"
expected_ft_boolean_show='ft_boolean_syntax|+ -><()~*:""&|'
actual_ft_boolean_show=$(run_mysql "SHOW VARIABLES LIKE 'ft_boolean_syntax';" | normalize_tsv)
expect_value "ft_boolean_syntax show" "$expected_ft_boolean_show" "$actual_ft_boolean_show"
actual_ft_boolean_global_show=$(
    run_mysql "SHOW GLOBAL VARIABLES LIKE 'ft_boolean_syntax';" | normalize_tsv
)
expect_value \
    "ft_boolean_syntax show global" \
    "$expected_ft_boolean_show" \
    "$actual_ft_boolean_global_show"
actual_ft_boolean_session_show=$(
    run_mysql "SHOW SESSION VARIABLES LIKE 'ft_boolean_syntax';" | normalize_tsv
)
expect_value \
    "ft_boolean_syntax show session" \
    "$expected_ft_boolean_show" \
    "$actual_ft_boolean_session_show"
expect_error \
    "ft_boolean_syntax session scalar" \
    1238 \
    HY000 \
    "Variable 'ft_boolean_syntax' is a GLOBAL variable" \
    "SELECT @@SESSION.ft_boolean_syntax;"

external_scalar=$(run_mysql "SELECT @@external_user, @@SESSION.external_user;")
expect_value "external_user scalar/session" "NULL${TAB}NULL" "$external_scalar"
expected_external_show='external_user|'
actual_external_show=$(run_mysql "SHOW VARIABLES LIKE 'external_user';" | normalize_tsv)
expect_value "external_user show" "$expected_external_show" "$actual_external_show"
actual_external_session_show=$(
    run_mysql "SHOW SESSION VARIABLES LIKE 'external_user';" | normalize_tsv
)
expect_value "external_user show session" "$expected_external_show" "$actual_external_session_show"
actual_external_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'external_user';")
expect_value "external_user show global" "" "$actual_external_global_show"
expect_error \
    "external_user global scalar" \
    1238 \
    HY000 \
    "Variable 'external_user' is a SESSION variable" \
    "SELECT @@GLOBAL.external_user;"

expect_error \
    "concurrent_insert set global-only" \
    1229 \
    HY000 \
    "Variable 'concurrent_insert' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET concurrent_insert = DEFAULT;"
expect_error \
    "core_file read-only SET" \
    1238 \
    HY000 \
    "Variable 'core_file' is a read only variable" \
    "SET GLOBAL core_file = DEFAULT;"
expect_error \
    "delay_key_write set global-only" \
    1229 \
    HY000 \
    "Variable 'delay_key_write' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET delay_key_write = DEFAULT;"
expect_error \
    "delayed_insert_limit set global-only" \
    1229 \
    HY000 \
    "Variable 'delayed_insert_limit' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET delayed_insert_limit = DEFAULT;"
expect_error \
    "disabled_storage_engines read-only SET" \
    1238 \
    HY000 \
    "Variable 'disabled_storage_engines' is a read only variable" \
    "SET GLOBAL disabled_storage_engines = DEFAULT;"
expect_error \
    "enforce_gtid_consistency set global-only" \
    1229 \
    HY000 \
    "Variable 'enforce_gtid_consistency' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET enforce_gtid_consistency = DEFAULT;"
expect_error \
    "event_scheduler set global-only" \
    1229 \
    HY000 \
    "Variable 'event_scheduler' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET event_scheduler = DEFAULT;"
expect_error \
    "external_user read-only SET" \
    1238 \
    HY000 \
    "Variable 'external_user' is a read only variable" \
    "SET external_user = DEFAULT;"
expect_error \
    "flush set global-only" \
    1229 \
    HY000 \
    "Variable 'flush' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET flush = DEFAULT;"
expect_error \
    "flush_time set global-only" \
    1229 \
    HY000 \
    "Variable 'flush_time' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET flush_time = DEFAULT;"
expect_error \
    "ft_boolean_syntax set global-only" \
    1229 \
    HY000 \
    "Variable 'ft_boolean_syntax' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET ft_boolean_syntax = DEFAULT;"
expect_error \
    "ft_max_word_len read-only SET" \
    1238 \
    HY000 \
    "Variable 'ft_max_word_len' is a read only variable" \
    "SET GLOBAL ft_max_word_len = DEFAULT;"
expect_error \
    "ft_min_word_len read-only SET" \
    1238 \
    HY000 \
    "Variable 'ft_min_word_len' is a read only variable" \
    "SET GLOBAL ft_min_word_len = DEFAULT;"
expect_error \
    "ft_query_expansion_limit read-only SET" \
    1238 \
    HY000 \
    "Variable 'ft_query_expansion_limit' is a read only variable" \
    "SET GLOBAL ft_query_expansion_limit = DEFAULT;"
expect_error \
    "ft_stopword_file read-only SET" \
    1238 \
    HY000 \
    "Variable 'ft_stopword_file' is a read only variable" \
    "SET GLOBAL ft_stopword_file = DEFAULT;"

default_noops=$(
    run_mysql \
        "SET completion_type = DEFAULT;
         SET SESSION completion_type = NO_CHAIN;
         SET GLOBAL completion_type = 'NO_CHAIN';
         SET GLOBAL concurrent_insert = DEFAULT;
         SET GLOBAL concurrent_insert = AUTO;
         SET cte_max_recursion_depth = DEFAULT;
         SET SESSION cte_max_recursion_depth = 1000;
         SET GLOBAL cte_max_recursion_depth = 1000;
         SET default_table_encryption = DEFAULT;
         SET SESSION default_table_encryption = OFF;
         SET GLOBAL default_table_encryption = 0;
         SET default_week_format = DEFAULT;
         SET SESSION default_week_format = 0;
         SET GLOBAL default_week_format = 0;
         SET GLOBAL delay_key_write = DEFAULT;
         SET GLOBAL delay_key_write = ON;
         SET div_precision_increment = DEFAULT;
         SET SESSION div_precision_increment = 4;
         SET GLOBAL div_precision_increment = 4;
         SET GLOBAL enforce_gtid_consistency = DEFAULT;
         SET GLOBAL enforce_gtid_consistency = OFF;
         SET eq_range_index_dive_limit = DEFAULT;
         SET SESSION eq_range_index_dive_limit = 200;
         SET GLOBAL eq_range_index_dive_limit = 200;
         SET GLOBAL event_scheduler = DEFAULT;
         SET GLOBAL event_scheduler = ON;
         SET explain_format = DEFAULT;
         SET SESSION explain_format = TRADITIONAL;
         SET GLOBAL explain_format = 'TRADITIONAL';
         SET explain_json_format_version = DEFAULT;
         SET SESSION explain_json_format_version = 1;
         SET GLOBAL explain_json_format_version = 1;
         SET GLOBAL flush = DEFAULT;
         SET GLOBAL flush = OFF;
         SET GLOBAL flush_time = DEFAULT;
         SET GLOBAL flush_time = 0;
         SET GLOBAL ft_boolean_syntax = DEFAULT;
         SELECT @@completion_type,
                @@GLOBAL.concurrent_insert,
                @@cte_max_recursion_depth,
                @@default_table_encryption,
                @@default_week_format,
                @@GLOBAL.delay_key_write,
                @@div_precision_increment,
                @@GLOBAL.enforce_gtid_consistency,
                @@eq_range_index_dive_limit,
                @@GLOBAL.event_scheduler,
                @@explain_format,
                @@explain_json_format_version,
                @@GLOBAL.flush,
                @@GLOBAL.flush_time,
                @@GLOBAL.ft_boolean_syntax,
                @@warning_count;"
)
expect_value \
    "compatibility default-compatible SET values" \
    "NO_CHAIN${TAB}AUTO${TAB}1000${TAB}0${TAB}0${TAB}ON${TAB}4${TAB}OFF${TAB}200${TAB}ON${TAB}TRADITIONAL${TAB}1${TAB}0${TAB}0${TAB}+ -><()~*:\"\"&|${TAB}0" \
    "$default_noops"

for variable in delayed_insert_limit delayed_insert_timeout delayed_queue_size; do
    warning=$(
        run_mysql \
            "SET GLOBAL $variable = DEFAULT;
             SHOW WARNINGS LIMIT 1;
             SELECT @@warning_count;" \
            | normalize_tsv
    )
    expect_value \
        "$variable default SET warning" \
        "Warning|1287|'@@$variable' is deprecated and will be removed in a future release.
1" \
        "$warning"
done

mutable_session=$(
    run_mysql \
        "SET completion_type = CHAIN;
         SET SESSION cte_max_recursion_depth = 7;
         SET SESSION default_table_encryption = ON;
         SET SESSION default_week_format = 1;
         SET SESSION div_precision_increment = 5;
         SET SESSION eq_range_index_dive_limit = 201;
         SET SESSION explain_format = JSON;
         SET SESSION explain_json_format_version = 2;
         SELECT @@completion_type,
                @@SESSION.cte_max_recursion_depth,
                @@SESSION.default_table_encryption,
                @@SESSION.default_week_format,
                @@SESSION.div_precision_increment,
                @@SESSION.eq_range_index_dive_limit,
                @@SESSION.explain_format,
                @@SESSION.explain_json_format_version,
                @@warning_count;" \
        | normalize_tsv
)
expect_value "MySQL mutable compatibility session values" "CHAIN|7|1|1|5|201|JSON|2|0" "$mutable_session"

mutable_global=$(
    run_mysql \
        "SET GLOBAL completion_type = 'RELEASE';
         SET GLOBAL concurrent_insert = NEVER;
         SET GLOBAL cte_max_recursion_depth = 8;
         SET GLOBAL default_table_encryption = ON;
         SET GLOBAL default_week_format = 2;
         SET GLOBAL delay_key_write = OFF;
         SET GLOBAL delayed_insert_limit = 101;
         SET GLOBAL delayed_insert_timeout = 301;
         SET GLOBAL delayed_queue_size = 1001;
         SET GLOBAL div_precision_increment = 6;
         SET GLOBAL enforce_gtid_consistency = WARN;
         SET GLOBAL eq_range_index_dive_limit = 201;
         SET GLOBAL event_scheduler = OFF;
         SET GLOBAL explain_format = TREE;
         SET GLOBAL explain_json_format_version = 2;
         SET GLOBAL flush_time = 1;
         SELECT @@GLOBAL.completion_type,
                @@GLOBAL.concurrent_insert,
                @@GLOBAL.cte_max_recursion_depth,
                @@GLOBAL.default_table_encryption,
                @@GLOBAL.default_week_format,
                @@GLOBAL.delay_key_write,
                @@GLOBAL.delayed_insert_limit,
                @@GLOBAL.delayed_insert_timeout,
                @@GLOBAL.delayed_queue_size,
                @@GLOBAL.div_precision_increment,
                @@GLOBAL.enforce_gtid_consistency,
                @@GLOBAL.eq_range_index_dive_limit,
                @@GLOBAL.event_scheduler,
                @@GLOBAL.explain_format,
                @@GLOBAL.explain_json_format_version,
                @@GLOBAL.flush_time;" \
        | normalize_tsv
)
expect_value \
    "MySQL mutable compatibility global values" \
    "RELEASE|NEVER|8|1|2|OFF|101|301|1001|6|WARN|201|OFF|TREE|2|1" \
    "$mutable_global"

printf '%s\n' "mysql_baseline_compatibility_system_variables_expectations: ok"
