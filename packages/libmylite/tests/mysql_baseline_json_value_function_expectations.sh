#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_value_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_value_function_expectations: $1" >&2
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

values_expected=$(cat <<EXPECTED
1	x	NULL	true	20	{"k": "v"}	NULL	NULL
EXPECTED
)
expect_output \
    "literal JSON_VALUE values" \
    "$values_expected" \
    "SELECT JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.a'), "\
"JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.b'), "\
"JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.c'), "\
"JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.d'), "\
"JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.e[1]'), "\
"JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.o'), "\
"JSON_VALUE('{\"a\":1}', '$.missing'), JSON_VALUE(NULL, '$.a');" \
    "$DATABASE"

path_expected=$(cat <<EXPECTED
{"a": 1}	40	2	1
EXPECTED
)
expect_output \
    "simple path forms" \
    "$path_expected" \
    "SELECT JSON_VALUE('{\"a\":1}', '$'), JSON_VALUE('[10,20,[30,40]]', '\$[2][1]'), "\
"JSON_VALUE('{\"a-b\":2}', '$.\"a-b\"'), JSON_VALUE('1', '$');" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	x	NULL	NULL	NULL	10	y	NULL	NULL
2	NULL	null	NULL	NULL	{"k": "v"}	20	NULL	NULL	{"k": "w"}
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_VALUE values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(128)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1,\"b\":\"x\",\"c\":null}', '{\"a\":10,\"b\":\"y\",\"c\":null}'), "\
"(2, '{\"a\":null,\"b\":\"null\",\"o\":{\"k\":\"v\"}}', "\
"'{\"a\":20,\"b\":null,\"o\":{\"k\":\"w\"}}'), "\
"(3, NULL, NULL); "\
"SELECT id, JSON_VALUE(j, '$.a'), JSON_VALUE(j, '$.b'), JSON_VALUE(j, '$.c'), "\
"JSON_VALUE(j, '$.missing'), JSON_VALUE(j, '$.o'), JSON_VALUE(s, '$.a'), JSON_VALUE(s, '$.b'), "\
"JSON_VALUE(s, '$.c'), JSON_VALUE(s, '$.o') FROM t ORDER BY id;" \
    "$DATABASE"

row_warning_expected=$(cat <<EXPECTED
1	NULL
2	1
3	NULL
Warning	3141	Invalid JSON text in argument 1 to function json_value: "Missing a name for object member." at position 1.
Warning	3141	Invalid JSON text in argument 1 to function json_value: "Missing a name for object member." at position 1.
2
EXPECTED
)
expect_output \
    "row JSON_VALUE invalid document warnings" \
    "$row_warning_expected" \
    "CREATE TABLE invalid_rows(id INT, s VARCHAR(20)); "\
"INSERT INTO invalid_rows VALUES (1,'{bad}'),(2,'{\"a\":1}'),(3,'{bad}'); "\
"SELECT id, JSON_VALUE(s, '$.a') FROM invalid_rows ORDER BY id; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_VALUE('{"a":1}', '$.a')	value
1	x
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_VALUE('{\"a\":1}', '$.a'), JSON_VALUE('{\"a\":\"x\"}', '$.a') AS value "\
"FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_VALUE status" \
    "$do_expected" \
    "DO JSON_VALUE('{\"a\":1}', '$.a'), JSON_VALUE('{\"a\":null}', '$.a'), "\
"JSON_VALUE('{\"a\":1}', '$.missing'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

warning_expected=$(cat <<EXPECTED
NULL
Warning	3141	Invalid JSON text in argument 1 to function json_value: "Missing a name for object member." at position 1.
1
EXPECTED
)
expect_output \
    "invalid JSON document warning" \
    "$warning_expected" \
    "SELECT JSON_VALUE('{bad}', '$.a'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

returning_expected=$(cat <<EXPECTED
x	123	123.00
EXPECTED
)
expect_output \
    "MySQL accepts deferred JSON_VALUE returning clauses" \
    "$returning_expected" \
    "SELECT JSON_VALUE('{\"a\":\"x\"}', '$.a' RETURNING CHAR), "\
"JSON_VALUE('{\"a\":123}', '$.a' RETURNING UNSIGNED), "\
"JSON_VALUE('{\"a\":123}', '$.a' RETURNING DECIMAL(5,2));" \
    "$DATABASE"

expect_error \
    "JSON_VALUE zero arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT JSON_VALUE();" \
    "$DATABASE"

expect_error \
    "JSON_VALUE one argument" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT JSON_VALUE('{\"a\":1}');" \
    "$DATABASE"

expect_error \
    "JSON_VALUE too many arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT JSON_VALUE('{\"a\":1}', '$.a', '$.b');" \
    "$DATABASE"

expect_error \
    "JSON_VALUE invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_VALUE('{\"a\":1}', '\$[');" \
    "$DATABASE"

expect_error \
    "JSON_VALUE numeric document" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_VALUE(1, '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_VALUE binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_VALUE(CAST('{\"a\":1}' AS BINARY), '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_VALUE numeric path syntax" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT JSON_VALUE('{\"a\":1}', 1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_value_function_expectations: ok"
