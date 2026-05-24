#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_keys_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_keys_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

literal_expected=$(cat <<EXPECTED
["a", "b"]	["c"]	[]	NULL	NULL	NULL	NULL	["a", "m", "z"]	["a"]
-1	0
EXPECTED
)
expect_output \
    "literal JSON_KEYS values" \
    "$literal_expected" \
    "SELECT JSON_KEYS('{\"a\":1,\"b\":{\"c\":30}}'), "\
"JSON_KEYS('{\"a\":1,\"b\":{\"c\":30}}', '$.b'), JSON_KEYS('{}'), "\
"JSON_KEYS('[]'), JSON_KEYS('{\"a\":1}', '$.missing'), "\
"JSON_KEYS('{\"a\":1}', '$.a'), JSON_KEYS(NULL), "\
"JSON_KEYS('{\"z\":1,\"a\":2,\"m\":3}'), JSON_KEYS('{\"a\":1,\"a\":2}'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

null_order_expected=$(cat <<EXPECTED
NULL	NULL
EXPECTED
)
expect_output \
    "NULL argument ordering" \
    "$null_order_expected" \
    "SELECT JSON_KEYS('{\"a\":1}', NULL), JSON_KEYS(NULL, 'bad');" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	["a", "b"]	["c"]	["x"]
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_KEYS values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, v VARCHAR(100)); "\
"INSERT INTO t VALUES (1, '{\"b\":{\"c\":1},\"a\":2}', '{\"x\":1}'), "\
"(2, NULL, NULL); "\
"SELECT id, JSON_KEYS(j), JSON_KEYS(JSON_EXTRACT(j,'$.b')), JSON_KEYS(v) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_KEYS('{"a":1}')	keys_value
["a"]	["b"]
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_KEYS('{\"a\":1}'), JSON_KEYS('{\"b\":2}') AS keys_value FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_KEYS status" \
    "$do_expected" \
    "DO JSON_KEYS('{\"a\":1}'), JSON_KEYS('{\"a\":{\"b\":2}}', '$.a'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_KEYS zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_KEYS();" \
    "$DATABASE"

expect_error \
    "JSON_KEYS many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_KEYS('{\"a\":1}', '$', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_KEYS invalid JSON" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_KEYS('bad');" \
    "$DATABASE"

expect_error \
    "JSON_KEYS invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_KEYS('{\"a\":1}', 'bad');" \
    "$DATABASE"

expect_error \
    "JSON_KEYS binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_KEYS(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_KEYS non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_KEYS(1);" \
    "$DATABASE"

expect_error \
    "JSON_KEYS wildcard path" \
    3149 \
    "42000" \
    "path expressions may not contain" \
    "SELECT JSON_KEYS('{\"a\":{\"b\":1}}', '$.*');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_keys_function_expectations: ok"
