#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ddl_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_ddl_residual_surfaces_expectations: $1" >&2
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

    output=$(run_mysql "$sql")
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
"CREATE TABLE t1(e INT, m INT, c INT, d DATE, dt DATETIME, tm TIME, v VARCHAR(10));" \
    >/dev/null

expect_success \
    "type aliases" \
    "USE ${DATABASE}; CREATE TABLE type_aliases ("\
"d DOUBLE PRECISION(42,12), r REAL(42,12), f FLOAT(58,0) SIGNED, "\
"y YEAR UNSIGNED, y4 YEAR(4) UNSIGNED, vb VARCHAR(10) BYTE, lb LONG BYTE);"
expect_output \
    "type alias metadata" \
    "double(42,12)
double(42,12)
float(58,0)
year
year
varbinary(10)
mediumblob" \
    "USE ${DATABASE}; SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'type_aliases' "\
"ORDER BY ORDINAL_POSITION;"

expect_success "legacy create index type prefix" \
    "USE ${DATABASE}; CREATE INDEX e_index TYPE btree ON t1(e);"
expect_success "legacy create index type suffix" \
    "USE ${DATABASE}; CREATE INDEX m_index ON t1(m) TYPE btree;"

expect_success "alter table character set binary" \
    "USE ${DATABASE}; ALTER TABLE t1 CHARACTER SET binary;"
expect_success "convert default charset with collate" \
    "USE ${DATABASE}; ALTER TABLE t1 CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin;"

expect_success \
    "generated virtual not null with table options" \
    "USE ${DATABASE}; CREATE TABLE generated_residual ("\
"pk INT NOT NULL AUTO_INCREMENT, c INT NOT NULL, "\
"g INT GENERATED ALWAYS AS ((c + c)) VIRTUAL NOT NULL, "\
"PRIMARY KEY (pk)) ENGINE=InnoDB AUTO_INCREMENT=30 DEFAULT CHARSET=utf8mb4;"

expect_error \
    "fulltext parser plugin diagnostic" \
    "USE ${DATABASE}; CREATE TABLE ft_parser (a TEXT, FULLTEXT(a) WITH PARSER simple_parser);"
expect_error \
    "malformed generated expression remains error" \
    "USE ${DATABASE}; CREATE TABLE bad_generated (c INT, "\
"g INT GENERATED ALWAYS AS () VIRTUAL NOT NULL) ENGINE=InnoDB;"
expect_error \
    "incomplete fulltext parser remains error" \
    "USE ${DATABASE}; CREATE TABLE bad_fulltext (a TEXT, FULLTEXT(a) WITH PARSER);"

cleanup

printf '%s\n' "mysql_parser_corpus_ddl_residual_surfaces_expectations: ok"
