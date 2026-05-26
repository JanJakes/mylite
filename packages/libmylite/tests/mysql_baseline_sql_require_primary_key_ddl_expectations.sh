#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_sql_require_primary_key_ddl_probe"

fail() {
    printf '%s\n' "mysql_baseline_sql_require_primary_key_ddl_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "SET SESSION sql_require_primary_key=DEFAULT; \
DROP DATABASE IF EXISTS \`$DATABASE\`; CREATE DATABASE \`$DATABASE\`; USE \`$DATABASE\`;" \
    >/dev/null

expect_output \
    "session assignment forms" \
    "on	1	0	1	1	0	0	0
local-off	0	0	0	0	0	0	0
sys-session	1	0	1	1	0	0	0
default	0	0	0	0	0	0	0" \
    "USE \`$DATABASE\`; \
     SET SESSION sql_require_primary_key=ON; \
     SELECT 'on', @@sql_require_primary_key, @@global.sql_require_primary_key, \
            @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET LOCAL sql_require_primary_key=OFF; \
     SELECT 'local-off', @@sql_require_primary_key, @@global.sql_require_primary_key, \
            @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET @@session.sql_require_primary_key=TRUE; \
     SELECT 'sys-session', @@sql_require_primary_key, @@global.sql_require_primary_key, \
            @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET @@sql_require_primary_key=DEFAULT; \
     SELECT 'default', @@sql_require_primary_key, @@global.sql_require_primary_key, \
            @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
            @@warning_count, @@error_count, ROW_COUNT();"

expect_error \
    "invalid sql_require_primary_key value" \
    1231 \
    42000 \
    "can't be set to the value of '2'" \
    "SET SESSION sql_require_primary_key=2;"

expect_error \
    "null sql_require_primary_key value" \
    1231 \
    42000 \
    "can't be set to the value of 'NULL'" \
    "SET SESSION sql_require_primary_key=NULL;"

expect_error \
    "create table without primary key" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; CREATE TABLE no_pk (id INT);"

expect_error \
    "unique key is not sufficient" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE unique_only (id INT NOT NULL, UNIQUE KEY u_id(id));"

expect_output \
    "primary key creation succeeds" \
    "inline	0	0
table	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE inline_pk (id INT PRIMARY KEY); SELECT 'inline', ROW_COUNT(), @@warning_count; \
     CREATE TABLE table_pk (id INT, PRIMARY KEY(id)); SELECT 'table', ROW_COUNT(), @@warning_count;"

run_mysql "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=OFF; \
CREATE TABLE existing_no_pk (id INT); CREATE TABLE like_no_pk_source (id INT); \
CREATE TABLE like_pk_source (id INT PRIMARY KEY, v INT);" >/dev/null

expect_output \
    "if not exists no-op is not rechecked" \
    "if-exists	0	1" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE IF NOT EXISTS existing_no_pk (id INT PRIMARY KEY); \
     SELECT 'if-exists', ROW_COUNT(), @@warning_count;"

expect_error \
    "like no primary key source fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE like_no_pk LIKE like_no_pk_source;"

expect_output \
    "like primary key source succeeds" \
    "like-pk	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE like_pk LIKE like_pk_source; SELECT 'like-pk', ROW_COUNT(), @@warning_count;"

expect_error \
    "ctas without primary key fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TABLE ctas_no_pk AS SELECT id, v FROM like_pk_source;"

expect_error \
    "alter no primary key add column fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE existing_no_pk ADD COLUMN v INT;"

expect_error \
    "alter no primary key add key fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE existing_no_pk ADD KEY k_id(id);"

expect_error \
    "create index on no primary key table fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE INDEX k_id ON existing_no_pk(id);"

expect_error \
    "alter no primary key comment fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE existing_no_pk COMMENT='blocked';"

expect_output \
    "rename no primary key table still succeeds" \
    "rename-no-pk	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE existing_no_pk RENAME TO renamed_no_pk; \
     SELECT 'rename-no-pk', ROW_COUNT(), @@warning_count;"

expect_output \
    "add primary key to existing no primary key table succeeds" \
    "add-pk	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE renamed_no_pk ADD PRIMARY KEY(id); \
     SELECT 'add-pk', ROW_COUNT(), @@warning_count;"

expect_error \
    "temporary without primary key fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TEMPORARY TABLE temp_no_pk (id INT);"

expect_output \
    "temporary with primary key succeeds" \
    "temp-pk	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     CREATE TEMPORARY TABLE temp_pk (id INT PRIMARY KEY); \
     SELECT 'temp-pk', ROW_COUNT(), @@warning_count;"

expect_error \
    "drop primary key fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE table_pk DROP PRIMARY KEY;"

expect_error \
    "drop constraint primary fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE table_pk DROP CONSTRAINT \`PRIMARY\`;"

run_mysql "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
CREATE TABLE ai_pk (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY); \
CREATE TABLE drop_pk_column (id INT PRIMARY KEY, v INT);" >/dev/null

expect_error \
    "drop primary key on auto increment primary fails with primary-key requirement" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE ai_pk DROP PRIMARY KEY;"

expect_error \
    "drop primary key column fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE drop_pk_column DROP COLUMN id;"

expect_error \
    "drop primary key add secondary fails" \
    3750 \
    HY000 \
    "without a primary key" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE table_pk DROP PRIMARY KEY, ADD KEY k_id(id);"

expect_output \
    "drop primary key add primary succeeds" \
    "swap-pk	0	0" \
    "USE \`$DATABASE\`; SET SESSION sql_require_primary_key=ON; \
     ALTER TABLE table_pk DROP PRIMARY KEY, ADD PRIMARY KEY(id); \
     SELECT 'swap-pk', ROW_COUNT(), @@warning_count;"

run_mysql "SET SESSION sql_require_primary_key=DEFAULT; DROP DATABASE IF EXISTS \`$DATABASE\`;" \
    >/dev/null
