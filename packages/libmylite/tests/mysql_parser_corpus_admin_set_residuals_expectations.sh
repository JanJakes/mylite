#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_admin_set_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_admin_set_residuals_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
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

expect_first_column_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" | cut -f1)
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2

    if ! run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected success"
    fi
}

expect_error() {
    label=$1
    sql=$2

    if run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected error"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t1(id INT NOT NULL PRIMARY KEY, f1 INT, f2 INT) ENGINE=InnoDB; "\
"CREATE TABLE t2(id INT NOT NULL PRIMARY KEY) ENGINE=InnoDB; "\
"INSERT INTO t1 VALUES (1,10,20),(2,30,40);" >/dev/null

expect_success "analyze tables plural" "USE ${DATABASE}; ANALYZE TABLES t1;"
expect_success "optimize tables plural" "USE ${DATABASE}; OPTIMIZE TABLES t1;"

expect_first_column_output \
    "describe identifier filter" \
    "f1" \
    "USE ${DATABASE}; DESCRIBE t1 f1;"

expect_first_column_output \
    "describe string filter" \
    "f1
f2" \
    "USE ${DATABASE}; DESCRIBE t1 'f%';"

expect_success \
    "describe select" \
    "USE ${DATABASE}; DESCRIBE SELECT * FROM t1 WHERE id = 1;"
expect_success \
    "explain analyze delete" \
    "USE ${DATABASE}; EXPLAIN ANALYZE DELETE FROM t2 WHERE id = 0;"
expect_success "show engine csv logs" "SHOW ENGINE CSV LOGS;"
expect_success "show engine csv mutex" "SHOW ENGINE CSV MUTEX;"
expect_success "show engine myisam mutex" "SHOW ENGINE MYISAM MUTEX;"
expect_success "show extended columns" "USE ${DATABASE}; SHOW EXTENDED COLUMNS FROM t1;"
expect_success "show extended full columns" "USE ${DATABASE}; SHOW EXTENDED FULL COLUMNS FROM t1;"
expect_success "show extended index" "USE ${DATABASE}; SHOW EXTENDED INDEX FROM t1;"

expect_output \
    "set system variable assign operator" \
    "UTC" \
    "SET @@time_zone := 'UTC'; SELECT @@time_zone;"

expect_error "legacy show master status" "SHOW MASTER STATUS;"
expect_error "legacy show slave status" "SHOW SLAVE STATUS;"
expect_error "legacy show slave hosts" "SHOW SLAVE HOSTS;"

cleanup

printf '%s\n' "mysql_parser_corpus_admin_set_residuals_expectations: ok"
