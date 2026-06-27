#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_optional_absence: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

optional_tables="
clone_progress
clone_status
component_scheduler_tasks
firewall_group_allowlist
firewall_groups
firewall_membership
ndb_sync_excluded_objects
ndb_sync_pending_objects
replication_group_communication_information
replication_group_configuration_version
replication_group_member_actions
tp_connections
tp_thread_group_state
tp_thread_group_stats
tp_thread_state
"

for table_name in $optional_tables; do
    expect_error \
        "performance_schema.${table_name} direct read" \
        1146 \
        42S02 \
        "Table 'performance_schema.${table_name}' doesn't exist" \
        "SELECT COUNT(*) FROM performance_schema.${table_name};"
    expect_error \
        "performance_schema.${table_name} show columns" \
        1146 \
        42S02 \
        "Table 'performance_schema.${table_name}' doesn't exist" \
        "SHOW COLUMNS FROM performance_schema.${table_name};"
done

expect_error \
    "performance_schema.clone_status describe" \
    1146 \
    42S02 \
    "Table 'performance_schema.clone_status' doesn't exist" \
    "DESC performance_schema.clone_status;"
expect_error \
    "performance_schema.tp_connections show index" \
    1146 \
    42S02 \
    "Table 'performance_schema.tp_connections' doesn't exist" \
    "SHOW INDEX FROM performance_schema.tp_connections;"
expect_error \
    "performance_schema.clone_progress unqualified direct read" \
    1146 \
    42S02 \
    "Table 'performance_schema.clone_progress' doesn't exist" \
    "USE performance_schema; SELECT COUNT(*) FROM clone_progress;"

expect_output \
    "optional tables absent from INFORMATION_SCHEMA.TABLES" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from INFORMATION_SCHEMA.COLUMNS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from INFORMATION_SCHEMA.STATISTICS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from INFORMATION_SCHEMA.TABLE_CONSTRAINTS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from INFORMATION_SCHEMA.KEY_COLUMN_USAGE" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('clone_progress',
                           'clone_status',
                           'component_scheduler_tasks',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'ndb_sync_excluded_objects',
                           'ndb_sync_pending_objects',
                           'replication_group_communication_information',
                           'replication_group_configuration_version',
                           'replication_group_member_actions',
                           'tp_connections',
                           'tp_thread_group_state',
                           'tp_thread_group_stats',
                           'tp_thread_state');"
expect_output \
    "optional tables absent from SHOW FULL TABLES" \
    "" \
    "SHOW FULL TABLES FROM performance_schema
      WHERE Tables_in_performance_schema IN ('clone_progress',
                                             'clone_status',
                                             'component_scheduler_tasks',
                                             'firewall_group_allowlist',
                                             'firewall_groups',
                                             'firewall_membership',
                                             'ndb_sync_excluded_objects',
                                             'ndb_sync_pending_objects',
                                             'replication_group_communication_information',
                                             'replication_group_configuration_version',
                                             'replication_group_member_actions',
                                             'tp_connections',
                                             'tp_thread_group_state',
                                             'tp_thread_group_stats',
                                             'tp_thread_state');"
expect_output \
    "optional tables absent from SHOW TABLE STATUS" \
    "" \
    "SHOW TABLE STATUS FROM performance_schema
      WHERE Name IN ('clone_progress',
                     'clone_status',
                     'component_scheduler_tasks',
                     'firewall_group_allowlist',
                     'firewall_groups',
                     'firewall_membership',
                     'ndb_sync_excluded_objects',
                     'ndb_sync_pending_objects',
                     'replication_group_communication_information',
                     'replication_group_configuration_version',
                     'replication_group_member_actions',
                     'tp_connections',
                     'tp_thread_group_state',
                     'tp_thread_group_stats',
                     'tp_thread_state');"

printf '%s\n' "mysql_baseline_performance_schema_optional_absence: ok"
