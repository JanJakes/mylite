#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_privileges_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_privileges=$(
    run_mysql_with_headers 'SHOW PRIVILEGES;' \
        | awk -F '\t' 'NR == 1 { print; next } { comment = $3; if (comment == "") comment = "<empty>"; print $1 "\t" $2 "\t" comment }'
)
expected_show_privileges=$(cat <<'EXPECTED'
Privilege	Context	Comment
Alter	Tables	To alter the table
Alter routine	Functions,Procedures	To alter or drop stored functions/procedures
Create	Databases,Tables,Indexes	To create new databases and tables
Create routine	Databases	To use CREATE FUNCTION/PROCEDURE
Create role	Server Admin	To create new roles
Create temporary tables	Databases	To use CREATE TEMPORARY TABLE
Create view	Tables	To create new views
Create user	Server Admin	To create new users
Delete	Tables	To delete existing rows
Drop	Databases,Tables	To drop databases, tables, and views
Drop role	Server Admin	To drop roles
Event	Server Admin	To create, alter, drop and execute events
Execute	Functions,Procedures	To execute stored routines
File	File access on server	To read and write files on the server
Grant option	Databases,Tables,Functions,Procedures	To give to other users those privileges you possess
Index	Tables	To create or drop indexes
Insert	Tables	To insert data into tables
Lock tables	Databases	To use LOCK TABLES (together with SELECT privilege)
Process	Server Admin	To view the plain text of currently executing queries
Proxy	Server Admin	To make proxy user possible
References	Databases,Tables	To have references on tables
Reload	Server Admin	To reload or refresh tables, logs and privileges
Replication client	Server Admin	To ask where the slave or master servers are
Replication slave	Server Admin	To read binary log events from the master
Select	Tables	To retrieve rows from table
Show databases	Server Admin	To see all databases with SHOW DATABASES
Show view	Tables	To see views with SHOW CREATE VIEW
Shutdown	Server Admin	To shut down the server
Super	Server Admin	To use KILL thread, SET GLOBAL, CHANGE REPLICATION SOURCE, etc.
Trigger	Tables	To use triggers
Create tablespace	Server Admin	To create/alter/drop tablespaces
Update	Tables	To update existing rows
Usage	Server Admin	No privileges - allow connect only
AUDIT_ABORT_EXEMPT	Server Admin	<empty>
FIREWALL_EXEMPT	Server Admin	<empty>
OPTIMIZE_LOCAL_TABLE	Server Admin	<empty>
ALLOW_NONEXISTENT_DEFINER	Server Admin	<empty>
SET_ANY_DEFINER	Server Admin	<empty>
SENSITIVE_VARIABLES_OBSERVER	Server Admin	<empty>
AUTHENTICATION_POLICY_ADMIN	Server Admin	<empty>
GROUP_REPLICATION_STREAM	Server Admin	<empty>
FLUSH_PRIVILEGES	Server Admin	<empty>
XA_RECOVER_ADMIN	Server Admin	<empty>
CONNECTION_ADMIN	Server Admin	<empty>
CLONE_ADMIN	Server Admin	<empty>
ENCRYPTION_KEY_ADMIN	Server Admin	<empty>
INNODB_REDO_LOG_ARCHIVE	Server Admin	<empty>
SESSION_VARIABLES_ADMIN	Server Admin	<empty>
APPLICATION_PASSWORD_ADMIN	Server Admin	<empty>
REPLICATION_SLAVE_ADMIN	Server Admin	<empty>
BACKUP_ADMIN	Server Admin	<empty>
GROUP_REPLICATION_ADMIN	Server Admin	<empty>
SYSTEM_VARIABLES_ADMIN	Server Admin	<empty>
BINLOG_ADMIN	Server Admin	<empty>
PERSIST_RO_VARIABLES_ADMIN	Server Admin	<empty>
TRANSACTION_GTID_TAG	Server Admin	<empty>
PASSWORDLESS_USER_ADMIN	Server Admin	<empty>
ROLE_ADMIN	Server Admin	<empty>
INNODB_REDO_LOG_ENABLE	Server Admin	<empty>
RESOURCE_GROUP_USER	Server Admin	<empty>
BINLOG_ENCRYPTION_ADMIN	Server Admin	<empty>
SERVICE_CONNECTION_ADMIN	Server Admin	<empty>
SHOW_ROUTINE	Server Admin	<empty>
RESOURCE_GROUP_ADMIN	Server Admin	<empty>
SYSTEM_USER	Server Admin	<empty>
TABLE_ENCRYPTION_ADMIN	Server Admin	<empty>
TELEMETRY_LOG_ADMIN	Server Admin	<empty>
FLUSH_STATUS	Server Admin	<empty>
REPLICATION_APPLIER	Server Admin	<empty>
FLUSH_OPTIMIZER_COSTS	Server Admin	<empty>
AUDIT_ADMIN	Server Admin	<empty>
FLUSH_USER_RESOURCES	Server Admin	<empty>
FLUSH_TABLES	Server Admin	<empty>
EXPECTED
)
expect_value "SHOW PRIVILEGES rows" "$expected_show_privileges" "$show_privileges"

row_count=$(printf '%s\n' "$show_privileges" | awk 'NR > 1 { count++ } END { print count + 0 }')
expect_value "SHOW PRIVILEGES row count" "73" "$row_count"

show_status=$(run_mysql 'SHOW PRIVILEGES; SELECT @@warning_count, ROW_COUNT();' | tail -n 1)
expect_value "SHOW PRIVILEGES diagnostics" "$(printf '%b' '0\t-1')" "$show_status"

expect_error \
    "SHOW PRIVILEGES LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE 'Select''" \
    "SHOW PRIVILEGES LIKE 'Select';"

expect_error \
    "SHOW PRIVILEGES WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Privilege = 'Select''" \
    "SHOW PRIVILEGES WHERE Privilege = 'Select';"

expect_error \
    "SHOW FULL PRIVILEGES syntax" \
    1064 \
    42000 \
    "near 'PRIVILEGES'" \
    "SHOW FULL PRIVILEGES;"

expect_error \
    "SHOW PRIVILEGES FROM syntax" \
    1064 \
    42000 \
    "near 'FROM mysql'" \
    "SHOW PRIVILEGES FROM mysql;"

expect_error \
    "SHOW PRIVILEGES ORDER BY syntax" \
    1064 \
    42000 \
    "near 'ORDER BY Privilege'" \
    "SHOW PRIVILEGES ORDER BY Privilege;"

expect_error \
    "SHOW PRIVILEGES LIMIT syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "SHOW PRIVILEGES LIMIT 1;"

printf '%s\n' "mysql_baseline_show_privileges_expectations: ok"
