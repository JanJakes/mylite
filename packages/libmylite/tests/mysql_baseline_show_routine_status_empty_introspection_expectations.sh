#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_routine_status_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
PROCEDURE_NAME="routine_proc_$$"
FUNCTION_NAME="routine_func_$$"

fail() {
    printf '%s\n' "mysql_baseline_show_routine_status_empty_introspection_expectations: $1" >&2
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

field_from_routine_rows() {
    rows=$1
    schema_name=$2
    routine_name=$3
    field_index=$4

    printf '%s\n' "$rows" \
        | awk -F '\t' -v schema="$schema_name" -v routine="$routine_name" -v field="$field_index" \
            '$1 == schema && $2 == routine { print $field; exit }'
}

expect_routine_field() {
    label=$1
    rows=$2
    schema_name=$3
    routine_name=$4
    field_index=$5
    expected=$6

    actual=$(field_from_routine_rows "$rows" "$schema_name" "$routine_name" "$field_index")
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
    *) fail "expected @@lower_case_table_names=0 for SHOW routine status LIKE probes" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     CREATE PROCEDURE ${DATABASE}.${PROCEDURE_NAME}() SELECT 1;
     CREATE PROCEDURE ${DATABASE}.MixedProcCase() SELECT 1;
     CREATE FUNCTION ${DATABASE}.${FUNCTION_NAME}() RETURNS INT DETERMINISTIC RETURN 1;
     CREATE FUNCTION ${DATABASE}.MixedFuncCase() RETURNS INT DETERMINISTIC RETURN 1;" \
    >/dev/null

expected_headers="Db	Name	Type	Language	Definer	Modified	Created	Security_type	Comment	character_set_client	collation_connection	Database Collation"

procedure_output=$(run_mysql_with_headers "SHOW PROCEDURE STATUS LIKE '${PROCEDURE_NAME}';")
procedure_headers=$(printf '%s\n' "$procedure_output" | sed -n '1p')
procedure_rows=$(printf '%s\n' "$procedure_output" | sed '1d')
expect_value "show procedure status headers" "$expected_headers" "$procedure_headers"
expect_routine_field "procedure db" "$procedure_rows" "$DATABASE" "$PROCEDURE_NAME" 1 "$DATABASE"
expect_routine_field "procedure name" "$procedure_rows" "$DATABASE" "$PROCEDURE_NAME" 2 "$PROCEDURE_NAME"
expect_routine_field "procedure type" "$procedure_rows" "$DATABASE" "$PROCEDURE_NAME" 3 "PROCEDURE"
expect_routine_field "procedure language" "$procedure_rows" "$DATABASE" "$PROCEDURE_NAME" 4 "SQL"
expect_routine_field "procedure security type" "$procedure_rows" "$DATABASE" "$PROCEDURE_NAME" 8 "DEFINER"

function_output=$(run_mysql_with_headers "SHOW FUNCTION STATUS LIKE '${FUNCTION_NAME}';")
function_headers=$(printf '%s\n' "$function_output" | sed -n '1p')
function_rows=$(printf '%s\n' "$function_output" | sed '1d')
expect_value "show function status headers" "$expected_headers" "$function_headers"
expect_routine_field "function db" "$function_rows" "$DATABASE" "$FUNCTION_NAME" 1 "$DATABASE"
expect_routine_field "function name" "$function_rows" "$DATABASE" "$FUNCTION_NAME" 2 "$FUNCTION_NAME"
expect_routine_field "function type" "$function_rows" "$DATABASE" "$FUNCTION_NAME" 3 "FUNCTION"
expect_routine_field "function language" "$function_rows" "$DATABASE" "$FUNCTION_NAME" 4 "SQL"
expect_routine_field "function security type" "$function_rows" "$DATABASE" "$FUNCTION_NAME" 8 "DEFINER"

status=$(run_mysql "SHOW PROCEDURE STATUS LIKE 'missing%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "empty procedure status" "0	-1" "$status"

status=$(run_mysql "SHOW FUNCTION STATUS LIKE 'missing%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "empty function status" "0	-1" "$status"

bare_no_default=$(run_mysql "SHOW PROCEDURE STATUS LIKE '${PROCEDURE_NAME}';")
expect_routine_field \
    "bare no-default procedure status" \
    "$bare_no_default" \
    "$DATABASE" \
    "$PROCEDURE_NAME" \
    2 \
    "$PROCEDURE_NAME"

selected_other=$(run_mysql "USE ${OTHER_DATABASE}; SHOW PROCEDURE STATUS LIKE '${PROCEDURE_NAME}';")
expect_routine_field \
    "selected schema does not restrict procedure status" \
    "$selected_other" \
    "$DATABASE" \
    "$PROCEDURE_NAME" \
    2 \
    "$PROCEDURE_NAME"

selected_function=$(run_mysql "USE ${OTHER_DATABASE}; SHOW FUNCTION STATUS LIKE '${FUNCTION_NAME}';")
expect_routine_field \
    "selected schema does not restrict function status" \
    "$selected_function" \
    "$DATABASE" \
    "$FUNCTION_NAME" \
    2 \
    "$FUNCTION_NAME"

escaped_procedure=$(run_mysql "SHOW PROCEDURE STATUS LIKE 'routine\\_proc\\_%';")
expect_routine_field "escaped procedure like" "$escaped_procedure" "$DATABASE" "$PROCEDURE_NAME" 2 "$PROCEDURE_NAME"

escaped_function=$(run_mysql "SHOW FUNCTION STATUS LIKE 'routine\\_func\\_%';")
expect_routine_field "escaped function like" "$escaped_function" "$DATABASE" "$FUNCTION_NAME" 2 "$FUNCTION_NAME"

uppercase_procedure=$(run_mysql "SHOW PROCEDURE STATUS LIKE 'ROUTINE\\_PROC\\_%';")
expect_routine_field "case-insensitive procedure like" "$uppercase_procedure" "$DATABASE" "$PROCEDURE_NAME" 2 "$PROCEDURE_NAME"

uppercase_function=$(run_mysql "SHOW FUNCTION STATUS LIKE 'ROUTINE\\_FUNC\\_%';")
expect_routine_field "case-insensitive function like" "$uppercase_function" "$DATABASE" "$FUNCTION_NAME" 2 "$FUNCTION_NAME"

where_procedure=$(run_mysql "SHOW PROCEDURE STATUS WHERE Db = '${DATABASE}' AND Name = '${PROCEDURE_NAME}';")
expect_routine_field "procedure where accepted upstream" "$where_procedure" "$DATABASE" "$PROCEDURE_NAME" 2 "$PROCEDURE_NAME"

where_function=$(run_mysql "SHOW FUNCTION STATUS WHERE Db = '${DATABASE}' AND Name = '${FUNCTION_NAME}';")
expect_routine_field "function where accepted upstream" "$where_function" "$DATABASE" "$FUNCTION_NAME" 2 "$FUNCTION_NAME"

expect_error \
    "unsupported procedure from" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCEDURE STATUS FROM ${DATABASE};"

expect_error \
    "unsupported function in" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FUNCTION STATUS IN ${DATABASE};"

expect_error \
    "unsupported full procedure status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FULL PROCEDURE STATUS;"

expect_error \
    "unsupported extended function status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EXTENDED FUNCTION STATUS;"

expect_error \
    "unsupported order procedure status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCEDURE STATUS ORDER BY Name;"

expect_error \
    "unsupported limit function status" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FUNCTION STATUS LIMIT 1;"

expect_error \
    "unsupported numeric procedure like" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCEDURE STATUS LIKE 1;"

expect_error \
    "unsupported null function like" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FUNCTION STATUS LIKE NULL;"

expect_error \
    "unsupported national procedure like" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCEDURE STATUS LIKE N'routine%';"

expect_error \
    "unsupported charset function like" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FUNCTION STATUS LIKE _utf8mb4'routine%';"

expect_error \
    "unsupported combined procedure like where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW PROCEDURE STATUS LIKE 'routine%' WHERE Name = '${PROCEDURE_NAME}';"

