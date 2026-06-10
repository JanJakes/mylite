#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_create_table_attribute_order_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_create_table_attribute_order_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "default before not null" \
    "$(printf '%b' 'id\tint\tNO\tPRI\t0\t\nname\tvarchar(80)\tNO\t\t\t')" \
    "USE ${DATABASE}; "\
"CREATE TABLE default_before_not_null ("\
"id INT DEFAULT '0' NOT NULL, "\
"name VARCHAR(80) DEFAULT '' NOT NULL, "\
"PRIMARY KEY (id)); "\
"SHOW COLUMNS FROM default_before_not_null;"

expect_output \
    "primary before default" \
    "$(printf '%b' 'id\tint\tNO\tPRI\t3\t\nb\tint\tYES\t\tNULL\t')" \
    "USE ${DATABASE}; "\
"CREATE TABLE primary_before_default (id INT PRIMARY KEY DEFAULT 3, b INT); "\
"SHOW COLUMNS FROM primary_before_default;"

expect_output \
    "default before primary" \
    "$(printf '%b' 'id\tint\tNO\tPRI\t3\t\nb\tint\tYES\t\tNULL\t')" \
    "USE ${DATABASE}; "\
"CREATE TABLE default_before_primary (id INT DEFAULT 3 PRIMARY KEY, b INT); "\
"SHOW COLUMNS FROM default_before_primary;"

expect_output \
    "unique before default" \
    "$(printf '%b' 'id\tint\tYES\tUNI\t3\t\nb\tint\tYES\t\tNULL\t')" \
    "USE ${DATABASE}; "\
"CREATE TABLE unique_before_default (id INT UNIQUE DEFAULT 3, b INT); "\
"SHOW COLUMNS FROM unique_before_default;"

expect_output \
    "auto increment legacy order" \
    "$(printf '%b' 'id\tint\tNO\tPRI\tNULL\tauto_increment\nname\tvarchar(32)\tNO\t\t\t')" \
    "USE ${DATABASE}; "\
"CREATE TABLE auto_increment_legacy ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"name VARCHAR(32) DEFAULT '' NOT NULL); "\
"SHOW COLUMNS FROM auto_increment_legacy;"

expect_error \
    "default null before not null semantic rejection" \
    1067 \
    42000 \
    "Invalid default value for 'id'" \
    "USE ${DATABASE}; CREATE TABLE invalid_default_null_order "\
"(id INT DEFAULT NULL NOT NULL);"

cleanup

printf '%s\n' "mysql_parser_corpus_create_table_attribute_order_expectations: ok"
