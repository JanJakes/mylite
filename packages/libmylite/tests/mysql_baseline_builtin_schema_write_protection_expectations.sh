#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TEMP_TABLE="_mylite_builtin_write_temp_$$"

fail() {
    printf '%s\n' "mysql_baseline_builtin_schema_write_protection_expectations: $1" >&2
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

expect_error \
    "create information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE DATABASE information_schema;"
expect_error \
    "create information_schema if not exists denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE DATABASE IF NOT EXISTS information_schema;"
expect_error \
    "create schema information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE SCHEMA information_schema;"
expect_error \
    "alter database information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "ALTER DATABASE information_schema DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "alter schema information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "ALTER SCHEMA information_schema DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "drop information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "DROP DATABASE information_schema;"
expect_error \
    "drop schema information_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "DROP SCHEMA information_schema;"
expect_error \
    "drop information_schema if exists denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "DROP DATABASE IF EXISTS information_schema;"

expect_error \
    "create performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "CREATE DATABASE performance_schema;"
expect_error \
    "create performance_schema if not exists denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "CREATE DATABASE IF NOT EXISTS performance_schema;"
expect_error \
    "create schema performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "CREATE SCHEMA performance_schema;"
expect_error \
    "alter database performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "ALTER DATABASE performance_schema DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "alter schema performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "ALTER SCHEMA performance_schema DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "drop performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "DROP DATABASE performance_schema;"
expect_error \
    "drop schema performance_schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "DROP SCHEMA performance_schema;"
expect_error \
    "drop performance_schema if exists denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "DROP DATABASE IF EXISTS performance_schema;"

expect_error \
    "create mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "CREATE DATABASE mysql;"
expect_error \
    "create schema mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "CREATE SCHEMA mysql;"
expect_error \
    "alter database mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "ALTER DATABASE mysql DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "alter schema mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "ALTER SCHEMA mysql DEFAULT CHARACTER SET utf8mb4;"
expect_error \
    "drop mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "DROP DATABASE mysql;"
expect_error \
    "drop schema mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "DROP SCHEMA mysql;"
expect_error \
    "drop mysql system schema if exists rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "DROP DATABASE IF EXISTS mysql;"
expect_output \
    "create mysql if not exists no-op" \
    "Note	1007	Can't create database 'mysql'; database exists" \
    "CREATE DATABASE IF NOT EXISTS mysql; SHOW WARNINGS;"

expect_error \
    "create sys existing database" \
    1007 \
    HY000 \
    "Can't create database 'sys'; database exists" \
    "CREATE DATABASE sys;"
expect_error \
    "create schema sys existing database" \
    1007 \
    HY000 \
    "Can't create database 'sys'; database exists" \
    "CREATE SCHEMA sys;"
expect_output \
    "create sys if not exists no-op" \
    "Note	1007	Can't create database 'sys'; database exists" \
    "CREATE DATABASE IF NOT EXISTS sys; SHOW WARNINGS;"
expect_output \
    "alter database sys succeeds for root" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "ALTER DATABASE sys DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = 'sys';"
expect_output \
    "alter schema sys succeeds for root" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "ALTER SCHEMA sys DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = 'sys';"

expect_error \
    "information_schema temporary table denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE TEMPORARY TABLE information_schema.${TEMP_TABLE} (id INT);"
expect_error \
    "information_schema drop temporary table denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "DROP TEMPORARY TABLE information_schema.${TEMP_TABLE};"
expect_error \
    "performance_schema temporary table denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "CREATE TEMPORARY TABLE performance_schema.${TEMP_TABLE} (id INT);"
expect_error \
    "performance_schema drop temporary table unknown" \
    1051 \
    42S02 \
    "Unknown table 'performance_schema.${TEMP_TABLE}'" \
    "DROP TEMPORARY TABLE performance_schema.${TEMP_TABLE};"

expect_output \
    "mysql temporary table writable for root" \
    "1" \
    "CREATE TEMPORARY TABLE mysql.${TEMP_TABLE} (id INT); "\
"INSERT INTO mysql.${TEMP_TABLE} VALUES (1); "\
"SELECT COUNT(*) FROM mysql.${TEMP_TABLE}; "\
"DROP TEMPORARY TABLE mysql.${TEMP_TABLE};"
expect_output \
    "selected mysql temporary table writable for root" \
    "mysql	1" \
    "USE mysql; "\
"CREATE TEMPORARY TABLE ${TEMP_TABLE} (id INT); "\
"INSERT INTO ${TEMP_TABLE} VALUES (1); "\
"SELECT DATABASE(), COUNT(*) FROM ${TEMP_TABLE}; "\
"DROP TEMPORARY TABLE ${TEMP_TABLE};"

expect_output \
    "sys temporary table writable for root" \
    "1" \
    "CREATE TEMPORARY TABLE sys.${TEMP_TABLE} (id INT); "\
"INSERT INTO sys.${TEMP_TABLE} VALUES (1); "\
"SELECT COUNT(*) FROM sys.${TEMP_TABLE}; "\
"DROP TEMPORARY TABLE sys.${TEMP_TABLE};"
expect_output \
    "selected sys temporary table writable for root" \
    "sys	1" \
    "USE sys; "\
"CREATE TEMPORARY TABLE ${TEMP_TABLE} (id INT); "\
"INSERT INTO ${TEMP_TABLE} VALUES (1); "\
"SELECT DATABASE(), COUNT(*) FROM ${TEMP_TABLE}; "\
"DROP TEMPORARY TABLE ${TEMP_TABLE};"

printf '%s\n' "mysql_baseline_builtin_schema_write_protection_expectations: ok"
