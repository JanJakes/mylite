#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_dml_scalar_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_dml_scalar_residuals_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expected=$(cat <<'EXPECTED'
insert	1	0	2	b	7
update	1	0	1	y	9
EXPECTED
)
expect_output \
    "DML REGEXP_SUBSTR and parenthesized scalar values" \
    "$expected" \
    "CREATE TABLE t(id INT PRIMARY KEY, v VARCHAR(64), i INT) ENGINE=InnoDB; "\
"INSERT INTO t VALUES (1, 'seed', 0); "\
"INSERT INTO t(id, v, i) VALUES (2, REGEXP_SUBSTR('abc', 'b', 1), (7)); "\
"SELECT 'insert', ROW_COUNT(), @@warning_count, id, v, i FROM t WHERE id = 2; "\
"UPDATE t SET v = REGEXP_SUBSTR('xyz', 'y', 1), i = ('9') WHERE id = 1; "\
"SELECT 'update', ROW_COUNT(), @@warning_count, id, v, i FROM t WHERE id = 1;" \
    "$DATABASE"

printf '%s\n' "mysql_parser_corpus_dml_scalar_residuals_expectations: ok"
