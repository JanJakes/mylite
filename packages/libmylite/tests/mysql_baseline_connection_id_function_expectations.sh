#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_connection_id_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_connection_id_function_expectations: $1" >&2
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

assert_positive_integer() {
    label=$1
    value=$2

    case "$value" in
        ''|*[!0-9]*) fail "$label: expected decimal integer, got [$value]" ;;
        0) fail "$label: expected nonzero connection id" ;;
    esac
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

same_connection=$(run_mysql 'DO 0; SELECT CONNECTION_ID(), CONNECTION_ID(), @@warning_count;')
first_id=$(printf '%s\n' "$same_connection" | cut -f1)
second_id=$(printf '%s\n' "$same_connection" | cut -f2)
warning_count=$(printf '%s\n' "$same_connection" | cut -f3)
assert_positive_integer "first connection id" "$first_id"
if [ "$first_id" != "$second_id" ]; then
    fail "same connection returned different ids [$first_id] and [$second_id]"
fi
if [ "$warning_count" != "0" ]; then
    fail "expected zero warnings, got [$warning_count]"
fi

headers_output=$(run_mysql_with_headers \
    'SELECT connection_id(), Connection_Id(), CONNECTION_ID (), CONNECTION_ID/**/(), CONNECTION_ID(/* inside */), (CONNECTION_ID());'
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expected_headers='connection_id()	Connection_Id()	CONNECTION_ID ()	CONNECTION_ID/**/ ()	CONNECTION_ID(/* inside */ )	(CONNECTION_ID())'
if [ "$headers" != "$expected_headers" ]; then
    fail "unexpected column labels: [$headers]"
fi
label_id=$(printf '%s\n' "$values" | cut -f1)
assert_positive_integer "label connection id" "$label_id"
for field in 2 3 4 5 6; do
    value=$(printf '%s\n' "$values" | cut -f"$field")
    if [ "$value" != "$label_id" ]; then
        fail "label query returned inconsistent id in field $field: [$values]"
    fi
done

from_dual=$(run_mysql 'SELECT CONNECTION_ID() FROM DUAL;')
assert_positive_integer "from dual connection id" "$from_dual"

selected_database=$(run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT CONNECTION_ID(), DATABASE();"
)
selected_id=$(printf '%s\n' "$selected_database" | cut -f1)
selected_schema=$(printf '%s\n' "$selected_database" | cut -f2)
assert_positive_integer "selected database connection id" "$selected_id"
if [ "$selected_schema" != "$DATABASE" ]; then
    fail "expected selected schema [$DATABASE], got [$selected_schema]"
fi

table_backed=$(run_mysql \
    "USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1),(2); DO 0; SELECT CONNECTION_ID() FROM t ORDER BY id; SELECT ROW_COUNT(), @@warning_count;"
)
table_first=$(printf '%s\n' "$table_backed" | sed -n '1p')
table_second=$(printf '%s\n' "$table_backed" | sed -n '2p')
row_count=$(printf '%s\n' "$table_backed" | sed -n '3p' | cut -f1)
table_warning_count=$(printf '%s\n' "$table_backed" | sed -n '3p' | cut -f2)
assert_positive_integer "table-backed first connection id" "$table_first"
if [ "$table_first" != "$table_second" ]; then
    fail "table-backed connection ids differed: [$table_backed]"
fi
if [ "$row_count" != "-1" ] || [ "$table_warning_count" != "0" ]; then
    fail "unexpected row count / warnings after table-backed select: [$table_backed]"
fi

first_file=$(mktemp)
second_file=$(mktemp)
(
    run_mysql 'SELECT CONNECTION_ID(); DO SLEEP(2);' >"$first_file"
) &
first_pid=$!
sleep 1
run_mysql 'SELECT CONNECTION_ID();' >"$second_file"
wait "$first_pid"
concurrent_first=$(sed -n '1p' "$first_file")
concurrent_second=$(sed -n '1p' "$second_file")
rm -f "$first_file" "$second_file"
assert_positive_integer "concurrent first connection id" "$concurrent_first"
assert_positive_integer "concurrent second connection id" "$concurrent_second"
if [ "$concurrent_first" = "$concurrent_second" ]; then
    fail "concurrent connections returned the same id [$concurrent_first]"
fi

expect_error \
    "connection id function rejects integer argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID(1);"

expect_error \
    "connection id function rejects null argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID(NULL);"

expect_error \
    "connection id function rejects string argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID('x');"

expect_error \
    "connection id function rejects multiple arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID(1, 2);"

expect_error \
    "mixed bad scalar arguments report first connection id expression" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID(1), VERSION(1);"

expect_error \
    "mixed bad scalar arguments report first version expression" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'VERSION'" \
    "SELECT VERSION(1), CONNECTION_ID(1);"

expect_error \
    "bare connection id is not an information function" \
    1054 \
    42S22 \
    "Unknown column 'CONNECTION_ID'" \
    "SELECT CONNECTION_ID;"

limit_value=$(run_mysql 'SELECT CONNECTION_ID() LIMIT 1;')
assert_positive_integer "limit connection id" "$limit_value"
