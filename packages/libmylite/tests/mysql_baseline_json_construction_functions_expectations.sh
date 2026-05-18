#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_construction_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_construction_functions_expectations: $1" >&2
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

array_expected=$(cat <<EXPECTED
[]	[1, "abc", null, true, false]	[-1, 2, 9223372036854775807]
-1	0
EXPECTED
)
expect_output \
    "literal JSON_ARRAY values" \
    "$array_expected" \
    "SELECT JSON_ARRAY(), JSON_ARRAY(1, 'abc', NULL, TRUE, FALSE), "\
"JSON_ARRAY(-1, +2, 9223372036854775807); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

escape_expected=$(cat <<'EXPECTED'
["a\\nb", "\"q\"", "\\t"]
["a\\\\nb", "\\\"q\\\"", "\\\\t"]
EXPECTED
)
expect_output \
    "JSON_ARRAY string escaping and SQL mode literal decoding" \
    "$escape_expected" \
    "SET @@sql_mode = ''; SELECT JSON_ARRAY('a\\\\nb', '\\\"q\\\"', '\\\\t'); "\
"SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT JSON_ARRAY('a\\\\nb', '\\\"q\\\"', '\\\\t');" \
    "$DATABASE"

object_expected=$(cat <<EXPECTED
{}	{"id": 87, "name": "carrot"}	{"a": 1, "b": "x", "f": false, "n": null, "t": true}
-1	0
EXPECTED
)
expect_output \
    "literal JSON_OBJECT values" \
    "$object_expected" \
    "SELECT JSON_OBJECT(), JSON_OBJECT('id', 87, 'name', 'carrot'), "\
"JSON_OBJECT('a', 1, 'b', 'x', 'n', NULL, 't', TRUE, 'f', FALSE); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

duplicate_key_expected=$(cat <<EXPECTED
{"a": 2}	{"1": false}
EXPECTED
)
expect_output \
    "JSON_OBJECT duplicate and nonstring keys" \
    "$duplicate_key_expected" \
    "SELECT JSON_OBJECT('a', 1, 'a', 2), JSON_OBJECT(1, 'x', TRUE, FALSE);" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	[1, "abc", {"a": 1}, 1, null]	{"b": 1, "j": {"a": 1}, "s": "abc", "id": 1}
2	[2, null, null, 0, null]	{"b": 0, "j": null, "s": null, "id": 2}
EXPECTED
)
expect_output \
    "table JSON construction values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, s VARCHAR(20), j JSON, b BOOLEAN); "\
"INSERT INTO t VALUES (1, 'abc', '{\"a\":1}', TRUE), (2, NULL, NULL, FALSE); "\
"SELECT id, JSON_ARRAY(id, s, j, b, NULL), "\
"JSON_OBJECT('id', id, 's', s, 'j', j, 'b', b) FROM t ORDER BY id;" \
    "$DATABASE"

descriptor_key_expected=$(cat <<EXPECTED
1	{"1": "abc"}	{"abc": 1}	{"1": "abc"}	{"abc": "abc"}	[]	{}
EXPECTED
)
expect_output \
    "table JSON_OBJECT descriptor keys" \
    "$descriptor_key_expected" \
    "SELECT id, JSON_OBJECT(id, s), JSON_OBJECT(s, id), JSON_OBJECT(b, s), "\
"JSON_OBJECT(s, id, s, s), JSON_ARRAY(), JSON_OBJECT() FROM t "\
"WHERE s IS NOT NULL ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
ja	jo
[1]	{"a": 1}
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_ARRAY(1) AS ja, JSON_OBJECT('a', 1) AS jo FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON construction status" \
    "$do_expected" \
    "DO JSON_ARRAY(1), JSON_OBJECT('a', 1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_OBJECT NULL key" \
    3158 \
    "22032" \
    "JSON documents may not contain NULL member names" \
    "SELECT JSON_OBJECT(NULL, 1);" \
    "$DATABASE"

expect_error \
    "JSON_OBJECT odd arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_OBJECT('a');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_construction_functions_expectations: ok"
