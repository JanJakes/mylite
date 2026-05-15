#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_privileges_expectations: $1" >&2
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

root_privileges_expected=$(cat <<\EXPECTED
'root'@'%'	def	ALLOW_NONEXISTENT_DEFINER	YES
'root'@'%'	def	ALTER	YES
'root'@'%'	def	ALTER ROUTINE	YES
'root'@'%'	def	APPLICATION_PASSWORD_ADMIN	YES
'root'@'%'	def	AUDIT_ABORT_EXEMPT	YES
'root'@'%'	def	AUDIT_ADMIN	YES
'root'@'%'	def	AUTHENTICATION_POLICY_ADMIN	YES
'root'@'%'	def	BACKUP_ADMIN	YES
'root'@'%'	def	BINLOG_ADMIN	YES
'root'@'%'	def	BINLOG_ENCRYPTION_ADMIN	YES
'root'@'%'	def	CLONE_ADMIN	YES
'root'@'%'	def	CONNECTION_ADMIN	YES
'root'@'%'	def	CREATE	YES
'root'@'%'	def	CREATE ROLE	YES
'root'@'%'	def	CREATE ROUTINE	YES
'root'@'%'	def	CREATE TABLESPACE	YES
'root'@'%'	def	CREATE TEMPORARY TABLES	YES
'root'@'%'	def	CREATE USER	YES
'root'@'%'	def	CREATE VIEW	YES
'root'@'%'	def	DELETE	YES
'root'@'%'	def	DROP	YES
'root'@'%'	def	DROP ROLE	YES
'root'@'%'	def	ENCRYPTION_KEY_ADMIN	YES
'root'@'%'	def	EVENT	YES
'root'@'%'	def	EXECUTE	YES
'root'@'%'	def	FILE	YES
'root'@'%'	def	FIREWALL_EXEMPT	YES
'root'@'%'	def	FLUSH_OPTIMIZER_COSTS	YES
'root'@'%'	def	FLUSH_PRIVILEGES	YES
'root'@'%'	def	FLUSH_STATUS	YES
'root'@'%'	def	FLUSH_TABLES	YES
'root'@'%'	def	FLUSH_USER_RESOURCES	YES
'root'@'%'	def	GROUP_REPLICATION_ADMIN	YES
'root'@'%'	def	GROUP_REPLICATION_STREAM	YES
'root'@'%'	def	INDEX	YES
'root'@'%'	def	INNODB_REDO_LOG_ARCHIVE	YES
'root'@'%'	def	INNODB_REDO_LOG_ENABLE	YES
'root'@'%'	def	INSERT	YES
'root'@'%'	def	LOCK TABLES	YES
'root'@'%'	def	OPTIMIZE_LOCAL_TABLE	YES
'root'@'%'	def	PASSWORDLESS_USER_ADMIN	YES
'root'@'%'	def	PERSIST_RO_VARIABLES_ADMIN	YES
'root'@'%'	def	PROCESS	YES
'root'@'%'	def	REFERENCES	YES
'root'@'%'	def	RELOAD	YES
'root'@'%'	def	REPLICATION CLIENT	YES
'root'@'%'	def	REPLICATION SLAVE	YES
'root'@'%'	def	REPLICATION_APPLIER	YES
'root'@'%'	def	REPLICATION_SLAVE_ADMIN	YES
'root'@'%'	def	RESOURCE_GROUP_ADMIN	YES
'root'@'%'	def	RESOURCE_GROUP_USER	YES
'root'@'%'	def	ROLE_ADMIN	YES
'root'@'%'	def	SELECT	YES
'root'@'%'	def	SENSITIVE_VARIABLES_OBSERVER	YES
'root'@'%'	def	SERVICE_CONNECTION_ADMIN	YES
'root'@'%'	def	SESSION_VARIABLES_ADMIN	YES
'root'@'%'	def	SET_ANY_DEFINER	YES
'root'@'%'	def	SHOW DATABASES	YES
'root'@'%'	def	SHOW VIEW	YES
'root'@'%'	def	SHOW_ROUTINE	YES
'root'@'%'	def	SHUTDOWN	YES
'root'@'%'	def	SUPER	YES
'root'@'%'	def	SYSTEM_USER	YES
'root'@'%'	def	SYSTEM_VARIABLES_ADMIN	YES
'root'@'%'	def	TABLE_ENCRYPTION_ADMIN	YES
'root'@'%'	def	TELEMETRY_LOG_ADMIN	YES
'root'@'%'	def	TRANSACTION_GTID_TAG	YES
'root'@'%'	def	TRIGGER	YES
'root'@'%'	def	UPDATE	YES
'root'@'%'	def	XA_RECOVER_ADMIN	YES
EXPECTED
)
expect_output \
    "root global user privilege rows" \
    "$root_privileges_expected" \
    "SELECT GRANTEE, TABLE_CATALOG, PRIVILEGE_TYPE, IS_GRANTABLE "\
