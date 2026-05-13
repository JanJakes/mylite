#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_add_second_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_add_second_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
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

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
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

core_expected=$(cat <<EXPECTED
2008-01-02 13:29:18	2008-01-02 13:29:18	2008-01-02 13:29:16	2008-01-02 13:29:17	2008-01-02 00:00:01	NULL	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core date_add second values" \
    "$core_expected" \
    "DO 0; SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"DATE_ADD(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND), "\
"DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND), "\
"DATE_ADD('2008-01-02 13:29:17', INTERVAL 0 SECOND), "\
"DATE_ADD('2008-01-02', INTERVAL 1 SECOND), "\
"DATE_ADD(NULL, INTERVAL 1 SECOND), "\
"DATE_ADD('2008-01-02 13:29:17', INTERVAL NULL SECOND), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)	shifted
2008-01-02 13:29:18	2008-01-02 13:29:16
EXPECTED
)
expect_output_with_headers \
    "date_add labels" \
    "$labels_expected" \
    "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND) AS shifted FROM DUAL;" \
    "$DATABASE"

rollover_expected=$(cat <<EXPECTED
2024-02-29 00:00:00	2024-03-01 00:00:00
EXPECTED
)
expect_output \
    "leap rollover" \
    "$rollover_expected" \
    "SELECT DATE_ADD('2024-02-28 23:59:59', INTERVAL 1 SECOND), "\
"DATE_ADD('2024-02-29 23:59:59', INTERVAL 1 SECOND);" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "date_add do status" \
    "$do_expected" \
    "DO DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"DATE_ADD(NULL, INTERVAL 1 SECOND); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "ansi quotes makes double quoted date an identifier" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; "\
"SELECT DATE_ADD(\"2008-01-02 13:29:17\", INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_error \
    "default sql mode rejects whitespace after date_add" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = ''; "\
"SELECT DATE_ADD ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_error \
    "default sql mode rejects date_add identifier immediately followed by paren" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS date_add; CREATE TABLE date_add(id INT);" \
    "$DATABASE"

expect_output \
    "default sql mode allows date_add identifier before spaced table definition" \
    "date_add" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS date_add; CREATE TABLE date_add (id INT); "\
"SHOW TABLES LIKE 'date_add'; DROP TABLE date_add;" \
    "$DATABASE"

expect_output \
    "ignore_space accepts whitespace after date_add" \
    "2008-01-02 13:29:18" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT DATE_ADD ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_error \
    "ignore_space reserves unquoted date_add identifier" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS \`date_add\`; CREATE TABLE date_add (id INT);" \
    "$DATABASE"

expect_output \
    "ignore_space allows quoted date_add identifier" \
    "date_add" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS \`date_add\`; CREATE TABLE \`date_add\` (id INT); "\
"SHOW TABLES LIKE 'date_add'; DROP TABLE \`date_add\`;" \
    "$DATABASE"

expect_output \
    "ignore_space still allows second identifier" \
    "second" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS second_identifier; CREATE TABLE second_identifier (second INT); "\
"SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'second_identifier' "\
"AND COLUMN_NAME = 'second'; DROP TABLE second_identifier;" \
    "$DATABASE"

expect_upstream_accepts \
    "other interval unit accepted by MySQL but deferred by MyLite" \
    "SET SESSION sql_mode = ''; SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 MINUTE);" \
    "$DATABASE"

expect_upstream_accepts \
    "interval expression accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1+1 SECOND);" \
    "$DATABASE"

expect_upstream_accepts \
    "string interval accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL '1' SECOND);" \
    "$DATABASE"

expect_upstream_accepts \
    "invalid date warning behavior accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_ADD('2016-07-00', INTERVAL 1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "lower-year result accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_ADD('1000-01-01 00:00:00', INTERVAL -1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "overflow warning behavior accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_ADD('9999-12-31 23:59:59', INTERVAL 1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_date_add_second_expectations: ok"
