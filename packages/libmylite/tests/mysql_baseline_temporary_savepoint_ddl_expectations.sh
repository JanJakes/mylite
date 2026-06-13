#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporary_savepoint_ddl_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporary_savepoint_ddl_expectations: $1" >&2
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
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error containing [$expected], command succeeded with [$output]"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
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

created_expected=$(cat <<EXPECTED
created_count	0
created_rows	2:20
EXPECTED
)
expect_output \
    "temporary create survives rollback to savepoint while row changes roll back" \
    "$created_expected" \
    "START TRANSACTION; "\
"SAVEPOINT sp_create; "\
"CREATE TEMPORARY TABLE sp_created (id INT PRIMARY KEY, value INT); "\
"INSERT INTO sp_created VALUES (1, 10); "\
"ROLLBACK TO SAVEPOINT sp_create; "\
"SELECT 'created_count', COUNT(*) FROM sp_created; "\
"INSERT INTO sp_created VALUES (2, 20); "\
"SELECT 'created_rows', GROUP_CONCAT(CONCAT(id, ':', value) ORDER BY id) FROM sp_created; "\
"COMMIT;" \
    "$DATABASE"

expect_error \
    "temporary drop is not undone by rollback to savepoint" \
    "ERROR 1146 (42S02)" \
    "CREATE TEMPORARY TABLE sp_dropped (id INT PRIMARY KEY); "\
"INSERT INTO sp_dropped VALUES (1); "\
"START TRANSACTION; "\
"SAVEPOINT sp_drop; "\
"DROP TEMPORARY TABLE sp_dropped; "\
"ROLLBACK TO SAVEPOINT sp_drop; "\
"SELECT COUNT(*) FROM sp_dropped;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_temporary_savepoint_ddl_expectations: ok"
