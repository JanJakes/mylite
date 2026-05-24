#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_remove_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_remove_function_expectations: $1" >&2
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
{"b": 2}	{"a": {"c": 2}}	{"a": 1}	{"a": 1}	{"b": 2}	{"b": 2}
[2, 3]	[1, 3]	[1, 2, 3]	[1, 3]	[2]	1	"x"	1
{}	{"a": {}}	{"a": 1}
NULL	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "literal JSON_REMOVE values" \
    "$literal_expected" \
    "SELECT JSON_REMOVE('{\"a\":1,\"b\":2}', '$.a'), "\
"JSON_REMOVE('{\"a\":{\"b\":1,\"c\":2}}', '$.a.b'), "\
"JSON_REMOVE('{\"a\":1}', '$.b'), "\
"JSON_REMOVE('{\"a\":1}', '$.b.c'), "\
"JSON_REMOVE('{\"a\":1,\"b\":2}', '$.a', '$.a'), "\
"JSON_REMOVE('{\"a\":1,\"b\":2,\"c\":3}', '$.a', '$.c'); "\
"SELECT JSON_REMOVE('[1,2,3]', '\$[0]'), JSON_REMOVE('[1,2,3]', '\$[1]'), "\
"JSON_REMOVE('[1,2,3]', '\$[99]'), JSON_REMOVE('[1,2,3]', '\$[01]'), "\
"JSON_REMOVE('[1,2,3]', '\$[0]', '\$[1]'), JSON_REMOVE('1', '\$[0]'), "\
"JSON_REMOVE('\"x\"', '\$[0]'), JSON_REMOVE('1', '$.a'); "\
"SELECT JSON_REMOVE('{\"a\":1}', '\$[0].a'), "\
"JSON_REMOVE('{\"a\":{\"b\":1}}', '$.a[0].b'), JSON_REMOVE('{\"a\":1}', '\$[1].a'); "\
"SELECT JSON_REMOVE(NULL, '$.a'), JSON_REMOVE('{\"a\":1}', NULL), "\
"JSON_REMOVE('{}', NULL, 'a'), JSON_REMOVE('{}', '$', NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
removed	JSON_REMOVE('{"a":1}','$.a')
{}	{}
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_REMOVE('{\"a\":1}', '$.a') AS removed, "\
"JSON_REMOVE('{\"a\":1}','$.a') FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_REMOVE status" \
    "$do_expected" \
    "DO JSON_REMOVE('{\"a\":1}', '$.a'), JSON_REMOVE('{\"a\":1}', '$.b'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	{"b": [2, 3], "c": "x"}	{"a": 1, "b": [3], "c": "x"}	{"a": 1, "b": [2, 3]}	NULL
2	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_REMOVE values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, doc_text LONGTEXT, b VARBINARY(10)); "\
"INSERT INTO t VALUES (1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', "\
"'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), (2, NULL, NULL, NULL); "\
"SELECT id, JSON_REMOVE(j, '$.a'), JSON_REMOVE(j, '$.b[0]'), "\
"JSON_REMOVE(doc_text, '$.c'), JSON_REMOVE(j, NULL) FROM t ORDER BY id;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
1	{"b": [2, 3], "c": "x"}
EXPECTED
)
expect_output \
    "table WHERE ORDER LIMIT JSON_REMOVE values" \
    "$limited_expected" \
    "SELECT id, JSON_REMOVE(j, '$.a') FROM t WHERE id >= 1 ORDER BY id LIMIT 1;" \
    "$DATABASE"

nested_expected=$(cat <<EXPECTED
1	2	x	NULL
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row-scalar JSON_REMOVE composition" \
    "$nested_expected" \
    "SELECT id, JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.b[0]'), "\
"JSON_UNQUOTE(JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.c')), "\
"JSON_TYPE(JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.a')) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_REMOVE();" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_REMOVE('{}');" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE root path" \
    3153 \
    "42000" \
    "path expression '$' is not allowed" \
    "SELECT JSON_REMOVE('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_REMOVE('{bad}', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE invalid document before NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_REMOVE('{bad}', NULL);" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_REMOVE('{}', 'a');" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE invalid path before NULL path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_REMOVE('{}', 'a', NULL);" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE wildcard path" \
    3149 \
    "42000" \
    "path expressions may not contain the * and ** tokens" \
    "SELECT JSON_REMOVE('{}', '$.*');" \
    "$DATABASE"

expect_error \
    "JSON_REMOVE invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_REMOVE(1, '$.a');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_remove_function_expectations: ok"
