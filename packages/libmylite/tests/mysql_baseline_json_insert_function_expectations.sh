#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_insert_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_insert_function_expectations: $1" >&2
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
{"a": 1}	{"a": 1, "b": 2}	{"a": {"b": 1, "c": 2}}	{"a": 1}	{"a": 1}	{"a": 1}
[1, 2]	[1, 2]	[1, 2, 9]	[1, 2, 9]	[1, 2]
1	[1, 2]	"x"	["x", 2]
{"a": 1}	{"a": [1, 2]}	{"a": "[1,2]"}	STRING	ARRAY
{"a": 1}	NULL	NULL	{"a": 1, "b": null}
NULL	{}	NULL
-1	0
EXPECTED
)
expect_output \
    "literal JSON_INSERT values" \
    "$literal_expected" \
    "SELECT JSON_INSERT('{\"a\":1}', '$.a', 2), "\
"JSON_INSERT('{\"a\":1}', '$.b', 2), "\
"JSON_INSERT('{\"a\":{\"b\":1}}', '$.a.c', 2), "\
"JSON_INSERT('{\"a\":1}', '$.b.c', 2), "\
"JSON_INSERT('{}', '$.a', 1, '$.a', 2), "\
"JSON_INSERT('{\"a\":1}', '$', 2); "\
"SELECT JSON_INSERT('[1,2]', '\$[0]', 9), JSON_INSERT('[1,2]', '\$[1]', 9), "\
"JSON_INSERT('[1,2]', '\$[2]', 9), JSON_INSERT('[1,2]', '\$[99]', 9), "\
"JSON_INSERT('[1,2]', '\$[01]', 9); "\
"SELECT JSON_INSERT('1', '\$[0]', 2), JSON_INSERT('1', '\$[1]', 2), "\
"JSON_INSERT('\"x\"', '\$[0]', 2), JSON_INSERT('\"x\"', '\$[2]', 2); "\
"SELECT JSON_INSERT('{\"a\":1}', '$.a', JSON_EXTRACT('{\"x\":2}', '$.x')), "\
"JSON_INSERT('{}', '$.a', JSON_ARRAY(1,2)), JSON_INSERT('{}', '$.a', '[1,2]'), "\
"JSON_TYPE(JSON_EXTRACT(JSON_INSERT('{}', '$.a', '[1,2]'), '$.a')), "\
"JSON_TYPE(JSON_EXTRACT(JSON_INSERT('{}', '$.a', JSON_ARRAY(1,2)), '$.a')); "\
"SELECT JSON_INSERT('{\"a\":1}', '$.a', NULL), JSON_INSERT(NULL, '$.a', 1), "\
"JSON_INSERT('{\"a\":1}', NULL, 2), JSON_INSERT('{\"a\":1}', '$.b', NULL); "\
"SELECT JSON_INSERT('{}', NULL, 'a'), JSON_INSERT('{}', '$', NULL), "\
"JSON_INSERT('{}', '$.a', 1, NULL, 2); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
inserted	JSON_INSERT('{}','$.a',1)
{"a": 1}	{"a": 1}
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_INSERT('{}', '$.a', 1) AS inserted, "\
"JSON_INSERT('{}','$.a',1) FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_INSERT status" \
    "$do_expected" \
    "DO JSON_INSERT('{}', '$.a', 1), JSON_INSERT('{\"a\":1}', '$.a', 2); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	{"a": 1, "b": [2, 3], "c": "x"}	{"a": 1, "b": [2, 3], "c": "x", "i": 7}	{"a": 1, "b": [2, 3], "c": "x", "flag": 1}	{"a": 1, "b": [2, 3], "c": "x", "copy": {"a": 1, "b": [2, 3], "c": "x"}}	{"a": 1, "b": [2, 3], "c": "x", "label": "name"}	{"a": 1, "b": [2, 3], "c": "x", "doc": "{\"a\":1,\"b\":[2,3],\"c\":\"x\"}"}
2	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_INSERT values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, i INT, flag TINYINT, label VARCHAR(10), "\
"doc_text LONGTEXT, b VARBINARY(10)); "\
"INSERT INTO t VALUES (1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', 7, 1, 'name', "\
"'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), (2, NULL, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, JSON_INSERT(j, '$.a', 9), JSON_INSERT(j, '$.i', i), "\
"JSON_INSERT(j, '$.flag', flag), JSON_INSERT(j, '$.copy', j), "\
"JSON_INSERT(j, '$.label', label), JSON_INSERT(j, '$.doc', doc_text) FROM t ORDER BY id;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
1	{"a": 1, "b": [2, 3], "c": "x", "i": 7}
EXPECTED
)
expect_output \
    "table WHERE ORDER LIMIT JSON_INSERT values" \
    "$limited_expected" \
    "SELECT id, JSON_INSERT(j, '$.i', i) FROM t WHERE id >= 1 ORDER BY id LIMIT 1;" \
    "$DATABASE"

nested_expected=$(cat <<EXPECTED
1	7	name	ARRAY
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row-scalar JSON_INSERT composition" \
    "$nested_expected" \
    "SELECT id, JSON_EXTRACT(JSON_INSERT(j, '$.i', i), '$.i'), "\
"JSON_UNQUOTE(JSON_EXTRACT(JSON_INSERT(j, '$.label', label), '$.label')), "\
"JSON_TYPE(JSON_EXTRACT(JSON_INSERT(j, '$.arr', JSON_ARRAY(i)), '$.arr')) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_INSERT zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_INSERT();" \
    "$DATABASE"

expect_error \
    "JSON_INSERT one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_INSERT('{}');" \
    "$DATABASE"

expect_error \
    "JSON_INSERT missing value argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_INSERT('{}', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_INSERT invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_INSERT('{bad}', '$.a', 1);" \
    "$DATABASE"

expect_error \
    "JSON_INSERT invalid document before NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_INSERT('{bad}', NULL, 1);" \
    "$DATABASE"

expect_error \
    "JSON_INSERT invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_INSERT('{}', 'a', 1);" \
    "$DATABASE"

expect_error \
    "JSON_INSERT invalid path before NULL path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_INSERT('{}', 'a', 1, NULL, 2);" \
    "$DATABASE"

expect_error \
    "JSON_INSERT wildcard path" \
    3149 \
    "42000" \
    "path expressions may not contain the * and ** tokens" \
    "SELECT JSON_INSERT('{}', '$.*', 1);" \
    "$DATABASE"

expect_error \
    "JSON_INSERT invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_INSERT(1, '$.a', 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_insert_function_expectations: ok"
