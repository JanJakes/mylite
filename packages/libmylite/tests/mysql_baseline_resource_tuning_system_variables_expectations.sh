#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_resource_tuning_system_variables_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

session_track_default="time_zone,autocommit,character_set_client,character_set_results,character_set_connection"

scalar_values=$(
    run_mysql \
        "SELECT @@lc_time_names, @@GLOBAL.lc_time_names, @@SESSION.lc_time_names,
                @@net_buffer_length, @@GLOBAL.net_buffer_length, @@SESSION.net_buffer_length,
                @@preload_buffer_size, @@query_alloc_block_size, @@range_alloc_block_size,
                @@range_optimizer_max_mem_size, @@read_buffer_size, @@read_rnd_buffer_size,
                @@GLOBAL.regexp_stack_limit, @@GLOBAL.regexp_time_limit,
                @@restrict_fk_on_non_standard_key, @@secondary_engine_cost_threshold,
                @@select_into_buffer_size, @@select_into_disk_sync_delay,
                @@session_track_system_variables, @@set_operations_buffer_size,
                @@show_gipk_in_create_table_and_information_schema, @@tmp_table_size,
                @@transaction_alloc_block_size, @@windowing_use_high_precision,
                @@xa_detach_on_prepare;"
)
expect_value \
    "default scalar values" \
    "en_US${TAB}en_US${TAB}en_US${TAB}16384${TAB}16384${TAB}16384${TAB}32768${TAB}8192${TAB}4096${TAB}8388608${TAB}131072${TAB}262144${TAB}8000000${TAB}32${TAB}1${TAB}100000.000000${TAB}131072${TAB}0${TAB}${session_track_default}${TAB}262144${TAB}1${TAB}16777216${TAB}8192${TAB}1${TAB}1" \
    "$scalar_values"

expected_show_rows="lc_time_names|en_US
net_buffer_length|16384
preload_buffer_size|32768
profiling|OFF
profiling_history_size|15
query_alloc_block_size|8192
query_prealloc_size|8192
range_alloc_block_size|4096
range_optimizer_max_mem_size|8388608
read_buffer_size|131072
read_rnd_buffer_size|262144
regexp_stack_limit|8000000
regexp_time_limit|32
restrict_fk_on_non_standard_key|ON
secondary_engine_cost_threshold|100000.000000
select_into_buffer_size|131072
select_into_disk_sync_delay|0
session_track_system_variables|$session_track_default
set_operations_buffer_size|262144
show_gipk_in_create_table_and_information_schema|ON
terminology_use_previous|NONE
tmp_table_size|16777216
transaction_alloc_block_size|8192
transaction_prealloc_size|4096
windowing_use_high_precision|ON
xa_detach_on_prepare|ON"

for scope in "" "GLOBAL" "SESSION"; do
    show_rows=$(
        run_mysql \
            "SHOW $scope VARIABLES WHERE Variable_name IN (
             'lc_time_names','net_buffer_length','preload_buffer_size','profiling',
             'profiling_history_size','query_alloc_block_size','query_prealloc_size',
             'range_alloc_block_size','range_optimizer_max_mem_size','read_buffer_size',
             'read_rnd_buffer_size','regexp_stack_limit','regexp_time_limit',
             'restrict_fk_on_non_standard_key','secondary_engine_cost_threshold',
             'select_into_buffer_size','select_into_disk_sync_delay',
             'session_track_system_variables','set_operations_buffer_size',
             'show_gipk_in_create_table_and_information_schema',
             'tmp_table_size','transaction_alloc_block_size','transaction_prealloc_size',
             'windowing_use_high_precision','xa_detach_on_prepare',
             'terminology_use_previous');" \
            | normalize_tsv
    )
    expect_value "show ${scope:-default} rows" "$expected_show_rows" "$show_rows"
done

