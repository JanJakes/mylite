#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_myisam_system_variables_expectations: $1" >&2
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
        "SET GLOBAL myisam_data_pointer_size = DEFAULT;
         SET GLOBAL myisam_max_sort_file_size = DEFAULT;
         SET GLOBAL myisam_sort_buffer_size = DEFAULT;
         SET GLOBAL myisam_stats_method = DEFAULT;
         SET GLOBAL myisam_use_mmap = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

defaults=$(
    run_mysql \
        "SELECT @@myisam_data_pointer_size, @@GLOBAL.myisam_data_pointer_size,
                @@myisam_max_sort_file_size, @@GLOBAL.myisam_max_sort_file_size,
                @@myisam_mmap_size, @@GLOBAL.myisam_mmap_size,
                @@myisam_recover_options, @@GLOBAL.myisam_recover_options,
                @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size,
                @@SESSION.myisam_sort_buffer_size, @@LOCAL.myisam_sort_buffer_size,
                @@myisam_stats_method, @@GLOBAL.myisam_stats_method,
                @@SESSION.myisam_stats_method, @@LOCAL.myisam_stats_method,
                @@myisam_use_mmap, @@GLOBAL.myisam_use_mmap;" \
        | normalize_tsv
)
expect_value \
    "MyISAM defaults" \
    "6|6|9223372036853727232|9223372036853727232|18446744073709551615|18446744073709551615|OFF|OFF|8388608|8388608|8388608|8388608|nulls_unequal|nulls_unequal|nulls_unequal|nulls_unequal|0|0" \
    "$defaults"

while IFS='|' read -r variable show; do
    [ -n "$variable" ] || continue

    expected_show="$variable|$show"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"
done <<EOF
myisam_data_pointer_size|6
myisam_max_sort_file_size|9223372036853727232
myisam_mmap_size|18446744073709551615
myisam_recover_options|OFF
myisam_sort_buffer_size|8388608
myisam_stats_method|nulls_unequal
myisam_use_mmap|OFF
EOF

for variable in \
    myisam_data_pointer_size \
    myisam_max_sort_file_size \
    myisam_mmap_size \
    myisam_recover_options \
    myisam_use_mmap
do
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
done

for variable in \
    myisam_data_pointer_size \
    myisam_max_sort_file_size \
    myisam_use_mmap
do
    expect_error \
        "$variable set global-only" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
done

expect_error \
    "myisam_mmap_size read-only SET" \
    1238 \
    HY000 \
    "Variable 'myisam_mmap_size' is a read only variable" \
    "SET GLOBAL myisam_mmap_size = DEFAULT;"

expect_error \
    "myisam_recover_options read-only SET" \
    1238 \
    HY000 \
    "Variable 'myisam_recover_options' is a read only variable" \
    "SET GLOBAL myisam_recover_options = DEFAULT;"

global_noops=$(
    run_mysql \
        "SET GLOBAL myisam_data_pointer_size = DEFAULT;
         SET GLOBAL myisam_data_pointer_size = 6;
         SET GLOBAL myisam_max_sort_file_size = DEFAULT;
         SET GLOBAL myisam_max_sort_file_size = 9223372036853727232;
         SET GLOBAL myisam_sort_buffer_size = DEFAULT;
         SET GLOBAL myisam_sort_buffer_size = 8388608;
         SET GLOBAL myisam_stats_method = DEFAULT;
         SET GLOBAL myisam_stats_method = 'nulls_unequal';
         SET GLOBAL myisam_use_mmap = DEFAULT;
         SET GLOBAL myisam_use_mmap = OFF;
         SELECT @@GLOBAL.myisam_data_pointer_size, @@GLOBAL.myisam_max_sort_file_size,
                @@GLOBAL.myisam_sort_buffer_size, @@GLOBAL.myisam_stats_method,
                @@GLOBAL.myisam_use_mmap;" \
        | normalize_tsv
)
expect_value \
    "fixed MyISAM global no-ops" \
    "6|9223372036853727232|8388608|nulls_unequal|0" \
    "$global_noops"

global_mutation=$(
    run_mysql \
        "SET GLOBAL myisam_data_pointer_size = 7;
         SET GLOBAL myisam_sort_buffer_size = 4096;
         SET GLOBAL myisam_stats_method = 'nulls_ignored';
         SET GLOBAL myisam_use_mmap = ON;
         SELECT @@GLOBAL.myisam_data_pointer_size, @@GLOBAL.myisam_sort_buffer_size,
                @@GLOBAL.myisam_stats_method, @@GLOBAL.myisam_use_mmap;" \
        | normalize_tsv
)
expect_value "mysql mutable MyISAM global observation" "7|4096|nulls_ignored|1" "$global_mutation"

reset_defaults

