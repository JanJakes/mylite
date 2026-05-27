#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_builtin_schema_catalog_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_builtin_schema_catalog_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "sys built-in schema is populated" \
    "1" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'sys_config';"

expect_output \
    "show databases user like filter" \
    "$DATABASE" \
    "SHOW DATABASES LIKE '${DATABASE}';"
expect_output \
    "show schemas user like filter" \
    "$DATABASE" \
    "SHOW SCHEMAS LIKE '${DATABASE}';"
expect_output \
    "show databases mysql" \
    "mysql" \
    "SHOW DATABASES LIKE 'mysql';"
expect_output \
    "show databases performance schema" \
    "performance_schema" \
    "SHOW DATABASES LIKE 'performance_schema';"
expect_output \
    "show databases sys" \
    "sys" \
    "SHOW DATABASES LIKE 'sys';"

schemata_expected=$(cat <<EXPECTED
information_schema	utf8mb3	utf8mb3_general_ci	NULL	NO
${DATABASE}	utf8mb4	utf8mb4_0900_ai_ci	NULL	NO
mysql	utf8mb4	utf8mb4_0900_ai_ci	NULL	NO
performance_schema	utf8mb4	utf8mb4_0900_ai_ci	NULL	NO
sys	utf8mb4	utf8mb4_0900_ai_ci	NULL	NO
EXPECTED
)
expect_output \
    "information schema schemata built-ins" \
    "$schemata_expected" \
    "SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME, "\
"SQL_PATH, DEFAULT_ENCRYPTION "\
"FROM INFORMATION_SCHEMA.SCHEMATA "\
"WHERE SCHEMA_NAME IN "\
"('information_schema','mysql','performance_schema','sys','${DATABASE}') "\
"ORDER BY SCHEMA_NAME;"

use_expected=$(cat <<\EXPECTED
information_schema	utf8mb3	utf8mb3_general_ci
mysql	utf8mb4	utf8mb4_0900_ai_ci
performance_schema	utf8mb4	utf8mb4_0900_ai_ci
sys	utf8mb4	utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "use built-in schemas" \
    "$use_expected" \
    "USE Information_Schema; SELECT DATABASE(), @@character_set_database, @@collation_database; "\
"USE mysql; SELECT DATABASE(), @@character_set_database, @@collation_database; "\
"USE performance_schema; SELECT DATABASE(), @@character_set_database, @@collation_database; "\
"USE sys; SELECT DATABASE(), @@character_set_database, @@collation_database;"

expect_error \
    "create information schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE DATABASE information_schema;"
expect_error \
    "create mysql system schema rejected" \
    3552 \
    HY000 \
    "Access to system schema 'mysql' is rejected." \
    "CREATE DATABASE mysql;"
expect_error \
    "create performance schema denied" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'performance_schema'" \
    "CREATE DATABASE performance_schema;"

printf '%s\n' "mysql_baseline_builtin_schema_catalog_expectations: ok"
