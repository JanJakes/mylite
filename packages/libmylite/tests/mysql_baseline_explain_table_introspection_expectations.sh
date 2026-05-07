#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_explain_table_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_explain_table_introspection_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE numbers(
       id INT NOT NULL,
       i INTEGER NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL,
       nn BIGINT UNSIGNED NOT NULL
     );" >/dev/null

expected_columns="Field	Type	Null	Key	Default	Extra"
expected_rows="id	int	NO		NULL	
i	int	YES		NULL	
iu	int unsigned	YES		NULL	
b	bigint	YES		NULL	
bu	bigint unsigned	YES		NULL	
nn	bigint unsigned	NO		NULL	"

check_explain_output() {
    label=$1
    sql=$2

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d')

    expect_value "$label headers" "$expected_columns" "$headers"
    expect_value "$label rows" "$expected_rows" "$rows"
}

check_explain_output "schema-qualified explain" "EXPLAIN ${DATABASE}.numbers;"
check_explain_output "default-schema explain" "USE ${DATABASE}; EXPLAIN numbers;"
status=$(run_mysql "USE ${DATABASE}; EXPLAIN numbers; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "explain status" "0	-1" "$status"

accepted_column_filter=$(run_mysql_with_headers "USE ${DATABASE}; EXPLAIN numbers id;")
expect_value \
    "accepted column filter headers" \
    "$expected_columns" \
    "$(printf '%s\n' "$accepted_column_filter" | sed -n '1p')"
expect_value \
    "accepted column filter row" \
    "id	int	NO		NULL	" \
    "$(printf '%s\n' "$accepted_column_filter" | sed -n '2p')"

accepted_wild_filter=$(run_mysql_with_headers "USE ${DATABASE}; EXPLAIN numbers 'i%';")
expect_value \
    "accepted wildcard first row" \
    "id" \
    "$(printf '%s\n' "$accepted_wild_filter" | awk -F '\t' '$1 == "id" { print $1; exit }')"
expect_value \
    "accepted wildcard second row" \
    "i" \
    "$(printf '%s\n' "$accepted_wild_filter" | awk -F '\t' '$1 == "i" { print $1; exit }')"

accepted_json=$(run_mysql_with_headers "EXPLAIN FORMAT=JSON SELECT 1;")
expect_value \
    "accepted format json header" \
    "EXPLAIN" \
    "$(printf '%s\n' "$accepted_json" | sed -n '1p')"

accepted_analyze=$(run_mysql_with_headers "EXPLAIN ANALYZE SELECT 1;")
expect_value \
    "accepted analyze header" \
    "EXPLAIN" \
    "$(printf '%s\n' "$accepted_analyze" | sed -n '1p')"

accepted_select=$(run_mysql_with_headers "EXPLAIN SELECT 1;")
expect_value \
    "accepted explain select header" \
    "id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra" \
    "$(printf '%s\n' "$accepted_select" | sed -n '1p')"

expect_error \
    "missing default schema explain" \
    1046 \
    3D000 \
    "No database selected" \
    "EXPLAIN numbers;"

expect_error \
    "unknown schema explain" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "EXPLAIN missing_schema.numbers;"

expect_error \
    "unknown table explain" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; EXPLAIN missing_table;"

expect_error \
    "trailing from schema explain" \
    1064 \
    42000 \
    "near 'FROM ${DATABASE}'" \
    "EXPLAIN ${DATABASE}.numbers FROM ${DATABASE};"

expect_error \
    "trailing in schema explain" \
    1064 \
    42000 \
    "near 'IN ${DATABASE}'" \
    "EXPLAIN ${DATABASE}.numbers IN ${DATABASE};"
