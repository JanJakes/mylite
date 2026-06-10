#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ddl_constraints_options_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_ddl_constraint_option_placeholders: $1" >&2
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
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

expect_success \
    "encryption option" \
    "USE ${DATABASE}; CREATE TABLE encrypted_n (id INT) ENCRYPTION='N';"
expect_error \
    "data directory parsed before path validation" \
    3121 \
    HY000 \
    "DATA DIRECTORY location must be in a known directory" \
    "USE ${DATABASE}; CREATE TABLE dir_t (id INT) DATA DIRECTORY='/tmp' INDEX DIRECTORY='/tmp';"
expect_success \
    "column storage and format attributes" \
    "USE ${DATABASE}; CREATE TABLE column_attrs ("\
"a INT STORAGE DISK, b INT STORAGE MEMORY, c INT COLUMN_FORMAT DYNAMIC, "\
"d INT COLUMN_FORMAT FIXED, e INT COLUMN_FORMAT DEFAULT);"
expect_success \
    "secondary engine options" \
    "USE ${DATABASE}; CREATE TABLE secondary_t (id INT) SECONDARY_ENGINE=myisam; "\
"CREATE TABLE secondary_space_t (id INT) SECONDARY_ENGINE MOCK;"
expect_success \
    "not secondary column attribute" \
    "USE ${DATABASE}; CREATE TABLE not_secondary_t (d DATE NOT SECONDARY);"

expect_contains \
    "bare constraint primary key" \
    "PRIMARY KEY (\`id\`)" \
    "USE ${DATABASE}; CREATE TABLE bare_pk (id INT, CONSTRAINT PRIMARY KEY (id)); "\
"SHOW CREATE TABLE bare_pk;"
expect_output \
    "bare constraint foreign key generated" \
    "1" \
    "USE ${DATABASE}; CREATE TABLE fk_parent (id INT PRIMARY KEY); "\
"CREATE TABLE fk_child (pid INT, CONSTRAINT FOREIGN KEY (pid) REFERENCES fk_parent(id)); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'fk_child' "\
"AND CONSTRAINT_TYPE = 'FOREIGN KEY';"
expect_contains \
    "check in expression" \
    "in (10,20)" \
    "USE ${DATABASE}; CREATE TABLE check_in_t ("\
"f1 INT CHECK (f1 IN (10, 20)), f2 INT, CHECK (f2 NOT IN (100, 120))); "\
"SHOW CREATE TABLE check_in_t;"
expect_contains \
    "alter constraint not enforced" \
    "NOT ENFORCED" \
    "USE ${DATABASE}; CREATE TABLE alter_constraint_t ("\
"id INT, CONSTRAINT c CHECK (id > 0)); "\
"ALTER TABLE alter_constraint_t ALTER CONSTRAINT c NOT ENFORCED; "\
"SHOW CREATE TABLE alter_constraint_t;"

cleanup

printf '%s\n' "mysql_parser_corpus_ddl_constraint_option_placeholders: ok"
