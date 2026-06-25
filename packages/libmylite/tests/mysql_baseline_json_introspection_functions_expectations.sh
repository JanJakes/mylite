#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_introspection_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_introspection_functions_expectations: $1" >&2
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

type_expected=$(cat <<EXPECTED
NULL	OBJECT	ARRAY	BOOLEAN	BOOLEAN	NULL	STRING	INTEGER	INTEGER	DOUBLE	DOUBLE
-1	0
EXPECTED
)
expect_output \
    "literal JSON_TYPE values" \
    "$type_expected" \
    "SELECT JSON_TYPE(NULL), JSON_TYPE('{}'), JSON_TYPE('[]'), JSON_TYPE('true'), "\
"JSON_TYPE('false'), JSON_TYPE('null'), JSON_TYPE('\"x\"'), JSON_TYPE('1'), "\
"JSON_TYPE('-1'), JSON_TYPE('1.5'), JSON_TYPE('1e2'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

length_expected=$(cat <<EXPECTED
NULL	1	1	1	3	2	1	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "literal JSON_LENGTH values" \
    "$length_expected" \
    "SELECT JSON_LENGTH(NULL), JSON_LENGTH('1'), JSON_LENGTH('null'), "\
"JSON_LENGTH('\"x\"'), JSON_LENGTH('[1,2,{\"a\":3}]'), "\
"JSON_LENGTH('{\"a\":1,\"b\":{\"c\":30}}'), "\
"JSON_LENGTH('{\"a\":1,\"b\":{\"c\":30}}', '$.b'), "\
"JSON_LENGTH('{\"a\":1}', '$.missing'), JSON_LENGTH('{\"a\":1}', NULL), "\
"JSON_LENGTH(NULL, 'bad');" \
    "$DATABASE"

depth_expected=$(cat <<EXPECTED
NULL	1	1	1	1	1	2	2	3	4
EXPECTED
)
expect_output \
    "literal JSON_DEPTH values" \
    "$depth_expected" \
    "SELECT JSON_DEPTH(NULL), JSON_DEPTH('{}'), JSON_DEPTH('[]'), JSON_DEPTH('true'), "\
"JSON_DEPTH('123'), JSON_DEPTH('\"x\"'), JSON_DEPTH('[10,20]'), "\
"JSON_DEPTH('[[],{}]'), JSON_DEPTH('[10,{\"a\":20}]'), "\
"JSON_DEPTH('{\"a\":{\"b\":[1]}}');" \
    "$DATABASE"

storage_expected=$(cat <<EXPECTED
NULL	5	5	13	14	5	NULL	0	0
EXPECTED
)
expect_output \
    "literal JSON storage values" \
    "$storage_expected" \
    "SELECT JSON_STORAGE_SIZE(NULL), JSON_STORAGE_SIZE('{}'), JSON_STORAGE_SIZE('[]'), "\
"JSON_STORAGE_SIZE('{\"a\":1}'), JSON_STORAGE_SIZE('[1,2,3]'), "\
"JSON_STORAGE_SIZE('\"abc\"'), JSON_STORAGE_FREE(NULL), JSON_STORAGE_FREE('{}'), "\
"JSON_STORAGE_FREE(JSON_SET('{\"a\":1}', '$.a', 2));" \
    "$DATABASE"

pretty_expected=$(cat <<EXPECTED
313233	7B0A20202261223A20322C0A20202262223A20310A7D	5B0A2020312C0A20207B0A202020202261223A205B0A202020202020747275652C0A20202020202066616C73652C0A2020202020206E756C6C2C0A2020202020202278220A202020205D0A20207D0A5D	7B7D	NULL
EXPECTED
)
expect_output \
    "literal JSON_PRETTY values" \
    "$pretty_expected" \
    "SELECT HEX(JSON_PRETTY('123')), HEX(JSON_PRETTY('{\"b\":1,\"a\":2}')), "\
"HEX(JSON_PRETTY('[1,{\"a\":[true,false,null,\"x\"]}]')), "\
"HEX(JSON_PRETTY('{}')), HEX(JSON_PRETTY(NULL));" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	OBJECT	2	3	43	0	13	0	ARRAY	2	2	11	0	23	0	NULL
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON introspection values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, v VARCHAR(100)); "\
"INSERT INTO t VALUES (1, '{\"a\":[10,true],\"b\":null}', '{\"x\":[1,2]}'), "\
"(2, NULL, NULL); "\
"UPDATE t SET j = '{\"a\":[10,true],\"b\":{\"c\":3}}' WHERE id = 1; "\
"SELECT id, JSON_TYPE(j), JSON_LENGTH(j), JSON_DEPTH(j), "\
"JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j), "\
"JSON_STORAGE_SIZE(JSON_EXTRACT(j,'$.b')), JSON_STORAGE_FREE(JSON_EXTRACT(j,'$.b')), "\
"JSON_TYPE(JSON_EXTRACT(j,'$.a')), JSON_LENGTH(JSON_EXTRACT(j,'$.a')), "\
"JSON_DEPTH(JSON_EXTRACT(j,'$.a')), "\
"JSON_STORAGE_SIZE(JSON_EXTRACT(j,'$.a')), JSON_STORAGE_FREE(JSON_EXTRACT(j,'$.a')), "\
"JSON_STORAGE_SIZE(v), JSON_STORAGE_FREE(v), JSON_DEPTH(NULL) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
jt	jl	jd	jss	jsf	jp
OBJECT	2	3	13	0	7B0A20202261223A20310A7D
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_TYPE('{\"a\":1}') AS jt, JSON_LENGTH('{\"a\":1,\"b\":2}') AS jl "\
" , JSON_DEPTH('[1,[2]]') AS jd, JSON_STORAGE_SIZE('{\"a\":1}') AS jss, "\
"JSON_STORAGE_FREE('{\"a\":1}') AS jsf, HEX(JSON_PRETTY('{\"a\":1}')) AS jp "\
"FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON introspection status" \
    "$do_expected" \
    "DO JSON_TYPE('{\"a\":1}'), JSON_LENGTH('[1,2]'), "\
"JSON_STORAGE_SIZE('{\"a\":1}'), JSON_STORAGE_FREE('{\"a\":1}'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_TYPE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_TYPE();" \
    "$DATABASE"

expect_error \
    "JSON_TYPE many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_TYPE('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_LENGTH();" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_LENGTH('{}', '$', '$.a');" \
    "$DATABASE"

expect_error \
    "JSON_DEPTH zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_DEPTH();" \
    "$DATABASE"

expect_error \
    "JSON_DEPTH many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_DEPTH('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_SIZE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_STORAGE_SIZE();" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_SIZE many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_STORAGE_SIZE('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_FREE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_STORAGE_FREE();" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_FREE many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_STORAGE_FREE('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_PRETTY zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_PRETTY();" \
    "$DATABASE"

expect_error \
    "JSON_PRETTY many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_PRETTY('{}', '$');" \
    "$DATABASE"

expect_error \
    "JSON_TYPE non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_TYPE(1);" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_LENGTH(1);" \
    "$DATABASE"

expect_error \
    "JSON_DEPTH non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_DEPTH(1);" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_SIZE non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_STORAGE_SIZE(1);" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_FREE non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_STORAGE_FREE(1);" \
    "$DATABASE"

expect_error \
    "JSON_PRETTY non-json scalar" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_PRETTY(1);" \
    "$DATABASE"

expect_error \
    "JSON_TYPE invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_TYPE('bad');" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_LENGTH('bad');" \
    "$DATABASE"

expect_error \
    "JSON_DEPTH invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_DEPTH('bad');" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_SIZE invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_STORAGE_SIZE('bad');" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_FREE invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_STORAGE_FREE('bad');" \
    "$DATABASE"

expect_error \
    "JSON_PRETTY invalid text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_PRETTY('bad');" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH invalid text with NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_LENGTH('bad', NULL);" \
    "$DATABASE"

expect_error \
    "JSON_TYPE binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_TYPE(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_LENGTH(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_DEPTH binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_DEPTH(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_SIZE binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_STORAGE_SIZE(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_STORAGE_FREE binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_STORAGE_FREE(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_PRETTY binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_PRETTY(CAST('{\"a\":1}' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_LENGTH invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_LENGTH('{\"a\":1}', 'bad');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_introspection_functions_expectations: ok"
