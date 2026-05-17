#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_extract_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_extract_functions_expectations: $1" >&2
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

extract_expected=$(cat <<EXPECTED
1	"x"	null	true	20	{"k": "v"}	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "literal JSON_EXTRACT values" \
    "$extract_expected" \
    "SELECT JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.a'), "\
"JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.b'), "\
"JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.c'), "\
"JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.d'), "\
"JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.e[1]'), "\
"JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],"\
"\"o\":{\"k\":\"v\"}}', '$.o'), "\
"JSON_EXTRACT('{\"a\":1}', '$.missing'), JSON_EXTRACT(NULL, '$.a'), "\
"JSON_EXTRACT('{\"a\":1}', NULL);" \
    "$DATABASE"

path_expected=$(cat <<EXPECTED
{"a": 1}	2	40	2
EXPECTED
)
expect_output \
    "simple path forms" \
    "$path_expected" \
    "SELECT JSON_EXTRACT('{\"a\":1}', '$'), "\
"JSON_EXTRACT('{\"a-b\":2}', '$.\"a-b\"'), "\
"JSON_EXTRACT('[10,20,[30,40]]', '\$[2][1]'), "\
"JSON_EXTRACT('{\"a\":{\"b\":[1,2]}}', '$.a.b[1]');" \
    "$DATABASE"

unquote_expected=$(cat <<EXPECTED
abc	abc	123	null	true	[1, 2]	{"a": 1}	NULL
EXPECTED
)
expect_output \
    "literal JSON_UNQUOTE values" \
    "$unquote_expected" \
    "SELECT JSON_UNQUOTE('\"abc\"'), JSON_UNQUOTE('abc'), JSON_UNQUOTE('123'), "\
"JSON_UNQUOTE('null'), JSON_UNQUOTE('true'), JSON_UNQUOTE('[1, 2]'), "\
"JSON_UNQUOTE('{\"a\": 1}'), JSON_UNQUOTE(NULL);" \
    "$DATABASE"

escape_expected=$(cat <<EXPECTED
0932
5C745C7530303332
EXPECTED
)
expect_output \
    "JSON_UNQUOTE escape decoding and SQL mode literal decoding" \
    "$escape_expected" \
    "SET @@sql_mode = ''; SELECT HEX(JSON_UNQUOTE('\"\\\\t\\\\u0032\"')); "\
"SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT HEX(JSON_UNQUOTE('\"\\\\t\\\\u0032\"'));" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	x	x	1	x	1
2	null	null	null	2	null	2
3	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_EXTRACT operators" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(128)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1,\"b\":\"x\"}', '{\"a\":1,\"b\":\"x\"}'), "\
"(2, '{\"a\":null,\"b\":\"null\"}', '{\"a\":2,\"b\":null}'), "\
"(3, NULL, NULL); "\
"SELECT id, JSON_EXTRACT(j, '$.a'), JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')), "\
"j->>'$.b', JSON_EXTRACT(s, '$.a'), JSON_UNQUOTE(JSON_EXTRACT(s, '$.b')), "\
"t.s->'$.a' FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_EXTRACT('{"a":1}', '$.a')	value
1	x
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_EXTRACT('{\"a\":1}', '$.a'), JSON_UNQUOTE('\"x\"') AS value FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON extraction status" \
    "$do_expected" \
    "DO JSON_EXTRACT('{\"a\":1}', '$.a'), JSON_UNQUOTE('\"x\"'), "\
"JSON_EXTRACT('{\"a\":1}', '$.missing'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_EXTRACT zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_EXTRACT();" \
    "$DATABASE"

expect_error \
    "JSON_EXTRACT one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_EXTRACT('{\"a\":1}');" \
    "$DATABASE"

expect_error \
    "JSON_EXTRACT invalid JSON" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_EXTRACT('bad', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_EXTRACT invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_EXTRACT('{\"a\":1}', 'bad');" \
    "$DATABASE"

expect_error \
    "JSON_EXTRACT numeric document" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_EXTRACT(1, '$');" \
    "$DATABASE"

expect_error \
    "JSON_UNQUOTE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_UNQUOTE();" \
    "$DATABASE"

expect_error \
    "JSON_UNQUOTE too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_UNQUOTE('\"a\"', '\"b\"');" \
    "$DATABASE"

expect_error \
    "JSON_UNQUOTE numeric argument" \
    3064 \
    "HY000" \
    "Incorrect type for argument" \
    "SELECT JSON_UNQUOTE(1);" \
    "$DATABASE"

expect_error \
    "JSON_UNQUOTE invalid JSON string" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'; SELECT JSON_UNQUOTE('\"\\u00ZZ\"');" \
    "$DATABASE"

expect_error \
    "operator literal left operand" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT '{\"a\":1}'->'$.a';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_extract_functions_expectations: ok"
