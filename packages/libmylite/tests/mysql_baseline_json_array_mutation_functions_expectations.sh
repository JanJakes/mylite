#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_array_mutation_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_array_mutation_functions_expectations: $1" >&2
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

append_expected=$(cat <<EXPECTED
["a", ["b", "c", 1], "d"]	[["a", 2], ["b", "c"], "d"]	["a", [["b", 3], "c"], "d"]
{"a": 1, "b": [2, 3, "x"], "c": 4}	{"a": 1, "b": [2, 3], "c": [4, "y"]}	[{"a": 1}, "z"]
[1, 2]	[1, 2]	{"a": 1}	{"a": [1, 9]}	[{"a": 1}, 9]	[1, 9]
NULL	NULL	{"a": [1, null]}	NULL
EXPECTED
)
expect_output \
    "literal JSON_ARRAY_APPEND values" \
    "$append_expected" \
    "SELECT JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '\$[1]', 1), "\
"JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '\$[0]', 2), "\
"JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '\$[1][0]', 3); "\
"SELECT JSON_ARRAY_APPEND('{\"a\":1,\"b\":[2,3],\"c\":4}', '$.b', 'x'), "\
"JSON_ARRAY_APPEND('{\"a\":1,\"b\":[2,3],\"c\":4}', '$.c', 'y'), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '$', 'z'); "\
"SELECT JSON_ARRAY_APPEND('[1,2]', '\$[5]', 9), "\
"JSON_ARRAY_APPEND('[1,2]', '$.missing', 9), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '$.missing', 9), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '$.a[0]', 9), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '\$[0]', 9), "\
"JSON_ARRAY_APPEND('1', '\$[0][0]', 9); "\
"SELECT JSON_ARRAY_APPEND(NULL, '$', 1), "\
"JSON_ARRAY_APPEND('{\"a\":1}', NULL, 1), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '$.a', NULL), "\
"JSON_ARRAY_APPEND('{\"a\":1}', '$.a', 1, NULL, 2, 'bad', 3);" \
    "$DATABASE"

insert_expected=$(cat <<EXPECTED
["a", "x", {"b": [1, 2]}, [3, 4]]	["a", {"b": [1, 2]}, [3, 4], "x"]	["a", {"b": ["x", 1, 2]}, [3, 4]]	["a", {"b": [1, 2]}, [3, "y", 4]]
["x", "a", {"b": [1, 2]}, [3, 4]]	{"a": 1}	{"a": 1}	{"a": 1}	[1]
NULL	NULL	[null, 1]	NULL
EXPECTED
)
expect_output \
    "literal JSON_ARRAY_INSERT values" \
    "$insert_expected" \
    "SELECT JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '\$[1]', 'x'), "\
"JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '\$[100]', 'x'), "\
"JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '\$[1].b[0]', 'x'), "\
"JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '\$[2][1]', 'y'); "\
"SELECT JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '\$[0]', 'x', "\
"'\$[2][1]', 'y'), JSON_ARRAY_INSERT('{\"a\":1}', '$.a[0]', 9), "\
"JSON_ARRAY_INSERT('{\"a\":1}', '$.missing[0]', 9), "\
"JSON_ARRAY_INSERT('{\"a\":1}', '\$[0]', 9), "\
"JSON_ARRAY_INSERT('[1]', '\$[0][0]', 9); "\
"SELECT JSON_ARRAY_INSERT(NULL, '\$[0]', 1), "\
"JSON_ARRAY_INSERT('[1]', NULL, 1), "\
"JSON_ARRAY_INSERT('[1]', '\$[0]', NULL), "\
"JSON_ARRAY_INSERT('[1]', '\$[0]', 1, NULL, 2, 'bad', 3);" \
    "$DATABASE"

