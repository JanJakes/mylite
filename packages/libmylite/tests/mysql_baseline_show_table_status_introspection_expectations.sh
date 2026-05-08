#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_table_status_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_table_status_introspection_expectations: $1" >&2
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

field_from_rows() {
    rows=$1
    table_name=$2
    field_index=$3

    printf '%s\n' "$rows" | awk -F '\t' -v table="$table_name" -v field="$field_index" '$1 == table { print $field; exit }'
}

expect_table_field() {
    label=$1
    rows=$2
    table_name=$3
    field_index=$4
    expected=$5

    actual=$(field_from_rows "$rows" "$table_name" "$field_index")
    expect_value "$label" "$expected" "$actual"
}

expect_table_field_not_null() {
    label=$1
    rows=$2
    table_name=$3
    field_index=$4

    actual=$(field_from_rows "$rows" "$table_name" "$field_index")
    if [ -z "$actual" ] || [ "$actual" = "NULL" ]; then
        fail "$label: expected non-NULL value, got [$actual]"
    fi
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

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE empty_numbers(id INT NOT NULL, i INT NULL) ENGINE=InnoDB;
     CREATE TABLE numbers(id INT NOT NULL, i INT NULL) ENGINE=InnoDB;
     INSERT INTO numbers VALUES (1, NULL), (2, 20), (3, 30);
     CREATE VIEW v_numbers AS SELECT id, i FROM numbers;
     CREATE TABLE ${OTHER_DATABASE}.only_other(id INT) ENGINE=InnoDB;" >/dev/null

expected_columns="Name	Engine	Version	Row_format	Rows	Avg_row_length	Data_length	Max_data_length	Index_length	Data_free	Auto_increment	Create_time	Update_time	Check_time	Collation	Checksum	Create_options	Comment"

show_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW TABLE STATUS;")
headers=$(printf '%s\n' "$show_output" | sed -n '1p')
rows=$(printf '%s\n' "$show_output" | sed '1d')
expect_value "show table status headers" "$expected_columns" "$headers"

expect_table_field "empty engine" "$rows" "empty_numbers" 2 "InnoDB"
expect_table_field "empty version" "$rows" "empty_numbers" 3 "10"
expect_table_field "empty row format" "$rows" "empty_numbers" 4 "Dynamic"
expect_table_field "empty rows" "$rows" "empty_numbers" 5 "0"
expect_table_field "empty average length" "$rows" "empty_numbers" 6 "0"
expect_table_field "empty data length" "$rows" "empty_numbers" 7 "16384"
expect_table_field "empty max data length" "$rows" "empty_numbers" 8 "0"
expect_table_field "empty index length" "$rows" "empty_numbers" 9 "0"
expect_table_field "empty data free" "$rows" "empty_numbers" 10 "0"
expect_table_field "empty auto increment" "$rows" "empty_numbers" 11 "NULL"
expect_table_field_not_null "empty create time" "$rows" "empty_numbers" 12
expect_table_field "empty update time" "$rows" "empty_numbers" 13 "NULL"
expect_table_field "empty check time" "$rows" "empty_numbers" 14 "NULL"
expect_table_field "empty collation" "$rows" "empty_numbers" 15 "utf8mb4_0900_ai_ci"
expect_table_field "empty checksum" "$rows" "empty_numbers" 16 "NULL"

expect_table_field "numbers engine" "$rows" "numbers" 2 "InnoDB"
expect_table_field "numbers rows" "$rows" "numbers" 5 "3"
expect_table_field "numbers average length" "$rows" "numbers" 6 "5461"
expect_table_field "numbers data length" "$rows" "numbers" 7 "16384"
expect_table_field_not_null "numbers create time" "$rows" "numbers" 12
expect_table_field_not_null "numbers update time" "$rows" "numbers" 13

expect_table_field "view engine" "$rows" "v_numbers" 2 "NULL"
expect_table_field "view comment" "$rows" "v_numbers" 18 "VIEW"

status=$(run_mysql "USE ${DATABASE}; SHOW TABLE STATUS; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show table status status" "0	-1" "$status"

from_output=$(run_mysql_with_headers "SHOW TABLE STATUS FROM ${DATABASE} LIKE 'empty%';")
from_rows=$(printf '%s\n' "$from_output" | sed '1d')
expect_value "from like row name" "empty_numbers" "$(field_from_rows "$from_rows" "empty_numbers" 1)"

in_output=$(run_mysql_with_headers "SHOW TABLE STATUS IN ${OTHER_DATABASE} LIKE 'only\\_%';")
in_rows=$(printf '%s\n' "$in_output" | sed '1d')
expect_value "in like escaped row name" "only_other" "$(field_from_rows "$in_rows" "only_other" 1)"

no_match_status=$(run_mysql "USE ${DATABASE}; SHOW TABLE STATUS LIKE 'missing%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "no-match like status" "0	-1" "$no_match_status"
no_match_rows=$(run_mysql "USE ${DATABASE}; SHOW TABLE STATUS LIKE 'missing%';")
expect_value "no-match like rows" "" "$no_match_rows"

where_output=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name = 'numbers';")
expect_value "where accepted row name" "numbers" "$(field_from_rows "$where_output" "numbers" 1)"

expect_error \
    "missing default schema show table status" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW TABLE STATUS;"

expect_error \
    "unknown schema show table status from" \
    1049 \
    42000 \
    "Unknown database 'missing_show_table_status_schema'" \
    "SHOW TABLE STATUS FROM missing_show_table_status_schema;"

expect_error \
    "unknown schema show table status in" \
    1049 \
    42000 \
    "Unknown database 'missing_show_table_status_schema'" \
    "SHOW TABLE STATUS IN missing_show_table_status_schema;"

expect_error \
    "unsupported full show table status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FULL TABLE STATUS;"

expect_error \
    "unsupported numeric like show table status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} LIKE 1;"

expect_error \
    "unsupported null like show table status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} LIKE NULL;"

expect_error \
    "unsupported national like show table status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} LIKE N'a%';"

expect_error \
    "unsupported misplaced like show table status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS LIKE 'a%' FROM ${DATABASE};"

printf '%s\n' "baseline-show-table-status-introspection MySQL 8.4.9 expectations verified"
