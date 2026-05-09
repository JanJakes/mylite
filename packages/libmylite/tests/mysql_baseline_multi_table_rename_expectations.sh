#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_rename_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_multi_table_rename_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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

expect_output \
    "multi rename case expectations require case-sensitive identifiers" \
    "0" \
    "SELECT @@lower_case_table_names;"

cleanup

expect_output \
    "multi rename same schema" \
    "0	0	c,d" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"RENAME TABLE a TO c, b TO d; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_output \
    "multi rename cross schema" \
    "0	0	${DATABASE}.d,${OTHER_DATABASE}.c" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE DATABASE ${OTHER_DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"CREATE TABLE ${OTHER_DATABASE}.b (id INT); "\
"RENAME TABLE ${DATABASE}.a TO ${OTHER_DATABASE}.c, ${OTHER_DATABASE}.b TO ${DATABASE}.d; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(TABLE_SCHEMA, '.', TABLE_NAME) ORDER BY TABLE_SCHEMA, TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA IN ('${DATABASE}', '${OTHER_DATABASE}');"

cleanup

expect_output \
    "multi rename swaps through temporary name left to right" \
    "0	0	2	1" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE s1 (id INT); "\
"CREATE TABLE s2 (id INT); "\
"INSERT INTO s1 VALUES (1); "\
"INSERT INTO s2 VALUES (2); "\
"RENAME TABLE s1 TO tmp_swap, s2 TO s1, tmp_swap TO s2; "\
"SELECT ROW_COUNT(), @@warning_count, (SELECT id FROM s1), (SELECT id FROM s2);"

cleanup

expect_output \
    "multi rename treats case-distinct targets as distinct" \
    "0	0	B,b" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE A (id INT); "\
"CREATE TABLE a (id INT); "\
"RENAME TABLE A TO B, a TO b; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename later missing source rolls back" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"RENAME TABLE a TO c, missing TO d;"

expect_output \
    "multi rename later missing source leaves originals" \
    "a,b" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename first target exists before later source move" \
    1050 \
    42S01 \
    "Table 'b' already exists" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"RENAME TABLE a TO b, b TO c;"

expect_output \
    "multi rename first target exists leaves originals" \
    "a,b" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename repeated target rolls back" \
    1050 \
    42S01 \
    "Table 'c' already exists" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"RENAME TABLE a TO c, b TO c;"

expect_output \
    "multi rename repeated target leaves originals" \
    "a,b" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename repeated source rolls back" \
    1146 \
    42S02 \
    "Table '${DATABASE}.a' doesn't exist" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"RENAME TABLE a TO c, a TO d;"

expect_output \
    "multi rename repeated source leaves original" \
    "a" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename qualified source unqualified target without default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"RENAME TABLE ${DATABASE}.a TO b;"

expect_output \
    "multi rename no default leaves table" \
    "a" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

expect_error \
    "multi rename unknown target schema rolls back" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"RENAME TABLE ${DATABASE}.a TO ${MISSING_DATABASE}.b;"

expect_output \
    "multi rename unknown target schema leaves table" \
    "a" \
    "SELECT GROUP_CONCAT(TABLE_NAME ORDER BY TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}';"

cleanup

printf '%s\n' "baseline-multi-table-rename MySQL 8.4.9 expectations verified"
