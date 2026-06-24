#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_procedure_placeholders_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" --skip-column-names "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    sql=$2
    expected=$3

    set +e
    output=$(run_mysql "$sql" --show-warnings 2>&1)
    rc=$?
    set -e
    if [ "$rc" -eq 0 ]; then
        fail "$label: expected error, got success"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

version=$(run_mysql "SELECT VERSION();" --skip-column-names)
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS mylite_sys_proc_probe;" --skip-column-names >/dev/null
run_mysql "CREATE DATABASE mylite_sys_proc_probe;" --skip-column-names >/dev/null

expect_output \
    "table_exists base view temporary missing" \
    "$(printf '%b' 'BASE TABLE\tVIEW\tTEMPORARY\t')" \
    "USE mylite_sys_proc_probe;
     CREATE TABLE base_table(id INT);
     CREATE VIEW view_table AS SELECT id FROM base_table;
     CREATE TEMPORARY TABLE temp_table(id INT);
     CALL sys.table_exists('mylite_sys_proc_probe','base_table',@base);
     CALL sys.table_exists('mylite_sys_proc_probe','view_table',@view);
     CALL sys.table_exists('mylite_sys_proc_probe','temp_table',@temp);
     CALL sys.table_exists('mylite_sys_proc_probe','missing_table',@missing);
     SELECT @base,@view,@temp,@missing;"

expect_output \
    "execute_prepared_stmt select" \
    "$(printf '%b' 'sys\t7')" \
    "USE mylite_sys_proc_probe;
     CALL sys.execute_prepared_stmt('SELECT DATABASE() AS db, 7 AS n');"

expect_output \
    "execute_prepared_stmt qualified ddl" \
    "BASE TABLE" \
    "USE mylite_sys_proc_probe;
     CALL sys.execute_prepared_stmt('CREATE TABLE mylite_sys_proc_probe.dynamic_table(id INT)');
     CALL sys.table_exists('mylite_sys_proc_probe','dynamic_table',@dynamic);
     SELECT @dynamic;"

expect_error \
    "table_exists arity" \
    "CALL sys.table_exists('mylite_sys_proc_probe','base_table');" \
    "ERROR 1318 (42000)"

expect_error \
    "execute_prepared_stmt arity" \
    "CALL sys.execute_prepared_stmt();" \
    "ERROR 1318 (42000)"

run_mysql "DROP DATABASE IF EXISTS mylite_sys_proc_probe;" --skip-column-names >/dev/null

printf '%s\n' "mysql_baseline_sys_procedure_placeholders_expectations: ok"