sort_buffer_session=$(
    run_mysql \
        "SET SESSION myisam_sort_buffer_size = 4096;
         SELECT @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size,
                @@SESSION.myisam_sort_buffer_size;
         SET SESSION myisam_sort_buffer_size = DEFAULT;
         SELECT @@myisam_sort_buffer_size, @@warning_count, @@error_count, ROW_COUNT();
         SET myisam_sort_buffer_size = 5000;
         SELECT @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size;
         SET LOCAL myisam_sort_buffer_size = 6000;
         SELECT @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size;
         SET @@myisam_sort_buffer_size = 7000;
         SELECT @@myisam_sort_buffer_size;" \
        | normalize_tsv
)
expect_value \
    "myisam_sort_buffer_size session assignments" \
    "4096|8388608|4096
8388608|0|0|0
5000|8388608
6000|8388608
7000" \
    "$sort_buffer_session"

sort_buffer_clamp=$(
    run_mysql \
        "SET SESSION myisam_sort_buffer_size = 0;
         SHOW WARNINGS LIMIT 1;
         SELECT @@myisam_sort_buffer_size, @@warning_count;
         SET SESSION myisam_sort_buffer_size = -1;
         SHOW WARNINGS LIMIT 1;
         SELECT @@myisam_sort_buffer_size, @@warning_count;
         SET SESSION myisam_sort_buffer_size = FALSE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@myisam_sort_buffer_size, @@warning_count;
         SET SESSION myisam_sort_buffer_size = TRUE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@myisam_sort_buffer_size, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "myisam_sort_buffer_size clamps" \
    "Warning|1292|Truncated incorrect myisam_sort_buffer_size value: '0'
4096|1
Warning|1292|Truncated incorrect myisam_sort_buffer_size value: '-1'
4096|1
Warning|1292|Truncated incorrect myisam_sort_buffer_size value: '0'
4096|1
Warning|1292|Truncated incorrect myisam_sort_buffer_size value: '1'
4096|1" \
    "$sort_buffer_clamp"

expect_error \
    "myisam_sort_buffer_size string" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'myisam_sort_buffer_size'" \
    "SET SESSION myisam_sort_buffer_size = '4096';"

sort_buffer_user_variables=$(
    run_mysql \
        "SET @buf = 8192;
         SET SESSION myisam_sort_buffer_size = @buf;
         SELECT @@myisam_sort_buffer_size, @@warning_count;
         SET @buf = -2;
         SET SESSION myisam_sort_buffer_size = @buf;
         SHOW WARNINGS LIMIT 1;
         SELECT @@myisam_sort_buffer_size, @@warning_count;
         SET SESSION myisam_sort_buffer_size = DEFAULT;" \
        | normalize_tsv
)
expect_value \
    "myisam_sort_buffer_size user variables" \
    "8192|0
Warning|1292|Truncated incorrect myisam_sort_buffer_size value: '-2'
4096|1" \
    "$sort_buffer_user_variables"

expect_error \
    "myisam_sort_buffer_size string user variable" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'myisam_sort_buffer_size'" \
    "SET @buf = '4096'; SET SESSION myisam_sort_buffer_size = @buf;"

stats_method_session=$(
    run_mysql \
        "SET SESSION myisam_stats_method = DEFAULT;
         SELECT @@myisam_stats_method, @@warning_count, @@error_count, ROW_COUNT();
         SET SESSION myisam_stats_method = 'nulls_equal';
         SELECT @@myisam_stats_method, @@GLOBAL.myisam_stats_method,
                @@SESSION.myisam_stats_method;
         SET SESSION myisam_stats_method = nulls_ignored;
         SELECT @@myisam_stats_method, @@GLOBAL.myisam_stats_method;
         SET SESSION myisam_stats_method = 0;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = 1;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = 2;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = TRUE;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = FALSE;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = DEFAULT;" \
        | normalize_tsv
)
expect_value \
    "myisam_stats_method session assignments" \
    "nulls_unequal|0|0|0
nulls_equal|nulls_unequal|nulls_equal
nulls_ignored|nulls_unequal
nulls_unequal
nulls_equal
nulls_ignored
nulls_equal
nulls_unequal" \
    "$stats_method_session"

for value in "'bad'" 3 NULL; do
    expect_error \
        "myisam_stats_method invalid $value" \
        1231 \
        42000 \
        "Variable 'myisam_stats_method' can't be set to the value of" \
        "SET SESSION myisam_stats_method = $value;"
done

stats_method_user_variables=$(
    run_mysql \
        "SET @method = 'nulls_ignored';
         SET SESSION myisam_stats_method = @method;
         SELECT @@myisam_stats_method;
         SET @method = 2;
         SET SESSION myisam_stats_method = @method;
         SELECT @@myisam_stats_method;
         SET SESSION myisam_stats_method = DEFAULT;" \
        | normalize_tsv
)
expect_value \
    "myisam_stats_method user variables" \
    "nulls_ignored
nulls_ignored" \
    "$stats_method_user_variables"

expect_error \
    "myisam_stats_method invalid user variable" \
    1231 \
    42000 \
    "Variable 'myisam_stats_method' can't be set to the value of 'bad'" \
    "SET @method = 'bad'; SET SESSION myisam_stats_method = @method;"

printf '%s\n' "mysql_baseline_myisam_system_variables_expectations: ok"
