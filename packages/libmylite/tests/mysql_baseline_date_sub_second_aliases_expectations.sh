#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_sub_second_aliases_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_sub_second_aliases_expectations: $1" >&2
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
2008-01-02 13:29:16	2008-01-02 13:29:16	2008-01-02 13:29:18	2008-01-02 13:29:17	2008-01-01 23:59:59	NULL	NULL	2008-01-02 13:29:18	2008-01-02 13:29:16	2008-01-02 13:29:16	2008-01-02 13:29:18	0
-1	0
EXPECTED
)
expect_output \
    "core date_sub aliases second values" \
    "$core_expected" \
    "DO 0; SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"DATE_SUB(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND), "\
"DATE_SUB('2008-01-02 13:29:17', INTERVAL -1 SECOND), "\
"DATE_SUB('2008-01-02 13:29:17', INTERVAL 0 SECOND), "\
"DATE_SUB('2008-01-02', INTERVAL 1 SECOND), "\
"DATE_SUB(NULL, INTERVAL 1 SECOND), "\
"DATE_SUB('2008-01-02 13:29:17', INTERVAL NULL SECOND), "\
"ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"ADDDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND), "\
"SUBDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"SUBDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)	adddate_alias	subdate_alias
2008-01-02 13:29:16	2008-01-02 13:29:18	2008-01-02 13:29:16
EXPECTED
)
expect_output_with_headers \
    "date_sub aliases labels" \
    "$labels_expected" \
    "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS adddate_alias, "\
"SUBDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS subdate_alias FROM DUAL;" \
    "$DATABASE"

rollover_expected=$(cat <<EXPECTED
2024-02-28 23:59:59	2024-02-29 00:00:00
EXPECTED
)
expect_output \
    "subtraction leap rollover" \
    "$rollover_expected" \
    "SELECT DATE_SUB('2024-02-29 00:00:00', INTERVAL 1 SECOND), "\
"SUBDATE('2024-02-28 23:59:59', INTERVAL -1 SECOND);" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "date_sub aliases do status" \
    "$do_expected" \
    "DO DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "\
"ADDDATE(NULL, INTERVAL 1 SECOND), "\
"SUBDATE('2008-01-02 13:29:17', INTERVAL NULL SECOND); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

for function_name in DATE_SUB SUBDATE; do
    expect_error \
        "default sql mode rejects whitespace after ${function_name}" \
        1064 \
        "42000" \
        "syntax" \
        "SET SESSION sql_mode = ''; "\
"SELECT ${function_name} ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
        "$DATABASE"

    expect_output \
        "ignore_space accepts whitespace after ${function_name}" \
        "2008-01-02 13:29:16" \
        "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT ${function_name} ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
        "$DATABASE"
done

expect_error \
    "default sql mode rejects whitespace after ADDDATE" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = ''; "\
"SELECT ADDDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_output \
    "ignore_space accepts whitespace after ADDDATE" \
    "2008-01-02 13:29:18" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT ADDDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_error \
    "ansi quotes makes double quoted date an identifier" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; "\
"SELECT DATE_SUB(\"2008-01-02 13:29:17\", INTERVAL 1 SECOND);" \
    "$DATABASE"

expect_error \
    "default sql mode rejects date_sub identifier immediately followed by paren" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS date_sub; CREATE TABLE date_sub(id INT);" \
    "$DATABASE"

expect_output \
    "default sql mode allows date_sub identifier before spaced table definition" \
    "date_sub" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS date_sub; CREATE TABLE date_sub (id INT); "\
"SHOW TABLES LIKE 'date_sub'; DROP TABLE date_sub;" \
    "$DATABASE"

expect_error \
    "ignore_space reserves unquoted date_sub identifier" \
    1064 \
    "42000" \
    "syntax" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS \`date_sub\`; CREATE TABLE date_sub (id INT);" \
    "$DATABASE"

expect_output \
    "ignore_space allows quoted date_sub identifier" \
    "date_sub" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS \`date_sub\`; CREATE TABLE \`date_sub\` (id INT); "\
"SHOW TABLES LIKE 'date_sub'; DROP TABLE \`date_sub\`;" \
    "$DATABASE"

expect_output \
    "adddate and subdate remain identifiers under ignore_space" \
    "adddate
subdate" \
"SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"DROP TABLE IF EXISTS adddate; DROP TABLE IF EXISTS subdate; "\
"CREATE TABLE adddate (id INT); CREATE TABLE subdate (id INT); "\
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN ('adddate', 'subdate') "\
"ORDER BY TABLE_NAME; "\
"DROP TABLE adddate; DROP TABLE subdate;" \
    "$DATABASE"

expect_upstream_accepts \
    "non-interval ADDDATE days form accepted by MySQL but deferred by MyLite" \
    "SET SESSION sql_mode = ''; SELECT ADDDATE('2008-01-02', 31);" \
    "$DATABASE"

expect_upstream_accepts \
    "other date_sub interval unit accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 MINUTE);" \
    "$DATABASE"

expect_upstream_accepts \
    "date_sub interval expression accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1+1 SECOND);" \
    "$DATABASE"

expect_upstream_accepts \
    "date_sub string interval accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL '1' SECOND);" \
    "$DATABASE"

expect_upstream_accepts \
    "date_sub invalid date warning behavior accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('2016-07-00', INTERVAL 1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "date_sub lower-year warning behavior accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('1000-01-01 00:00:00', INTERVAL 1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "date_sub upper-year warning behavior accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_SUB('9999-12-31 23:59:59', INTERVAL -1 SECOND); SHOW WARNINGS;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_date_sub_second_aliases_expectations: ok"
