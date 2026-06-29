#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_aggregate_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_aggregate_functions_expectations: $1" >&2
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

no_source_expected=$(cat <<EXPECTED
[null]	{"a": 1}
EXPECTED
)
expect_output \
    "no-source JSON aggregates" \
    "$no_source_expected" \
    "SELECT JSON_ARRAYAGG(NULL), JSON_OBJECTAGG('a', 1);" \
    "$DATABASE"

seed_sql="CREATE TABLE t ("\
"g INT, id INT, k VARCHAR(20), s VARCHAR(20), j JSON, b TINYINT"\
"); INSERT INTO t VALUES "\
"(1, 1, 'a', 'alpha', JSON_OBJECT('x', 1), 1),"\
"(1, 2, 'b', 'beta', JSON_ARRAY(2), 0),"\
"(1, 3, 'a', NULL, NULL, NULL),"\
"(2, 4, 'c', 'carrot', JSON_OBJECT('x', 4), 1),"\
"(3, 5, 'n', NULL, NULL, NULL);"

source_expected=$(cat <<EXPECTED
["alpha", "beta", null]	[{"x": 1}, [2], null]	[null, null, null]	{"k": 3}
EXPECTED
)
expect_output \
    "source JSON aggregates" \
    "$source_expected" \
    "$seed_sql SELECT JSON_ARRAYAGG(s), JSON_ARRAYAGG(j), JSON_ARRAYAGG(NULL), "\
"JSON_OBJECTAGG('k', id) FROM t WHERE g = 1;" \
    "$DATABASE"

grouped_expected=$(cat <<EXPECTED
1	["alpha", "beta", null]	{"a": null, "b": "beta"}
2	["carrot"]	{"c": "carrot"}
3	[null]	{"n": null}
EXPECTED
)
expect_output \
    "grouped JSON aggregates" \
    "$grouped_expected" \
    "SELECT g, JSON_ARRAYAGG(s), JSON_OBJECTAGG(k, s) "\
"FROM t GROUP BY g ORDER BY g;" \
    "$DATABASE"

empty_expected=$(cat <<EXPECTED
NULL	NULL
EXPECTED
)
expect_output \
    "empty JSON aggregates" \
    "$empty_expected" \
    "SELECT JSON_ARRAYAGG(s), JSON_OBJECTAGG(k, s) FROM t WHERE g = 99;" \
    "$DATABASE"

nonstring_key_expected=$(cat <<EXPECTED
{"4": "carrot"}	{"1": "carrot"}
EXPECTED
)
expect_output \
    "nonstring JSON object aggregate keys" \
    "$nonstring_key_expected" \
    "SELECT JSON_OBJECTAGG(id, s), JSON_OBJECTAGG(b, s) FROM t WHERE g = 2;" \
    "$DATABASE"

expect_error \
    "NULL JSON object aggregate key" \
    3158 \
    22032 \
    "JSON documents may not contain NULL member names." \
    "SELECT JSON_OBJECTAGG(NULL, id) FROM t WHERE g = 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_aggregate_functions_expectations: ok"
