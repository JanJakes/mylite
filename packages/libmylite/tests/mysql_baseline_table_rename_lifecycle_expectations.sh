#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_rename_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_table_rename_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_error \
    "rename without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "RENAME TABLE no_default_old TO no_default_new;"

run_mysql "CREATE TABLE ${DATABASE}.qualified_no_default_old (id INT);" >/dev/null
expect_error \
    "qualified source with unqualified target and no selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "RENAME TABLE ${DATABASE}.qualified_no_default_old TO no_default_new;"

run_mysql "CREATE TABLE ${DATABASE}.simple_old (id INT);" >/dev/null
expect_output \
    "simple rename status" \
    "0	0" \
    "RENAME TABLE simple_old TO simple_new; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
expect_output \
    "simple rename old hidden" \
    "" \
    "SHOW TABLES LIKE 'simple_old';" \
    "$DATABASE"
expect_output \
    "simple rename new shown" \
    "simple_new" \
    "SHOW TABLES LIKE 'simple_new';" \
    "$DATABASE"
expect_output \
    "simple rename information schema side effect" \
    "0	1" \
    "SELECT (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'simple_old'), (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'simple_new' AND COLUMN_NAME = 'id');"

run_mysql "CREATE TABLE ${DATABASE}.qualified_old (id INT);" >/dev/null
expect_output \
    "qualified same-schema rename status" \
    "0	0" \
    "RENAME TABLE ${DATABASE}.qualified_old TO ${DATABASE}.qualified_new; SELECT ROW_COUNT(), @@warning_count;"
expect_output \
    "qualified same-schema side effect" \
    "qualified_new" \
    "SHOW TABLES FROM ${DATABASE} LIKE 'qualified_new';"

run_mysql "CREATE TABLE ${DATABASE}.cross_old (id INT);" >/dev/null
expect_output \
    "cross-schema rename status" \
    "0	0" \
    "RENAME TABLE ${DATABASE}.cross_old TO ${OTHER_DATABASE}.cross_new; SELECT ROW_COUNT(), @@warning_count;"
expect_output \
    "cross-schema source hidden" \
    "" \
    "SHOW TABLES FROM ${DATABASE} LIKE 'cross_old';"
expect_output \
    "cross-schema target shown" \
    "cross_new" \
    "SHOW TABLES FROM ${OTHER_DATABASE} LIKE 'cross_new';"

run_mysql "CREATE TABLE ${DATABASE}.source_qualified_default_old (id INT);" >/dev/null
expect_output \
    "qualified source unqualified target uses default schema" \
    "target_in_other_default" \
    "RENAME TABLE ${DATABASE}.source_qualified_default_old TO target_in_other_default; SHOW TABLES LIKE 'target_in_other_default';" \
    "$OTHER_DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.source_unqualified_target_qualified (id INT);" >/dev/null
expect_output \
    "unqualified source qualified target moves to target schema" \
    "target_qualified" \
    "RENAME TABLE source_unqualified_target_qualified TO ${OTHER_DATABASE}.target_qualified; SHOW TABLES FROM ${OTHER_DATABASE} LIKE 'target_qualified';" \
    "$DATABASE"

expect_error \
    "unknown source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "RENAME TABLE missing_source TO missing_target;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.duplicate_source (id INT); CREATE TABLE ${DATABASE}.duplicate_target (id INT);" >/dev/null
expect_error \
    "duplicate target table" \
    1050 \
    42S01 \
    "Table 'duplicate_target' already exists" \
    "RENAME TABLE duplicate_source TO duplicate_target;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.same_name (id INT);" >/dev/null
expect_error \
    "same source and target name" \
    1050 \
    42S01 \
    "Table 'same_name' already exists" \
    "RENAME TABLE same_name TO same_name;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.missing_target_schema_source (id INT);" >/dev/null
expect_error \
    "unknown target schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "RENAME TABLE missing_target_schema_source TO ${MISSING_DATABASE}.new_name;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.multi_a (id INT); CREATE TABLE ${DATABASE}.multi_b (id INT);" >/dev/null
expect_output \
    "mysql multi-table rename is atomic and supported upstream" \
    "0	0	multi_c,multi_d" \
    "RENAME TABLE multi_a TO multi_c, multi_b TO multi_d; SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME LIKE 'multi_%';" \
    "$DATABASE"

run_mysql "CREATE VIEW ${DATABASE}.rename_view_source AS SELECT 1 AS id;" >/dev/null
expect_output \
    "mysql view rename is supported upstream" \
    "rename_view_target	VIEW" \
    "RENAME TABLE rename_view_source TO rename_view_target; SHOW FULL TABLES LIKE 'rename_view%';" \
    "$DATABASE"

printf '%s\n' "baseline-table-rename-lifecycle MySQL 8.4.9 expectations verified"
