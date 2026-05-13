#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temp_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_temporary_table_lifecycle_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
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
    "create temporary table without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE no_default (id INT);"

expect_output \
    "schema-qualified temporary table without selected schema" \
    "1	0	7" \
    "CREATE TEMPORARY TABLE ${DATABASE}.qualified_tmp (id INT); "\
"INSERT INTO ${DATABASE}.qualified_tmp VALUES (7); "\
"SELECT COUNT(*), @@warning_count, SUM(id) FROM ${DATABASE}.qualified_tmp;"

expect_error \
    "schema-qualified temporary table unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE TEMPORARY TABLE ${MISSING_DATABASE}.bad_tmp (id INT);"

expect_output \
    "temporary table shadows persistent table for dml" \
    "1	0	2:99" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS shadowed; "\
"CREATE TABLE shadowed (id INT, value INT); "\
"INSERT INTO shadowed VALUES (1, 10); "\
"CREATE TEMPORARY TABLE shadowed (id INT, value INT); "\
"INSERT INTO shadowed VALUES (2, 20), (3, 30); "\
"UPDATE shadowed SET value = 99 WHERE id = 2; "\
"DELETE FROM shadowed WHERE id = 3; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', value) ORDER BY id) "\
"FROM shadowed;"

expect_output \
    "drop temporary reveals persistent table" \
    "1:10" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS shadowed; "\
"CREATE TABLE shadowed (id INT, value INT); "\
"INSERT INTO shadowed VALUES (1, 10); "\
"CREATE TEMPORARY TABLE shadowed (id INT, value INT); "\
"INSERT INTO shadowed VALUES (2, 20); "\
"DROP TEMPORARY TABLE shadowed; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', value) ORDER BY id) FROM shadowed;"

expect_output \
    "plain drop table chooses temporary shadow first" \
    "1	0" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS drop_shadow; "\
"CREATE TABLE drop_shadow (id INT); "\
"CREATE TEMPORARY TABLE drop_shadow (id INT); "\
"INSERT INTO drop_shadow VALUES (9); "\
"DROP TABLE drop_shadow; "\
"SELECT COUNT(*), @@warning_count FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_shadow';"

expect_error \
    "drop temporary ignores persistent table" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.persistent_only'" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS persistent_only; "\
"CREATE TABLE persistent_only (id INT); "\
"DROP TEMPORARY TABLE persistent_only;"

expect_output \
    "drop temporary if exists persistent only warning" \
    "0	1	1" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS persistent_only_if_exists; "\
"CREATE TABLE persistent_only_if_exists (id INT); "\
"DROP TEMPORARY TABLE IF EXISTS persistent_only_if_exists; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'persistent_only_if_exists';"

expect_output \
    "create temporary if not exists ignores persistent table" \
    "0	0	0" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS if_exists_shadow; "\
"CREATE TABLE if_exists_shadow (id INT); "\
"INSERT INTO if_exists_shadow VALUES (1); "\
"CREATE TEMPORARY TABLE IF NOT EXISTS if_exists_shadow (id INT); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM if_exists_shadow;"

expect_output \
    "create temporary if not exists existing temporary warning" \
    "0	1" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE if_exists_existing_temp (id INT); "\
"CREATE TEMPORARY TABLE IF NOT EXISTS if_exists_existing_temp (id INT); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_contains \
    "show columns sees temporary table" \
    "name	varchar(10)	YES	MUL	NULL" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE meta_columns_tmp (id INT NOT NULL PRIMARY KEY, name VARCHAR(10), KEY meta_key (name(3))); "\
"SHOW COLUMNS FROM meta_columns_tmp;"

expect_contains \
    "show index sees temporary prefix index" \
    "meta_index_tmp	1	meta_key	1	name	A	0	3" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE meta_index_tmp (id INT NOT NULL PRIMARY KEY, name VARCHAR(10), KEY meta_key (name(3))); "\
"SHOW INDEX FROM meta_index_tmp;"

expect_contains \
    "show create table renders temporary table" \
    "CREATE TEMPORARY TABLE \`meta_create_tmp\`" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE meta_create_tmp (id INT NOT NULL PRIMARY KEY, name VARCHAR(10), KEY meta_key (name(3))); "\
"SHOW CREATE TABLE meta_create_tmp;"

expect_output \
    "information schema omits temporary table" \
    "0	0" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE meta_omitted_tmp (id INT); "\
"SELECT "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'meta_omitted_tmp'), "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'meta_omitted_tmp');"

expect_output \
    "show table status omits temporary table" \
    "" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE meta_status_tmp (id INT); "\
"SHOW TABLE STATUS LIKE 'meta_status_tmp';"

expect_output \
    "qualified temporary table survives dropped schema" \
    "NULL
1" \
    "USE ${OTHER_DATABASE}; "\
"CREATE TEMPORARY TABLE ${OTHER_DATABASE}.keep_tmp (id INT); "\
"INSERT INTO ${OTHER_DATABASE}.keep_tmp VALUES (1); "\
"DROP DATABASE ${OTHER_DATABASE}; "\
"SELECT DATABASE(); "\
"SELECT COUNT(*) FROM ${OTHER_DATABASE}.keep_tmp;"

expect_output \
    "temporary row dml rolls back inside user transaction" \
    "0" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE tx_rows (id INT); "\
"START TRANSACTION; "\
"INSERT INTO tx_rows VALUES (1); "\
"ROLLBACK; "\
"SELECT COUNT(*) FROM tx_rows;"

expect_output \
    "mysql keeps temporary create after rollback upstream" \
    "0" \
    "USE ${DATABASE}; "\
"START TRANSACTION; "\
"CREATE TEMPORARY TABLE tx_create (id INT); "\
"INSERT INTO tx_create VALUES (1); "\
"ROLLBACK; "\
"SELECT COUNT(*) FROM tx_create;"

expect_error \
    "mysql keeps temporary drop after rollback upstream" \
    1146 \
    42S02 \
    "Table '${DATABASE}.tx_drop' doesn't exist" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE tx_drop (id INT); "\
"START TRANSACTION; "\
"DROP TEMPORARY TABLE tx_drop; "\
"ROLLBACK; "\
"SELECT COUNT(*) FROM tx_drop;"

printf '%s\n' "mysql_baseline_temporary_table_lifecycle_expectations: ok"
