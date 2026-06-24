#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_overlaps_member_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_overlaps_member_functions_expectations: $1" >&2
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

overlaps_scalar_expected=$(cat <<EXPECTED
1	0	1	1	1	0	0
EXPECTED
)
expect_output \
    "literal JSON_OVERLAPS scalar and empty values" \
    "$overlaps_scalar_expected" \
    "SELECT JSON_OVERLAPS('1','1'), JSON_OVERLAPS('1','2'), "\
"JSON_OVERLAPS('true','true'), JSON_OVERLAPS('null','null'), "\
"JSON_OVERLAPS('[1]', '1'), JSON_OVERLAPS('[]','[]'), JSON_OVERLAPS('{}','{}');" \
    "$DATABASE"

overlaps_container_expected=$(cat <<EXPECTED
1	1	1	1	0	0	1
EXPECTED
)
expect_output \
    "literal JSON_OVERLAPS arrays and objects" \
    "$overlaps_container_expected" \
    "SELECT JSON_OVERLAPS('[1,2]','[2,3]'), "\
"JSON_OVERLAPS('[1,{\"a\":2}]','[{\"a\":2}]'), "\
"JSON_OVERLAPS('{\"a\":1}','{\"a\":1,\"b\":2}'), "\
"JSON_OVERLAPS('{\"a\":1}', '[{\"a\":1}]'), "\
"JSON_OVERLAPS('{\"a\":1}','{\"a\":2}'), "\
"JSON_OVERLAPS('{\"a\":{\"b\":2}}','{\"a\":{\"b\":2,\"c\":3}}'), "\
"JSON_OVERLAPS('{\"a\":{\"b\":2}}','{\"a\":{\"b\":2}}');" \
    "$DATABASE"

overlaps_null_expected=$(cat <<EXPECTED
NULL	NULL	NULL
EXPECTED
)
expect_output \
    "JSON_OVERLAPS NULL behavior" \
    "$overlaps_null_expected" \
    "SELECT JSON_OVERLAPS(NULL, 'bad'), JSON_OVERLAPS('{\"a\":1}', NULL), "\
"JSON_OVERLAPS(NULL, NULL);" \
    "$DATABASE"

member_expected=$(cat <<EXPECTED
1	1	0	0	0	0	0	1	1	1	1	0	NULL	NULL
EXPECTED
)
expect_output \
    "literal MEMBER OF values" \
    "$member_expected" \
    "SELECT 17 MEMBER OF('[23, \"abc\", 17, \"ab\", 10]'), "\
"'ab' MEMBER OF('[23, \"abc\", 17, \"ab\", 10]'), "\
"7 MEMBER OF('[23, \"abc\", 17, \"ab\", 10]'), "\
"'a' MEMBER OF('[23, \"abc\", 17, \"ab\", 10]'), "\
"17 MEMBER OF('[23, \"abc\", \"17\", \"ab\", 10]'), "\
"'17' MEMBER OF('[23, \"abc\", 17, \"ab\", 10]'), "\
"'[4,5]' MEMBER OF('[[4,5]]'), JSON_ARRAY(4,5) MEMBER OF('[[4,5]]'), "\
"JSON_OBJECT('a',1) MEMBER OF('[{\"a\":1}]'), 1 MEMBER OF('1'), "\
"'a' MEMBER OF('\"a\"'), 1 MEMBER OF('[]'), "\
"NULL MEMBER OF('bad'), 1 MEMBER OF(NULL);" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	1	1	1
2	0	1	0	1
3	NULL	NULL	NULL	0
EXPECTED
)
expect_output \
    "table JSON_OVERLAPS and MEMBER OF values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(128)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1,\"tags\":[\"red\",\"blue\"],\"o\":{\"k\":2}}', 'blue'), "\
"(2, '{\"a\":2,\"tags\":[\"green\",\"blue\"],\"o\":{\"k\":3}}', 'green'), "\
"(3, NULL, NULL); "\
"SELECT id, JSON_OVERLAPS(j, '{\"a\":1}'), "\
"JSON_OVERLAPS(JSON_EXTRACT(j, '$.tags'), '[\"blue\"]'), "\
"s MEMBER OF('[\"blue\",\"red\"]'), "\
"id MEMBER OF('[1,2]') "\
"FROM t ORDER BY id;" \
    "$DATABASE"

where_expected=$(cat <<EXPECTED
1
1
1
2
3
EXPECTED
)
expect_output \
    "WHERE JSON_OVERLAPS and MEMBER OF filters" \
    "$where_expected" \
    "SELECT id FROM t WHERE JSON_OVERLAPS(j, '{\"a\":1}') ORDER BY id; "\
"SELECT id FROM t WHERE s MEMBER OF('[\"blue\",\"red\"]') ORDER BY id; "\
"SELECT id FROM t WHERE JSON_OVERLAPS(j, '{\"missing\":1}') = 0 ORDER BY id; "\
"SELECT id FROM t WHERE JSON_OVERLAPS(j, '{\"missing\":1}') IS NULL ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_OVERLAPS('{"a":1}', '{"a":1}')	has_blue
1	1
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_OVERLAPS('{\"a\":1}', '{\"a\":1}'), "\
"'blue' MEMBER OF('[\"blue\"]') AS has_blue FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_OVERLAPS and MEMBER OF status" \
    "$do_expected" \
    "DO JSON_OVERLAPS('{\"a\":1}', '{\"a\":1}'), 'blue' MEMBER OF('[\"blue\"]'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_OVERLAPS();" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_OVERLAPS('{}');" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS three arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_OVERLAPS('{}','{}','{}');" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid first document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_OVERLAPS('bad','{}');" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid second document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_OVERLAPS('{}','bad');" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid first document before NULL second document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_OVERLAPS('bad', NULL);" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid first document before invalid second type" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_OVERLAPS('bad', 1);" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid first type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data in argument 1" \
    "SELECT JSON_OVERLAPS(1,'1');" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS invalid second type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data in argument 2" \
    "SELECT JSON_OVERLAPS('{}',1);" \
    "$DATABASE"

expect_error \
    "JSON_OVERLAPS binary document" \
    3144 \
    "22032" \
    "CHARACTER SET 'binary'" \
    "SELECT JSON_OVERLAPS(CAST('{\"a\":1}' AS BINARY),'{}');" \
    "$DATABASE"

expect_error \
    "MEMBER OF numeric right type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data in argument 2" \
    "SELECT 1 MEMBER OF(1);" \
    "$DATABASE"

expect_error \
    "MEMBER OF invalid right document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT 1 MEMBER OF('bad');" \
    "$DATABASE"

expect_error \
    "MEMBER OF binary right document" \
    3144 \
    "22032" \
    "CHARACTER SET 'binary'" \
    "SELECT 1 MEMBER OF(CAST('[1]' AS BINARY));" \
    "$DATABASE"

expect_error \
    "MEMBER OF malformed empty call" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT 1 MEMBER OF();" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_overlaps_member_functions_expectations: ok"
