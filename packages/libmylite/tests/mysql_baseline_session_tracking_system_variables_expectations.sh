#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_session_tracking_system_variables_expectations: $1" >&2
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

scalar_values=$(
    run_mysql \
        "SELECT @@default_collation_for_utf8mb4, @@GLOBAL.default_collation_for_utf8mb4,
                @@SESSION.default_collation_for_utf8mb4,
                @@end_markers_in_json, @@GLOBAL.end_markers_in_json,
                @@SESSION.end_markers_in_json,
                @@keep_files_on_create, @@GLOBAL.keep_files_on_create,
                @@SESSION.keep_files_on_create,
                @@old_alter_table, @@GLOBAL.old_alter_table,
                @@SESSION.old_alter_table,
                @@print_identified_with_as_hex, @@GLOBAL.print_identified_with_as_hex,
                @@SESSION.print_identified_with_as_hex,
                @@select_into_disk_sync, @@GLOBAL.select_into_disk_sync,
                @@SESSION.select_into_disk_sync,
                @@session_track_gtids, @@GLOBAL.session_track_gtids,
                @@SESSION.session_track_gtids,
                @@session_track_schema, @@GLOBAL.session_track_schema,
                @@SESSION.session_track_schema,
                @@session_track_state_change, @@GLOBAL.session_track_state_change,
                @@SESSION.session_track_state_change,
                @@session_track_transaction_info, @@GLOBAL.session_track_transaction_info,
                @@SESSION.session_track_transaction_info,
                @@show_create_table_verbosity, @@GLOBAL.show_create_table_verbosity,
                @@SESSION.show_create_table_verbosity;"
)
expect_value \
    "default/global/session scalar values" \
    "utf8mb4_0900_ai_ci${TAB}utf8mb4_0900_ai_ci${TAB}utf8mb4_0900_ai_ci${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}0${TAB}OFF${TAB}OFF${TAB}OFF${TAB}1${TAB}1${TAB}1${TAB}0${TAB}0${TAB}0${TAB}OFF${TAB}OFF${TAB}OFF${TAB}0${TAB}0${TAB}0" \
    "$scalar_values"

session_only_values=$(
    run_mysql \
        "SELECT @@require_row_format, @@SESSION.require_row_format,
                @@resultset_metadata, @@SESSION.resultset_metadata,
                @@show_create_table_skip_secondary_engine,
                @@SESSION.show_create_table_skip_secondary_engine,
                @@use_secondary_engine, @@SESSION.use_secondary_engine;"
)
expect_value \
    "session-only scalar values" \
    "0${TAB}0${TAB}FULL${TAB}FULL${TAB}0${TAB}0${TAB}ON${TAB}ON" \
    "$session_only_values"

show_default=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN (
           'default_collation_for_utf8mb4','end_markers_in_json','keep_files_on_create',
           'old_alter_table','print_identified_with_as_hex','require_row_format',
           'resultset_metadata','select_into_disk_sync','session_track_gtids',
           'session_track_schema','session_track_state_change',
           'session_track_transaction_info','show_create_table_skip_secondary_engine',
           'show_create_table_verbosity','use_secondary_engine');" \
        | normalize_tsv
)
expect_value "show session rows" "default_collation_for_utf8mb4|utf8mb4_0900_ai_ci
end_markers_in_json|OFF
keep_files_on_create|OFF
old_alter_table|OFF
print_identified_with_as_hex|OFF
require_row_format|OFF
resultset_metadata|FULL
select_into_disk_sync|OFF
session_track_gtids|OFF
session_track_schema|ON
session_track_state_change|OFF
session_track_transaction_info|OFF
show_create_table_skip_secondary_engine|OFF
show_create_table_verbosity|OFF
use_secondary_engine|ON" "$show_default"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
           'default_collation_for_utf8mb4','end_markers_in_json','keep_files_on_create',
           'old_alter_table','print_identified_with_as_hex','require_row_format',
           'resultset_metadata','select_into_disk_sync','session_track_gtids',
           'session_track_schema','session_track_state_change',
           'session_track_transaction_info','show_create_table_skip_secondary_engine',
           'show_create_table_verbosity','use_secondary_engine');" \
        | normalize_tsv
)
expect_value "show global rows" "default_collation_for_utf8mb4|utf8mb4_0900_ai_ci
end_markers_in_json|OFF
keep_files_on_create|OFF
old_alter_table|OFF
print_identified_with_as_hex|OFF
select_into_disk_sync|OFF
session_track_gtids|OFF
session_track_schema|ON
session_track_state_change|OFF
session_track_transaction_info|OFF
show_create_table_verbosity|OFF" "$show_global"

set_session_values=$(
    run_mysql \
        "SET require_row_format = ON;
         SET resultset_metadata = FULL;
         SET session_track_gtids = OWN_GTID;
         SET session_track_transaction_info = STATE;
         SET use_secondary_engine = FORCED;
         SELECT @@require_row_format, @@resultset_metadata, @@session_track_gtids,
                @@session_track_transaction_info, @@use_secondary_engine,
                @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value \
    "session set readback" \
    "1${TAB}FULL${TAB}OWN_GTID${TAB}STATE${TAB}FORCED${TAB}0${TAB}0${TAB}0" \
    "$set_session_values"

global_noop_values=$(
    run_mysql \
        "SET GLOBAL end_markers_in_json = DEFAULT;
         SET GLOBAL session_track_schema = ON;
         SET GLOBAL session_track_gtids = OFF;
         SELECT @@GLOBAL.end_markers_in_json, @@GLOBAL.session_track_schema,
                @@GLOBAL.session_track_gtids, @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "global no-op values" "0${TAB}1${TAB}OFF${TAB}0${TAB}0${TAB}0" "$global_noop_values"

for variable in require_row_format resultset_metadata show_create_table_skip_secondary_engine use_secondary_engine; do
    expect_error \
        "scalar global $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a SESSION variable" \
        "SELECT @@GLOBAL.$variable;"

    expect_error \
        "set global $variable" \
        1228 \
        HY000 \
        "Variable '$variable' is a SESSION variable and can't be used with SET GLOBAL" \
        "SET GLOBAL $variable = DEFAULT;"
done

printf '%s\n' "mysql_baseline_session_tracking_system_variables_expectations: ok"
