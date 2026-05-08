#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_processlist_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_show_processlist_introspection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

current_query_row() {
    rows=$1
    info=$2

    printf '%s\n' "$rows" \
        | awk -F '\t' -v info="$info" '$2 == "root" && $5 == "Query" && $8 == info { print; exit }'
}

current_query_info_with_prefix() {
    rows=$1
    prefix=$2

    printf '%s\n' "$rows" \
        | awk -F '\t' -v prefix="$prefix" \
            '$2 == "root" && $5 == "Query" && index($8, prefix) == 1 { print length($8) "\t" $8; exit }'
}

field_from_row() {
    row=$1
    field_index=$2

    printf '%s\n' "$row" | awk -F '\t' -v field="$field_index" '{ print $field; exit }'
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_value \
    "default processlist implementation mode" \
    "0" \
    "$(run_mysql 'SELECT @@performance_schema_show_processlist;')"

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expected_headers="Id	User	Host	db	Command	Time	State	Info"
headers=$(run_mysql_with_headers "SHOW PROCESSLIST;" | sed -n '1p')
expect_value "show processlist headers" "$expected_headers" "$headers"

bare_rows=$(run_mysql "SHOW PROCESSLIST;")
bare_row=$(current_query_row "$bare_rows" "SHOW PROCESSLIST")
if [ -z "$bare_row" ]; then
    fail "show processlist current root query row not found in [$bare_rows]"
fi
case "$(field_from_row "$bare_row" 1)" in
    ''|*[!0-9]*) fail "show processlist id is not decimal: [$bare_row]" ;;
esac
case "$(field_from_row "$bare_row" 3)" in
    *:*) ;;
    *) fail "show processlist host did not include a TCP client port: [$bare_row]" ;;
esac
expect_value "show processlist db without default" "NULL" "$(field_from_row "$bare_row" 4)"
expect_value "show processlist command" "Query" "$(field_from_row "$bare_row" 5)"
expect_value "show processlist time" "0" "$(field_from_row "$bare_row" 6)"
expect_value "show processlist state" "init" "$(field_from_row "$bare_row" 7)"

selected_rows=$(run_mysql "USE ${DATABASE}; SHOW PROCESSLIST;")
selected_row=$(current_query_row "$selected_rows" "SHOW PROCESSLIST")
if [ -z "$selected_row" ]; then
    fail "selected-schema show processlist current root query row not found in [$selected_rows]"
fi
expect_value "show processlist selected db" "$DATABASE" "$(field_from_row "$selected_row" 4)"

full_rows=$(run_mysql "SHOW FULL PROCESSLIST;")
full_row=$(current_query_row "$full_rows" "SHOW FULL PROCESSLIST")
if [ -z "$full_row" ]; then
    fail "show full processlist current root query row not found in [$full_rows]"
fi
expect_value "show full processlist db without default" "NULL" "$(field_from_row "$full_row" 4)"

commented_info="/* lead */ SHOW FULL PROCESSLIST /* trail */"
commented_rows=$(run_mysql "${commented_info};")
commented=$(current_query_info_with_prefix "$commented_rows" "/* lead */ SHOW FULL PROCESSLIST")
expect_value \
    "show full processlist leading and trailing comments" \
    "$(printf '%s\t%s' "$(printf '%s' "$commented_info" | wc -c | tr -d ' ')" "$commented_info")" \
    "$commented"

long_comment=$(printf 'x%.0s' $(seq 1 160))
long_nonfull_rows=$(run_mysql "SHOW /* ${long_comment} */ PROCESSLIST;")
long_nonfull=$(current_query_info_with_prefix "$long_nonfull_rows" "SHOW /* ")
expected_nonfull_prefix=$(printf '100\tSHOW /* ')
case "$long_nonfull" in
    "$expected_nonfull_prefix"*) ;;
    *) fail "non-FULL processlist Info was not truncated to 100 bytes: [$long_nonfull]" ;;
esac

long_full_rows=$(run_mysql "SHOW FULL /* ${long_comment} */ PROCESSLIST;")
long_full=$(current_query_info_with_prefix "$long_full_rows" "SHOW FULL /* ")
expected_full_info="SHOW FULL /* ${long_comment} */ PROCESSLIST"
expect_value \
    "show full processlist untruncated Info" \
    "$(printf '%s\t%s' "$(printf '%s' "$expected_full_info" | wc -c | tr -d ' ')" "$expected_full_info")" \
    "$long_full"

status=$(run_mysql "SHOW PROCESSLIST; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show processlist warnings and row count" "1	-1" "$status"

full_status=$(run_mysql "SHOW FULL PROCESSLIST; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show full processlist warnings and row count" "1	-1" "$full_status"

warnings=$(run_mysql "SHOW PROCESSLIST; SHOW WARNINGS;" | tail -n 1)
expect_value \
    "show processlist deprecation warning" \
    "Warning	1287	'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. Please use performance_schema.processlist instead" \
    "$warnings"

expect_error \
    "unsupported processlist like" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST LIKE 'root%';"

expect_error \
    "unsupported processlist where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST WHERE Id > 0;"

expect_error \
    "unsupported processlist order" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST ORDER BY Id;"

expect_error \
    "unsupported processlist limit" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST LIMIT 1;"

expect_error \
    "unsupported full processlist limit" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FULL PROCESSLIST LIMIT 1;"

expect_error \
    "unsupported processlist from" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST FROM mysql;"

expect_error \
    "unsupported processlist in" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCESSLIST IN mysql;"

expect_error \
    "unsupported extended processlist" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EXTENDED PROCESSLIST;"
