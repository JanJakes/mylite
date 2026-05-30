#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_enterprise_table_absence: $1" >&2
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

enterprise_tables="
audit_log_filter
audit_log_user
firewall_group_allowlist
firewall_groups
firewall_membership
firewall_users
firewall_whitelist
"

for table_name in $enterprise_tables; do
    expect_error \
        "mysql.${table_name} direct read" \
        1146 \
        42S02 \
        "Table 'mysql.${table_name}' doesn't exist" \
        "SELECT COUNT(*) FROM mysql.${table_name};"
    expect_error \
        "mysql.${table_name} show columns" \
        1146 \
        42S02 \
        "Table 'mysql.${table_name}' doesn't exist" \
        "SHOW COLUMNS FROM mysql.${table_name};"
done

expect_error \
    "mysql.audit_log_user describe" \
    1146 \
    42S02 \
    "Table 'mysql.audit_log_user' doesn't exist" \
    "DESC mysql.audit_log_user;"
expect_error \
    "mysql.firewall_groups show index" \
    1146 \
    42S02 \
    "Table 'mysql.firewall_groups' doesn't exist" \
    "SHOW INDEX FROM mysql.firewall_groups;"
expect_error \
    "mysql.firewall_users unqualified direct read" \
    1146 \
    42S02 \
    "Table 'mysql.firewall_users' doesn't exist" \
    "USE mysql; SELECT COUNT(*) FROM firewall_users;"

expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.TABLES" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.COLUMNS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.STATISTICS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.TABLE_CONSTRAINTS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.KEY_COLUMN_USAGE" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('audit_log_filter',
                           'audit_log_user',
                           'firewall_group_allowlist',
                           'firewall_groups',
                           'firewall_membership',
                           'firewall_users',
                           'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from SHOW FULL TABLES" \
    "" \
    "SHOW FULL TABLES FROM mysql
      WHERE Tables_in_mysql IN ('audit_log_filter',
                                'audit_log_user',
                                'firewall_group_allowlist',
                                'firewall_groups',
                                'firewall_membership',
                                'firewall_users',
                                'firewall_whitelist');"
expect_output \
    "Enterprise tables absent from SHOW TABLE STATUS" \
    "" \
    "SHOW TABLE STATUS FROM mysql
      WHERE Name IN ('audit_log_filter',
                     'audit_log_user',
                     'firewall_group_allowlist',
                     'firewall_groups',
                     'firewall_membership',
                     'firewall_users',
                     'firewall_whitelist');"

printf '%s\n' "mysql_baseline_mysql_enterprise_table_absence: ok"
