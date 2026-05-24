#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_replace_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_replace_function_expectations: $1" >&2
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
{"a": 10, "b": [2, 3]}	{"a": 1}	{"a": {"b": 2}}	{"a": 1}	{"a": 3}	2
[9, 2]	[1, 2]	[1, 2]	[1, 9]	2	1
{"a": 2}	{"a": [1, 2]}	{"a": "[1,2]"}	STRING	ARRAY
{"a": null}	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "literal JSON_REPLACE values" \
    "$literal_expected" \
    "SELECT JSON_REPLACE('{\"a\":1,\"b\":[2,3]}', '$.a', 10, '$.c', '[true,false]'), "\
"JSON_REPLACE('{\"a\":1}', '$.b', 2), "\
"JSON_REPLACE('{\"a\":{\"b\":1}}', '$.a.b', 2), "\
"JSON_REPLACE('{\"a\":1}', '$.b.c', 2), "\
"JSON_REPLACE('{\"a\":1}', '$.a', 2, '$.a', 3), "\
"JSON_REPLACE('{\"a\":1}', '$', 2); "\
"SELECT JSON_REPLACE('[1,2]', '\$[0]', 9), JSON_REPLACE('[1,2]', '\$[2]', 9), "\
"JSON_REPLACE('[1,2]', '\$[99]', 9), JSON_REPLACE('[1,2]', '\$[01]', 9), "\
"JSON_REPLACE('1', '\$[0]', 2), JSON_REPLACE('1', '\$[1]', 2); "\
"SELECT JSON_REPLACE('{\"a\":1}', '$.a', JSON_EXTRACT('{\"x\":2}', '$.x')), "\
"JSON_REPLACE('{\"a\":1}', '$.a', JSON_ARRAY(1,2)), "\
"JSON_REPLACE('{\"a\":1}', '$.a', '[1,2]'), "\
"JSON_TYPE(JSON_EXTRACT(JSON_REPLACE('{\"a\":1}', '$.a', '[1,2]'), '$.a')), "\
"JSON_TYPE(JSON_EXTRACT(JSON_REPLACE('{\"a\":1}', '$.a', JSON_ARRAY(1,2)), '$.a')); "\
"SELECT JSON_REPLACE('{\"a\":1}', '$.a', NULL), JSON_REPLACE(NULL, '$.a', 1), "\
"JSON_REPLACE('{\"a\":1}', NULL, 2); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
changed	JSON_REPLACE('{"a":0}','$.a',1)
{"a": 1}	{"a": 1}
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_REPLACE('{\"a\":0}', '$.a', 1) AS changed, "\
"JSON_REPLACE('{\"a\":0}','$.a',1) FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_REPLACE status" \
    "$do_expected" \
    "DO JSON_REPLACE('{\"a\":1}', '$.a', 2), "\
"JSON_REPLACE('{\"a\":1}', '$.a', NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	{"b": 0, "i": 0, "j": null, "s": "[1,2]", "x": ""}	{"b": 0, "i": 7, "j": null, "s": 0, "x": ""}	{"b": 1, "i": 0, "j": null, "s": 0, "x": ""}	{"b": 0, "i": 0, "j": {"b": 0, "i": 0, "j": null, "s": 0, "x": ""}, "s": 0, "x": ""}	{"b": 0, "i": 0, "j": null, "s": 0, "x": "row"}
2	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_REPLACE values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(100), i INT, b BOOLEAN, label VARCHAR(20)); "\
"INSERT INTO t VALUES (1, '{\"s\":0,\"i\":0,\"b\":0,\"j\":null,\"x\":\"\"}', "\
"'[1,2]', 7, TRUE, 'row'), (2, NULL, NULL, NULL, FALSE, NULL); "\
"SELECT id, JSON_REPLACE(j, '$.s', s), JSON_REPLACE(j, '$.i', i), "\
"JSON_REPLACE(j, '$.b', b), JSON_REPLACE(j, '$.j', j), "\
"JSON_REPLACE(j, '$.x', label) FROM t ORDER BY id;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
1	{"b": 0, "i": 7, "j": null, "s": 0, "x": ""}
EXPECTED
)
expect_output \
    "table WHERE ORDER LIMIT JSON_REPLACE values" \
    "$limited_expected" \
    "SELECT id, JSON_REPLACE(j, '$.i', i) FROM t WHERE id >= 1 ORDER BY id LIMIT 1;" \
    "$DATABASE"

nested_expected=$(cat <<EXPECTED
1	7	row	ARRAY
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row-scalar JSON_REPLACE composition" \
    "$nested_expected" \
    "SELECT id, JSON_EXTRACT(JSON_REPLACE(j, '$.i', i), '$.i'), "\
"JSON_UNQUOTE(JSON_EXTRACT(JSON_REPLACE(j, '$.x', label), '$.x')), "\
"JSON_TYPE(JSON_EXTRACT(JSON_REPLACE(j, '$.j', JSON_ARRAY(i)), '$.j')) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_REPLACE();" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_REPLACE('{}');" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE missing value argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_REPLACE('{}', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_REPLACE('{bad}', '$.a', 1);" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_REPLACE('{}', 'a', 1);" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE wildcard path" \
    3149 \
    "42000" \
    "path expressions may not contain the * and ** tokens" \
    "SELECT JSON_REPLACE('{}', '$.*', 1);" \
    "$DATABASE"

expect_error \
    "JSON_REPLACE invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_REPLACE(1, '$.a', 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_replace_function_expectations: ok"
