#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_m_session_limit_system_variables_expectations: $1" >&2
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

variables() {
    cat <<'EOF'
max_delayed_threads|20
max_execution_time|0
max_heap_table_size|16777216
max_insert_delayed_threads|20
max_join_size|18446744073709551615
max_length_for_sort_data|4096
max_points_in_geometry|65536
max_seeks_for_key|18446744073709551615
max_sort_length|1024
max_sp_recursion_depth|0
max_user_connections|0
min_examined_row_limit|0
EOF
}

variable_names_in_clause() {
    variables | awk -F'|' '{printf sep "'\''" $1 "'\''"; sep=","}'
}

expected_show_rows() {
    variables | awk -F'|' '{print $1 "|" $2}'
}

expected_scalar_row() {
    variables | awk -F'|' '{
        printf sep "%s", $2
        sep = "|"
    }'
}

scalar_select_list() {
    variables | awk -F'|' '{
        printf sep "@@" $1
        sep = ", "
    }'
}

reset_defaults() {
    run_mysql \
        "SET GLOBAL max_delayed_threads = DEFAULT;
         SET GLOBAL max_execution_time = DEFAULT;
         SET GLOBAL max_heap_table_size = DEFAULT;
         SET GLOBAL max_insert_delayed_threads = DEFAULT;
         SET GLOBAL max_join_size = DEFAULT;
         SET GLOBAL max_length_for_sort_data = DEFAULT;
         SET GLOBAL max_points_in_geometry = DEFAULT;
         SET GLOBAL max_seeks_for_key = DEFAULT;
         SET GLOBAL max_sort_length = DEFAULT;
         SET GLOBAL max_sp_recursion_depth = DEFAULT;
         SET GLOBAL max_user_connections = DEFAULT;
         SET GLOBAL min_examined_row_limit = DEFAULT;
         SET SESSION max_delayed_threads = DEFAULT;
         SET SESSION max_execution_time = DEFAULT;
         SET SESSION max_heap_table_size = DEFAULT;
         SET SESSION max_insert_delayed_threads = DEFAULT;
         SET SESSION max_join_size = DEFAULT;
         SET SESSION max_length_for_sort_data = DEFAULT;
         SET SESSION max_points_in_geometry = DEFAULT;
         SET SESSION max_seeks_for_key = DEFAULT;
         SET SESSION max_sort_length = DEFAULT;
         SET SESSION max_sp_recursion_depth = DEFAULT;
         SET SESSION min_examined_row_limit = DEFAULT;
         SET SESSION sql_big_selects = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

scalar_values=$(run_mysql "SELECT $(scalar_select_list);" | normalize_tsv)
expect_value "default scalar values" "$(expected_scalar_row)" "$scalar_values"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show default rows" "$(expected_show_rows)" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show global rows" "$(expected_show_rows)" "$show_global"

show_session=$(
    run_mysql "SHOW SESSION VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show session rows" "$(expected_show_rows)" "$show_session"

session_values=$(
    run_mysql \
        "SET SESSION max_execution_time = 11;
         SET SESSION max_heap_table_size = 0;
         SET SESSION max_join_size = 42;
         SET SESSION max_length_for_sort_data = 0;
         SET SESSION max_points_in_geometry = 0;
         SET SESSION max_seeks_for_key = 7;
         SET SESSION max_sort_length = 18446744073709551615;
         SET SESSION max_sp_recursion_depth = 18446744073709551615;
         SET SESSION min_examined_row_limit = 9;
         SELECT @@max_execution_time, @@max_heap_table_size, @@max_join_size,
                @@max_length_for_sort_data, @@max_points_in_geometry,
                @@max_seeks_for_key, @@max_sort_length, @@max_sp_recursion_depth,
                @@min_examined_row_limit, @@sql_big_selects;" \
        | normalize_tsv
)
expect_value \
    "session assignment and clamps" \
    "11|16384|42|4|3|7|8388608|255|9|0" \
    "$session_values"

join_default=$(
    run_mysql \
        "SET SESSION max_join_size = DEFAULT;
         SELECT @@max_join_size, @@sql_big_selects;" \
        | normalize_tsv
)
expect_value "max_join_size default restores sql_big_selects" \
    "18446744073709551615|1" \
    "$join_default"

user_variable_values=$(
    run_mysql \
        "SET @mylite_limit_value = 15;
         SET SESSION max_execution_time = @mylite_limit_value;
         SET SESSION max_heap_table_size = @mylite_limit_value;
         SET SESSION max_points_in_geometry = @mylite_limit_value;
         SET SESSION max_sort_length = @mylite_limit_value;
         SELECT @@max_execution_time, @@max_heap_table_size,
                @@max_points_in_geometry, @@max_sort_length;" \
        | normalize_tsv
)
expect_value "integer user-variable assignments" "15|16384|15|15" "$user_variable_values"

deprecated_warning="is deprecated and will be removed in a future release"
for variable in max_delayed_threads max_insert_delayed_threads max_length_for_sort_data; do
    output=$(
        run_mysql \
            "SET SESSION $variable = DEFAULT;
             SHOW WARNINGS LIMIT 1;
             SELECT @@$variable;
             SHOW WARNINGS LIMIT 1;" \
            | normalize_tsv
    )
    case "$output" in
        *"Warning|1287|'@@$variable' $deprecated_warning"*) ;;
        *) fail "$variable expected deprecation warning, got [$output]" ;;
    esac
done

run_mysql \
    "SET SESSION max_delayed_threads = 0;
     SET SESSION max_insert_delayed_threads = 0;
     SELECT @@max_delayed_threads, @@max_insert_delayed_threads;" \
    | normalize_tsv \
    | {
          read -r delayed_values
          expect_value "delayed thread zero assignments" "0|0" "$delayed_values"
      }

expect_error \
    "max_delayed_threads rejects nonzero" \
    1231 \
    42000 \
    "Variable 'max_delayed_threads' can't be set to the value of '1'" \
    "SET SESSION max_delayed_threads = 1;"
expect_error \
    "max_insert_delayed_threads rejects nonzero" \
    1231 \
    42000 \
    "Variable 'max_insert_delayed_threads' can't be set to the value of '1'" \
    "SET SESSION max_insert_delayed_threads = 1;"
expect_error \
    "max_execution_time string" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'max_execution_time'" \
    "SET SESSION max_execution_time = 'bogus';"
expect_error \
    "max_heap_table_size null" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'max_heap_table_size'" \
    "SET SESSION max_heap_table_size = NULL;"
expect_error \
    "max_user_connections session read only" \
    1621 \
    HY000 \
    "SESSION variable 'max_user_connections' is read-only. Use SET GLOBAL to assign the value" \
    "SET SESSION max_user_connections = DEFAULT;"
expect_error \
    "max_user_connections local read only" \
    1621 \
    HY000 \
    "SESSION variable 'max_user_connections' is read-only. Use SET GLOBAL to assign the value" \
    "SET LOCAL max_user_connections = 0;"

run_mysql \
    "SET GLOBAL max_execution_time = DEFAULT;
     SET GLOBAL max_heap_table_size = DEFAULT;
     SET GLOBAL max_join_size = DEFAULT;
     SET GLOBAL max_user_connections = DEFAULT;
     SET GLOBAL max_user_connections = 1;
     SET GLOBAL max_user_connections = DEFAULT;" >/dev/null

reset_defaults

printf '%s\n' "mysql_baseline_m_session_limit_system_variables_expectations: ok"
