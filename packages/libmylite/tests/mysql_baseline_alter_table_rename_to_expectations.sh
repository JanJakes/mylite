#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_rename_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_rename_to_expectations: $1" >&2
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
    "alter rename without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default_old RENAME no_default_new;"

run_mysql "CREATE TABLE ${DATABASE}.qualified_no_default_old (id INT);" >/dev/null
expect_error \
    "qualified source with unqualified target and no selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE ${DATABASE}.qualified_no_default_old RENAME no_default_new;"

expect_error \
    "missing qualified source with unqualified target and no selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE ${MISSING_DATABASE}.missing_source RENAME no_default_new;"

expect_error \
    "missing qualified source with qualified target" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing_source RENAME ${DATABASE}.missing_target;"

run_mysql "CREATE TABLE ${DATABASE}.bare_old (id INT);" >/dev/null
expect_output \
    "bare rename status" \
    "0	0" \
    "ALTER TABLE bare_old RENAME bare_new; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
expect_output \
    "bare rename side effect" \
    "bare_new" \
    "SHOW TABLES LIKE 'bare_new';" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.to_old (id INT);" >/dev/null
expect_output \
    "rename to status" \
    "0	0	to_new" \
    "ALTER TABLE to_old RENAME TO to_new; SELECT ROW_COUNT(), @@warning_count, (SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'to_new');" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.as_old (id INT);" >/dev/null
expect_output \
    "rename as status" \
    "0	0	as_new" \
    "ALTER TABLE as_old RENAME AS as_new; SELECT ROW_COUNT(), @@warning_count, (SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'as_new');" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.qualified_old (id INT);" >/dev/null
expect_output \
    "qualified same-schema alter rename status" \
    "0	0" \
    "ALTER TABLE ${DATABASE}.qualified_old RENAME ${DATABASE}.qualified_new; SELECT ROW_COUNT(), @@warning_count;"
expect_output \
    "qualified same-schema alter rename side effect" \
    "qualified_new" \
    "SHOW TABLES FROM ${DATABASE} LIKE 'qualified_new';"

run_mysql "CREATE TABLE ${DATABASE}.cross_old (id INT);" >/dev/null
expect_output \
    "cross-schema alter rename status" \
    "0	0" \
    "ALTER TABLE ${DATABASE}.cross_old RENAME ${OTHER_DATABASE}.cross_new; SELECT ROW_COUNT(), @@warning_count;"
expect_output \
    "cross-schema alter rename source hidden" \
    "" \
    "SHOW TABLES FROM ${DATABASE} LIKE 'cross_old';"
expect_output \
    "cross-schema alter rename target shown" \
    "cross_new" \
    "SHOW TABLES FROM ${OTHER_DATABASE} LIKE 'cross_new';"

run_mysql "CREATE TABLE ${DATABASE}.source_qualified_default_old (id INT);" >/dev/null
expect_output \
    "qualified source unqualified target uses default schema" \
    "target_in_other_default" \
    "ALTER TABLE ${DATABASE}.source_qualified_default_old RENAME target_in_other_default; SHOW TABLES LIKE 'target_in_other_default';" \
    "$OTHER_DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.source_unqualified_target_qualified (id INT);" >/dev/null
expect_output \
    "unqualified source qualified target moves to target schema" \
    "target_qualified" \
    "ALTER TABLE source_unqualified_target_qualified RENAME ${OTHER_DATABASE}.target_qualified; SHOW TABLES FROM ${OTHER_DATABASE} LIKE 'target_qualified';" \
    "$DATABASE"

expect_error \
    "unknown source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "ALTER TABLE missing_source RENAME missing_target;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.duplicate_source (id INT); CREATE TABLE ${DATABASE}.duplicate_target (id INT);" >/dev/null
expect_error \
    "duplicate target table" \
    1050 \
    42S01 \
    "Table 'duplicate_target' already exists" \
    "ALTER TABLE duplicate_source RENAME duplicate_target;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.same_name (id INT);" >/dev/null
expect_output \
    "same source and target is no-op" \
    "0	0	same_name" \
    "ALTER TABLE same_name RENAME same_name; SELECT ROW_COUNT(), @@warning_count, (SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'same_name');" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.missing_target_schema_source (id INT);" >/dev/null
expect_error \
    "unknown target schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE missing_target_schema_source RENAME ${MISSING_DATABASE}.new_name;" \
    "$DATABASE"

expect_output \
    "temporary table alter rename is supported upstream" \
    "2	0	2" \
    "CREATE TEMPORARY TABLE temp_old (id INT); INSERT INTO temp_old VALUES (1),(2); ALTER TABLE temp_old RENAME temp_new; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM temp_new;" \
    "$DATABASE"

run_mysql "CREATE VIEW ${DATABASE}.rename_view_source AS SELECT 1 AS id;" >/dev/null
expect_error \
    "view alter rename rejected as non-base table" \
    1347 \
    HY000 \
    "'${DATABASE}.rename_view_source' is not BASE TABLE" \
    "ALTER TABLE rename_view_source RENAME rename_view_target;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.multi_action_old (id INT);" >/dev/null
expect_output \
    "mysql combined alter rename action is supported upstream" \
    "0	0	id,added" \
    "ALTER TABLE multi_action_old RENAME multi_action_new, ADD COLUMN added INT; SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'multi_action_new';" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.multi_rename_old (id INT);" >/dev/null
expect_output \
    "mysql multiple rename actions are supported upstream" \
    "0	0	multi_rename_final" \
    "ALTER TABLE multi_rename_old RENAME multi_rename_new, RENAME multi_rename_final; SELECT ROW_COUNT(), @@warning_count, (SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'multi_rename_final');" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.rename_table_keyword_old (id INT);" >/dev/null
expect_error \
    "rename table keyword spelling is syntax error" \
    1064 \
    42000 \
    "near 'TABLE rename_table_keyword_new'" \
    "ALTER TABLE rename_table_keyword_old RENAME TABLE rename_table_keyword_new;" \
    "$DATABASE"

printf '%s\n' "baseline-alter-table-rename-to MySQL 8.4.9 expectations verified"
