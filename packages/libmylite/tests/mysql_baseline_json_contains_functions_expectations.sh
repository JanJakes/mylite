#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_contains_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_contains_functions_expectations: $1" >&2
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

contains_expected=$(cat <<EXPECTED
1	0	1	0	1	1	0	0
EXPECTED
)
expect_output \
    "literal JSON_CONTAINS scalar values" \
    "$contains_expected" \
    "SELECT JSON_CONTAINS('1','1'), JSON_CONTAINS('1','2'), "\
"JSON_CONTAINS('true','true'), JSON_CONTAINS('true','false'), "\
"JSON_CONTAINS('null','null'), JSON_CONTAINS('\"x\"','\"x\"'), "\
"JSON_CONTAINS('\"x\"','\"y\"'), JSON_CONTAINS('{\"a\":1}','{\"b\":1}');" \
    "$DATABASE"

array_object_expected=$(cat <<EXPECTED
1	1	1	1	1	0	0
EXPECTED
)
expect_output \
    "literal JSON_CONTAINS array and object values" \
    "$array_object_expected" \
    "SELECT JSON_CONTAINS('[1,2,3]','1'), JSON_CONTAINS('[1,2,3]','[1,3]'), "\
"JSON_CONTAINS('[1,2,3]','[3,1]'), "\
"JSON_CONTAINS('[1,{\"a\":2}]','{\"a\":2}'), "\
"JSON_CONTAINS('{\"a\":{\"b\":2,\"c\":3}}','{\"a\":{\"b\":2}}'), "\
"JSON_CONTAINS('{\"a\":1}','{\"a\":2}'), JSON_CONTAINS('{\"a\":1}','{\"b\":1}');" \
    "$DATABASE"

path_expected=$(cat <<EXPECTED
1	1	1	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "JSON_CONTAINS path and NULL behavior" \
    "$path_expected" \
    "SELECT JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','1','$.a'), "\
"JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','2','$.b'), "\
"JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','3','$.b[1]'), "\
"JSON_CONTAINS('{\"a\":1}','1','$.missing'), JSON_CONTAINS(NULL,'1'), "\
"JSON_CONTAINS('{\"a\":1}',NULL), JSON_CONTAINS('{\"a\":1}','1',NULL);" \
    "$DATABASE"

contains_path_expected=$(cat <<EXPECTED
1	1	0	0	1	1	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "JSON_CONTAINS_PATH values" \
    "$contains_path_expected" \
    "SELECT JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','one','$.a','$.x'), "\
"JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','all','$.a','$.b.c'), "\
"JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','all','$.a','$.x'), "\
"JSON_CONTAINS_PATH('{\"a\":1}','one','$.x'), "\
"JSON_CONTAINS_PATH('{\"a\":1}','ONE','$.a'), "\
"JSON_CONTAINS_PATH('{\"a\":1}','ALL','$.a'), "\
"JSON_CONTAINS_PATH(NULL,'one','bad'), "\
"JSON_CONTAINS_PATH('{\"a\":1}',NULL,'bad'), "\
"JSON_CONTAINS_PATH('{\"a\":1}','one',NULL), "\
"JSON_CONTAINS_PATH(NULL,NULL,NULL);" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	1	1	1
2	0	1	0	1
3	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_CONTAINS values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(128)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1,\"tags\":[\"red\",\"blue\"],\"o\":{\"k\":2}}', '{\"a\":1}'), "\
"(2, '{\"a\":2,\"tags\":[\"blue\"],\"o\":{\"k\":3}}', '{\"a\":2}'), "\
"(3, NULL, NULL); "\
"SELECT id, JSON_CONTAINS(j, '1', '$.a'), JSON_CONTAINS(j, '\"blue\"', '$.tags'), "\
"JSON_CONTAINS(s, '{\"a\":1}'), JSON_CONTAINS_PATH(j, 'one', '$.o.k') "\
"FROM t ORDER BY id;" \
    "$DATABASE"

where_expected=$(cat <<EXPECTED
1
2
1
2
EXPECTED
)
expect_output \
    "WHERE JSON contains filters" \
    "$where_expected" \
    "SELECT id FROM t WHERE JSON_CONTAINS(j, '\"blue\"', '$.tags') ORDER BY id; "\
"SELECT id FROM t WHERE JSON_CONTAINS_PATH(j, 'one', '$.o.k') ORDER BY id;" \
    "$DATABASE"

comparison_expected=$(cat <<EXPECTED
1
1
2
3
EXPECTED
)
expect_output \
    "WHERE JSON contains comparisons" \
    "$comparison_expected" \
    "SELECT id FROM t WHERE JSON_CONTAINS(j, '1', '$.a') = 1 ORDER BY id; "\
"SELECT id FROM t WHERE JSON_CONTAINS_PATH(j, 'one', '$.missing') = 0 ORDER BY id; "\
"SELECT id FROM t WHERE JSON_CONTAINS(j, '1', '$.a') IS NULL ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_CONTAINS('{"a":1}', '1', '$.a')	has_a
1	1
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_CONTAINS('{\"a\":1}', '1', '$.a'), "\
"JSON_CONTAINS_PATH('{\"a\":1}', 'one', '$.a') AS has_a FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON contains status" \
    "$do_expected" \
    "DO JSON_CONTAINS('{\"a\":1}', '1', '$.a'), "\
"JSON_CONTAINS_PATH('{\"a\":1}', 'one', '$.a'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_CONTAINS();" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_CONTAINS('{}');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS four arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_CONTAINS('{}','{}','$','$.a');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS invalid target" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_CONTAINS('bad','{}');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS invalid candidate" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_CONTAINS('{}','bad');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS numeric target" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_CONTAINS(1,'1');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS numeric candidate" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_CONTAINS('{}',1);" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_CONTAINS('{}','{}','bad');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS wildcard path" \
    3149 \
    "42000" \
    "path expressions may not contain" \
    "SELECT JSON_CONTAINS('{\"a\":[1]}','1','$.a[*]');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS binary target" \
    3144 \
    "22032" \
    "CHARACTER SET 'binary'" \
    "SELECT JSON_CONTAINS(CAST('{\"a\":1}' AS BINARY),'{}');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_CONTAINS_PATH();" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH two arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_CONTAINS_PATH('{}','one');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH invalid mode" \
    3154 \
    "42000" \
    "oneOrAll argument" \
    "SELECT JSON_CONTAINS_PATH('{\"a\":1}','some','$.a');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH invalid JSON" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_CONTAINS_PATH('bad','one','$.a');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH invalid JSON before invalid mode" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_CONTAINS_PATH('bad','some','bad');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH numeric document" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_CONTAINS_PATH(1,'one','$.a');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_CONTAINS_PATH('{\"a\":1}','one','bad');" \
    "$DATABASE"

expect_error \
    "JSON_CONTAINS_PATH binary document" \
    3144 \
    "22032" \
    "CHARACTER SET 'binary'" \
    "SELECT JSON_CONTAINS_PATH(CAST('{\"a\":1}' AS BINARY),'one','$.a');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_contains_functions_expectations: ok"