headers_expected=$(cat <<EXPECTED
appended	JSON_ARRAY_INSERT('[1]', '$[0]', 2)
[1, 2]	[2, 1]
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$headers_expected" \
    "SELECT JSON_ARRAY_APPEND('[1]', '$', 2) AS appended, "\
"JSON_ARRAY_INSERT('[1]', '\$[0]', 2) FROM DUAL;" \
    "$DATABASE"

status_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON array mutation status" \
    "$status_expected" \
    "DO JSON_ARRAY_APPEND('[1]', '$', 2), JSON_ARRAY_INSERT('[1]', '\$[0]', 2); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	{"a": [1, 7], "b": [2, 3], "c": "x"}	{"a": 1, "b": [2, 7, 3], "c": "x"}	{"a": 1, "b": [2, 3, "name"], "c": "x"}	{"a": 1, "b": [2, {"a": 1, "b": [2, 3], "c": "x"}, 3], "c": "x"}	{"a": 1, "b": [2, "{\"a\":1,\"b\":[2,3],\"c\":\"x\"}", 3], "c": "x"}
2	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON array mutation values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, i INT, label VARCHAR(10), doc_text LONGTEXT, "\
"b VARBINARY(10)); "\
"INSERT INTO t VALUES (1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', 7, 'name', "\
"'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), "\
"(2, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, JSON_ARRAY_APPEND(j, '$.a', i), JSON_ARRAY_INSERT(j, '$.b[1]', i), "\
"JSON_ARRAY_APPEND(j, '$.b', label), JSON_ARRAY_INSERT(j, '$.b[1]', j), "\
"JSON_ARRAY_INSERT(j, '$.b[1]', doc_text) FROM t ORDER BY id;" \
    "$DATABASE"

nested_expected=$(cat <<EXPECTED
1	7	ARRAY	7
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row-scalar JSON array mutation composition" \
    "$nested_expected" \
    "SELECT id, JSON_EXTRACT(JSON_ARRAY_APPEND(j, '$.a', i), '$.a[1]'), "\
"JSON_TYPE(JSON_EXTRACT(JSON_ARRAY_APPEND(j, '$.b', JSON_ARRAY(i)), '$.b[2]')), "\
"JSON_EXTRACT(JSON_ARRAY_INSERT(j, '$.b[1]', i), '$.b[1]') FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_APPEND();" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND missing value argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_APPEND('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND dangling path argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_APPEND('{}', '$', 1, '$');" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_ARRAY_APPEND('bad', '$', 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND invalid document before NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_ARRAY_APPEND('bad', NULL, 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND invalid path before NULL path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_ARRAY_APPEND('{\"a\":1}', 'bad', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND wildcard path before NULL path" \
    3149 \
    "42000" \
    "path expressions may not contain" \
    "SELECT JSON_ARRAY_APPEND('{\"a\":1}', '$.*', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_APPEND invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_ARRAY_APPEND(1, '$', 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_INSERT();" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT missing value argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_INSERT('[1]', '$[0]');" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT dangling path argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_ARRAY_INSERT('[1]', '$[0]', 1, '$[1]');" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_ARRAY_INSERT('bad', '$[0]', 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT invalid document before NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_ARRAY_INSERT('bad', NULL, 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT invalid path before NULL path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_ARRAY_INSERT('[1]', 'bad', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT wildcard path before NULL path" \
    3149 \
    "42000" \
    "path expressions may not contain" \
    "SELECT JSON_ARRAY_INSERT('[1]', '$.*', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT root path" \
    3165 \
    "42000" \
    "A path expression is not a path to a cell in an array" \
    "SELECT JSON_ARRAY_INSERT('{}', '$', 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT member path" \
    3165 \
    "42000" \
    "A path expression is not a path to a cell in an array" \
    "SELECT JSON_ARRAY_INSERT('{}', '$.a', 1);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT member path before NULL path" \
    3165 \
    "42000" \
    "A path expression is not a path to a cell in an array" \
    "SELECT JSON_ARRAY_INSERT('[1]', '$', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_ARRAY_INSERT invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_ARRAY_INSERT(1, '$[0]', 1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_array_mutation_functions_expectations: ok"