mutated_values=$(
    run_mysql \
        "SET SESSION lc_time_names = 'en_GB';
         SET SESSION preload_buffer_size = 65536;
         SET SESSION profiling = ON;
         SET profiling_history_size = 20;
         SET query_alloc_block_size = 16384;
         SET query_prealloc_size = 16384;
         SET range_alloc_block_size = 8192;
         SET range_optimizer_max_mem_size = 123456;
         SET read_buffer_size = 262144;
         SET read_rnd_buffer_size = 524288;
         SET restrict_fk_on_non_standard_key = OFF;
         SET secondary_engine_cost_threshold = 12345.5;
         SET select_into_buffer_size = 262144;
         SET select_into_disk_sync_delay = 1;
         SET session_track_system_variables = 'time_zone,autocommit';
         SET set_operations_buffer_size = 524288;
         SET show_gipk_in_create_table_and_information_schema = OFF;
         SET tmp_table_size = 33554432;
         SET transaction_alloc_block_size = 16384;
         SET transaction_prealloc_size = 8192;
         SET windowing_use_high_precision = OFF;
         SET xa_detach_on_prepare = OFF;
         SET terminology_use_previous = 'BEFORE_8_0_26';
         SELECT @@lc_time_names, @@GLOBAL.lc_time_names, @@SESSION.lc_time_names,
                @@preload_buffer_size, @@query_alloc_block_size, @@range_alloc_block_size,
                @@range_optimizer_max_mem_size, @@read_buffer_size, @@read_rnd_buffer_size,
                @@restrict_fk_on_non_standard_key, @@GLOBAL.secondary_engine_cost_threshold,
                @@secondary_engine_cost_threshold, @@select_into_buffer_size,
                @@select_into_disk_sync_delay, @@session_track_system_variables,
                @@set_operations_buffer_size,
                @@show_gipk_in_create_table_and_information_schema, @@tmp_table_size,
                @@transaction_alloc_block_size, @@windowing_use_high_precision,
                @@xa_detach_on_prepare;"
)
expect_value \
    "mutated session values" \
    "en_GB${TAB}en_US${TAB}en_GB${TAB}65536${TAB}16384${TAB}8192${TAB}123456${TAB}262144${TAB}524288${TAB}0${TAB}100000.000000${TAB}12345.500000${TAB}262144${TAB}1${TAB}time_zone,autocommit${TAB}524288${TAB}0${TAB}33554432${TAB}16384${TAB}0${TAB}0" \
    "$mutated_values"

deprecated_values=$(
    run_mysql \
        "SET SESSION profiling = ON;
         SET profiling_history_size = 20;
         SET query_prealloc_size = 16384;
         SET transaction_prealloc_size = 8192;
         SET terminology_use_previous = 'BEFORE_8_0_26';
         SELECT @@profiling, @@profiling_history_size, @@query_prealloc_size,
                @@transaction_prealloc_size, @@terminology_use_previous;"
)
expect_value \
    "deprecated scalar values" \
    "1${TAB}20${TAB}16384${TAB}8192${TAB}BEFORE_8_0_26" \
    "$deprecated_values"

for warning_case in \
    "profiling|SET SESSION profiling = ON;|Warning|1287|'@@profiling' is deprecated" \
    "profiling_history_size|SET profiling_history_size = 20;|Warning|1287|'@@profiling_history_size' is deprecated" \
    "query_prealloc_size|SET query_prealloc_size = 16384;|Warning|1287|'@@query_prealloc_size' is deprecated" \
    "transaction_prealloc_size|SET transaction_prealloc_size = 8192;|Warning|1287|'@@transaction_prealloc_size' is deprecated" \
    "terminology_use_previous|SET terminology_use_previous = 'BEFORE_8_0_26';|Warning|1287|'@@terminology_use_previous' is deprecated" \
    "restrict_fk_on_non_standard_key|SET restrict_fk_on_non_standard_key = OFF;|Warning|4166|'restrict_fk_on_non_standard_key' is deprecated"
do
    label=${warning_case%%|*}
    rest=${warning_case#*|}
    sql=${rest%%|*}
    rest=${rest#*|}
    expected_warning=$rest
    warning=$(
        run_mysql "$sql SHOW WARNINGS LIMIT 1;" \
            | normalize_tsv
    )
    case "$warning" in
        "$expected_warning"*) ;;
        *) fail "$label SET warning: expected [$expected_warning], got [$warning]" ;;
    esac
done

