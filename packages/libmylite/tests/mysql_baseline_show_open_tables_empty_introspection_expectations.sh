#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_open_tables_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
TABLE_NAME="open_table_$$"

fail() {
    printf '%s\n' "mysql_baseline_show_open_tables_empty_introspection_expectations: $1" >&2
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

field_from_open_table_rows() {
    rows=$1
    schema_name=$2
    table_name=$3
    field_index=$4

    printf '%s\n' "$rows" \
        | awk -F '\t' -v schema="$schema_name" -v table="$table_name" -v field="$field_index" \
            '$1 == schema && $2 == table { print $field; exit }'
}

expect_open_table_field() {
    label=$1
    rows=$2
    schema_name=$3
    table_name=$4
    field_index=$5
    expected=$6

    actual=$(field_from_open_table_rows "$rows" "$schema_name" "$table_name" "$field_index")
    expect_value "$label" "$expected" "$actual"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

case "$(run_mysql 'SELECT @@lower_case_table_names;')" in
    0) ;;
    *) fail "expected @@lower_case_table_names=0 for SHOW OPEN TABLES LIKE probes" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     CREATE TABLE ${DATABASE}.${TABLE_NAME}(id INT) ENGINE=InnoDB;
     CREATE TABLE ${DATABASE}.other_table(id INT) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.${TABLE_NAME} VALUES (1), (2);
     SELECT * FROM ${DATABASE}.${TABLE_NAME};" \
    >/dev/null

expected_headers="Database	Table	In_use	Name_locked"

show_output=$(run_mysql_with_headers "SHOW OPEN TABLES FROM ${DATABASE} LIKE '${TABLE_NAME}';")
headers=$(printf '%s\n' "$show_output" | sed -n '1p')
rows=$(printf '%s\n' "$show_output" | sed '1d')
expect_value "show open tables headers" "$expected_headers" "$headers"
expect_open_table_field "open table database" "$rows" "$DATABASE" "$TABLE_NAME" 1 "$DATABASE"
expect_open_table_field "open table name" "$rows" "$DATABASE" "$TABLE_NAME" 2 "$TABLE_NAME"
expect_open_table_field "open table in use" "$rows" "$DATABASE" "$TABLE_NAME" 3 "0"
expect_open_table_field "open table name locked" "$rows" "$DATABASE" "$TABLE_NAME" 4 "0"

status=$(run_mysql "SHOW OPEN TABLES FROM ${DATABASE} LIKE '${TABLE_NAME}'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show open tables status" "0	-1" "$status"

bare_no_default=$(run_mysql "SHOW OPEN TABLES LIKE '${TABLE_NAME}';")
expect_open_table_field \
    "bare no-default global show open tables" \
    "$bare_no_default" \
    "$DATABASE" \
    "$TABLE_NAME" \
    2 \
    "$TABLE_NAME"

selected_output=$(run_mysql "USE ${DATABASE}; SHOW OPEN TABLES LIKE '${TABLE_NAME}';")
expect_open_table_field \
    "bare selected-schema global show open tables" \
    "$selected_output" \
    "$DATABASE" \
    "$TABLE_NAME" \
    2 \
    "$TABLE_NAME"

from_output=$(run_mysql "SHOW OPEN TABLES FROM ${DATABASE} LIKE '${TABLE_NAME}';")
expect_open_table_field "from like row name" "$from_output" "$DATABASE" "$TABLE_NAME" 2 "$TABLE_NAME"

in_output=$(run_mysql "SHOW OPEN TABLES IN ${DATABASE} LIKE 'open\\_table\\_%';")
expect_open_table_field "in like escaped row name" "$in_output" "$DATABASE" "$TABLE_NAME" 2 "$TABLE_NAME"

uppercase_output=$(run_mysql "SHOW OPEN TABLES FROM ${DATABASE} LIKE 'OPEN\\_TABLE\\_%';")
expect_value "case-sensitive show open tables like" "" "$uppercase_output"

empty_other=$(run_mysql "SHOW OPEN TABLES FROM ${OTHER_DATABASE};")
expect_value "empty explicit schema rows" "" "$empty_other"
status=$(run_mysql "SHOW OPEN TABLES FROM ${OTHER_DATABASE}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "empty explicit schema status" "0	-1" "$status"

unknown_output=$(run_mysql "SHOW OPEN TABLES FROM missing_show_open_tables_schema;")
expect_value "unknown explicit schema rows" "" "$unknown_output"
status=$(run_mysql "SHOW OPEN TABLES FROM missing_show_open_tables_schema; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "unknown explicit schema status" "0	-1" "$status"

unknown_in_output=$(run_mysql "SHOW OPEN TABLES IN missing_show_open_tables_schema LIKE 'missing%';")
expect_value "unknown explicit schema in rows" "" "$unknown_in_output"

where_output=$(run_mysql "SHOW OPEN TABLES FROM ${DATABASE} WHERE \`Table\` = '${TABLE_NAME}';")
expect_open_table_field "where accepted upstream" "$where_output" "$DATABASE" "$TABLE_NAME" 2 "$TABLE_NAME"

expect_error \
    "unsupported full show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FULL OPEN TABLES FROM ${DATABASE};"

expect_error \
    "unsupported extended show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EXTENDED OPEN TABLES FROM ${DATABASE};"

expect_error \
    "unsupported order show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} ORDER BY \`Table\`;"

expect_error \
    "unsupported limit show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIMIT 1;"

expect_error \
    "unsupported numeric like show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIKE 1;"

expect_error \
    "unsupported null like show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIKE NULL;"

expect_error \
    "unsupported national like show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIKE N'open%';"

expect_error \
    "unsupported introducer like show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIKE _utf8mb4'open%';"

expect_error \
    "unsupported combined like where show open tables" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW OPEN TABLES FROM ${DATABASE} LIKE 'open%' WHERE \`Table\` = '${TABLE_NAME}';"

printf '%s\n' "baseline-show-open-tables-empty-introspection MySQL 8.4.9 expectations verified"
