#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_merge_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_merge_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
[1, 2, true, false]	{"a": [1, 3, 5], "b": 2, "c": 4, "d": 6}	[{"a": 1}, 2]	{"a": {"x": 1, "y": 2}}
[true, false]	{"a": 1}	{"a": {"x": 1, "y": 2}}	null	NULL	NULL
EXPECTED
)
expect_output \
    "literal JSON merge values" \
    "$literal_expected" \
    "SELECT JSON_MERGE_PRESERVE('[1,2]', '[true,false]'), "\
"JSON_MERGE_PRESERVE('{\"a\":1,\"b\":2}', '{\"a\":3,\"c\":4}', "\
"'{\"a\":5,\"d\":6}'), JSON_MERGE_PRESERVE('{\"a\":1}', '2'), "\
"JSON_MERGE_PRESERVE('{\"a\":{\"x\":1}}', '{\"a\":{\"y\":2}}'); "\
"SELECT JSON_MERGE_PATCH('[1,2]', '[true,false]'), "\
"JSON_MERGE_PATCH('{\"a\":1,\"b\":2}', '{\"b\":null}'), "\
"JSON_MERGE_PATCH('{\"a\":{\"x\":1}}', '{\"a\":{\"y\":2}}'), "\
"JSON_MERGE_PATCH('{\"a\":1}', 'null'), JSON_MERGE_PATCH(NULL, '{\"a\":1}'), "\
"JSON_MERGE_PRESERVE(NULL, '{bad}');" \
    "$DATABASE"

constructor_expected=$(cat <<EXPECTED
{"a": [1, 2]}	{"a": 1, "b": 2}
EXPECTED
)
expect_output \
    "JSON merge constructor document arguments" \
    "$constructor_expected" \
    "SELECT JSON_MERGE_PRESERVE(JSON_OBJECT('a', 1), JSON_OBJECT('a', 2)), "\
"JSON_MERGE_PATCH(JSON_OBJECT('a', 1), JSON_OBJECT('b', 2));" \
    "$DATABASE"

warning_expected=$(cat <<EXPECTED
[1, true]
Warning	1287	'JSON_MERGE' is deprecated and will be removed in a future release. Please use JSON_MERGE_PRESERVE/JSON_MERGE_PATCH instead
EXPECTED
)
expect_output \
    "JSON_MERGE deprecated synonym warning" \
    "$warning_expected" \
    "SELECT JSON_MERGE('1', 'true'); SHOW WARNINGS;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	{"a": 1, "b": [2, 3], "c": 4}	{"a": 1, "b": 3, "c": 4}	3
2	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON merge projection" \
    "$table_expected" \
    "CREATE TABLE t("\
"id INT, j JSON, patch JSON, doc_text LONGTEXT, b VARBINARY(10), key_name VARCHAR(10)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1,\"b\":2}', '{\"b\":3,\"c\":4}', '{\"d\":4}', x'6162', 'k'), "\
"(2, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, JSON_MERGE_PRESERVE(j, patch), JSON_MERGE_PATCH(j, patch), "\
"JSON_EXTRACT(JSON_MERGE_PATCH(j, patch), '$.b') FROM t ORDER BY id;" \
    "$DATABASE"

short_circuit_expected=$(cat <<EXPECTED
1	{"a": 1, "b": 2, "k": 1}	{"a": 1, "b": 2, "k": 1}
2	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON merge first NULL short-circuit" \
    "$short_circuit_expected" \
    "SELECT id, JSON_MERGE_PRESERVE(j, JSON_OBJECT(key_name, 1)), "\
"JSON_MERGE(j, JSON_OBJECT(key_name, 1)) FROM t ORDER BY id;" \
    "$DATABASE"

updated_expected=$(cat <<EXPECTED
1	{"a": 1, "b": 2, "d": 4}
2	NULL
EXPECTED
)
expect_output \
    "JSON merge update assignment" \
    "$updated_expected" \
"UPDATE t SET j = JSON_MERGE_PATCH(j, doc_text) WHERE id = 1; "\
"SELECT id, j FROM t ORDER BY id;" \
    "$DATABASE"

update_warning_expected=$(cat <<EXPECTED
1	1
EXPECTED
)
expect_output \
    "JSON_MERGE update warning state" \
    "$update_warning_expected" \
    "UPDATE t SET j = JSON_MERGE(j, doc_text) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PATCH zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_MERGE_PATCH();" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PATCH one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_MERGE_PATCH('{\"a\":1}');" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PATCH invalid document" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_MERGE_PATCH('{bad}', '{\"a\":1}');" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PATCH validates non-null after NULL" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_MERGE_PATCH(NULL, '{bad}');" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PRESERVE validates document before NULL" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_MERGE_PRESERVE('{bad}', NULL);" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PATCH invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_MERGE_PATCH(1, '{\"a\":1}');" \
    "$DATABASE"

expect_error \
    "JSON_MERGE_PRESERVE binary document" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_MERGE_PRESERVE(CAST('{\"a\":1}' AS BINARY), '{\"b\":2}');" \
    "$DATABASE"

expect_error \
    "JSON_MERGE row invalid document text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "UPDATE t SET doc_text = '{bad}' WHERE id = 1; "\
"SELECT JSON_MERGE(j, doc_text) FROM t WHERE id = 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_merge_functions_expectations: ok"