"FROM INFORMATION_SCHEMA.USER_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''' "\
"ORDER BY PRIVILEGE_TYPE;"

expect_output \
    "root user privilege count" \
    "70" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.USER_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''';"

expect_output \
    "root schema privileges absent" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMA_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''';"

expect_output \
    "root table privileges absent" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''';"

expect_output \
    "root column privileges absent" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMN_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''';"

expect_output \
    "user privilege alias predicate" \
    "SELECT" \
    "SELECT up.PRIVILEGE_TYPE FROM INFORMATION_SCHEMA.USER_PRIVILEGES AS up "\
"WHERE up.GRANTEE = '''root''@''%''' AND up.PRIVILEGE_TYPE = 'SELECT';"

status_output=$(run_mysql \
    "SELECT PRIVILEGE_TYPE FROM INFORMATION_SCHEMA.USER_PRIVILEGES "\
"WHERE GRANTEE = '''root''@''%''' ORDER BY PRIVILEGE_TYPE LIMIT 1; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
status_expected=$(printf '%b' "0\t-1")
if [ "$status_output" != "$status_expected" ]; then
    fail "successful privilege select status: expected [$status_expected], got [$status_output]"
fi

system_tables_expected=$(cat <<\EXPECTED
COLUMN_PRIVILEGES	information_schema	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
SCHEMA_PRIVILEGES	information_schema	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
TABLE_PRIVILEGES	information_schema	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
USER_PRIVILEGES	information_schema	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
EXPECTED
)
expect_output \
    "privilege system table rows" \
    "$system_tables_expected" \
    "SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "\
"TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('USER_PRIVILEGES','SCHEMA_PRIVILEGES',"\
"'TABLE_PRIVILEGES','COLUMN_PRIVILEGES') ORDER BY TABLE_NAME;"

system_columns_expected=$(cat <<\EXPECTED
COLUMN_PRIVILEGES	GRANTEE	1		NO	varchar	97	292	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(292)	select
COLUMN_PRIVILEGES	TABLE_CATALOG	2		NO	varchar	170	512	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(512)	select
COLUMN_PRIVILEGES	TABLE_SCHEMA	3		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
COLUMN_PRIVILEGES	TABLE_NAME	4		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
COLUMN_PRIVILEGES	COLUMN_NAME	5		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
COLUMN_PRIVILEGES	PRIVILEGE_TYPE	6		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
COLUMN_PRIVILEGES	IS_GRANTABLE	7		NO	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
SCHEMA_PRIVILEGES	GRANTEE	1		NO	varchar	97	292	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(292)	select
SCHEMA_PRIVILEGES	TABLE_CATALOG	2		NO	varchar	170	512	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(512)	select
SCHEMA_PRIVILEGES	TABLE_SCHEMA	3		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
SCHEMA_PRIVILEGES	PRIVILEGE_TYPE	4		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
SCHEMA_PRIVILEGES	IS_GRANTABLE	5		NO	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
TABLE_PRIVILEGES	GRANTEE	1		NO	varchar	97	292	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(292)	select
TABLE_PRIVILEGES	TABLE_CATALOG	2		NO	varchar	170	512	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(512)	select
TABLE_PRIVILEGES	TABLE_SCHEMA	3		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TABLE_PRIVILEGES	TABLE_NAME	4		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TABLE_PRIVILEGES	PRIVILEGE_TYPE	5		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TABLE_PRIVILEGES	IS_GRANTABLE	6		NO	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
USER_PRIVILEGES	GRANTEE	1		NO	varchar	97	292	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(292)	select
USER_PRIVILEGES	TABLE_CATALOG	2		NO	varchar	170	512	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(512)	select
USER_PRIVILEGES	PRIVILEGE_TYPE	3		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
USER_PRIVILEGES	IS_GRANTABLE	4		NO	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
EXPECTED
)
expect_output \
    "privilege system column rows" \
    "$system_columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "\
"DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "\
"COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('USER_PRIVILEGES','SCHEMA_PRIVILEGES',"\
"'TABLE_PRIVILEGES','COLUMN_PRIVILEGES') ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.USER_PRIVILEGES;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES WHERE nope = 'x';"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'order clause'" \
    "SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES ORDER BY nope;"

printf '%s\n' "mysql_baseline_information_schema_privileges_expectations: ok"