for warning_case in \
    "profiling|SELECT @@profiling;|0|Warning|1287|'@@profiling' is deprecated" \
    "profiling_history_size|SELECT @@profiling_history_size;|15|Warning|1287|'@@profiling_history_size' is deprecated" \
    "query_prealloc_size|SELECT @@query_prealloc_size;|8192|Warning|1287|'@@query_prealloc_size' is deprecated" \
    "transaction_prealloc_size|SELECT @@transaction_prealloc_size;|4096|Warning|1287|'@@transaction_prealloc_size' is deprecated" \
    "terminology_use_previous|SELECT @@terminology_use_previous;|NONE|Warning|1287|'@@terminology_use_previous' is deprecated"
do
    label=${warning_case%%|*}
    rest=${warning_case#*|}
    sql=${rest%%|*}
    rest=${rest#*|}
    value=${rest%%|*}
    expected_warning=${rest#*|}
    warning=$(
        run_mysql "$sql SHOW WARNINGS LIMIT 1;" \
            | normalize_tsv
    )
    case "$warning" in
        "$value"*"Warning|"*) ;;
        *) fail "$label scalar warning: expected value [$value] and warning, got [$warning]" ;;
    esac
    case "$warning" in
        *"$expected_warning"*) ;;
        *) fail "$label scalar warning: expected [$expected_warning], got [$warning]" ;;
    esac
done

run_mysql \
    "SET GLOBAL lc_time_names = DEFAULT;
     SET GLOBAL preload_buffer_size = DEFAULT;
     SET GLOBAL profiling = DEFAULT;
     SET GLOBAL profiling_history_size = DEFAULT;
     SET GLOBAL query_alloc_block_size = DEFAULT;
     SET GLOBAL query_prealloc_size = DEFAULT;
     SET GLOBAL range_alloc_block_size = DEFAULT;
     SET GLOBAL range_optimizer_max_mem_size = DEFAULT;
     SET GLOBAL read_buffer_size = DEFAULT;
     SET GLOBAL read_rnd_buffer_size = DEFAULT;
     SET GLOBAL regexp_stack_limit = DEFAULT;
     SET GLOBAL regexp_time_limit = DEFAULT;
     SET GLOBAL restrict_fk_on_non_standard_key = DEFAULT;
     SET GLOBAL secondary_engine_cost_threshold = DEFAULT;
     SET GLOBAL select_into_buffer_size = DEFAULT;
     SET GLOBAL select_into_disk_sync_delay = DEFAULT;
     SET GLOBAL session_track_system_variables = DEFAULT;
     SET GLOBAL set_operations_buffer_size = DEFAULT;
     SET GLOBAL show_gipk_in_create_table_and_information_schema = DEFAULT;
     SET GLOBAL tmp_table_size = DEFAULT;
     SET GLOBAL transaction_alloc_block_size = DEFAULT;
     SET GLOBAL transaction_prealloc_size = DEFAULT;
     SET GLOBAL windowing_use_high_precision = DEFAULT;
     SET GLOBAL xa_detach_on_prepare = DEFAULT;
     SET GLOBAL terminology_use_previous = DEFAULT;" >/dev/null

expect_error \
    "regexp_stack_limit session scalar" \
    1238 \
    HY000 \
    "Variable 'regexp_stack_limit' is a GLOBAL variable" \
    "SELECT @@SESSION.regexp_stack_limit;"
expect_error \
    "regexp_time_limit local scalar" \
    1238 \
    HY000 \
    "Variable 'regexp_time_limit' is a GLOBAL variable" \
    "SELECT @@LOCAL.regexp_time_limit;"
expect_error \
    "regexp_stack_limit unscoped SET" \
    1229 \
    HY000 \
    "Variable 'regexp_stack_limit' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET regexp_stack_limit = DEFAULT;"
expect_error \
    "regexp_time_limit session SET" \
    1229 \
    HY000 \
    "Variable 'regexp_time_limit' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET SESSION regexp_time_limit = DEFAULT;"
expect_error \
    "net_buffer_length session SET" \
    1621 \
    HY000 \
    "SESSION variable 'net_buffer_length' is read-only. Use SET GLOBAL to assign the value" \
    "SET SESSION net_buffer_length = DEFAULT;"

printf '%s\n' "mysql_baseline_resource_tuning_system_variables_expectations: ok"
