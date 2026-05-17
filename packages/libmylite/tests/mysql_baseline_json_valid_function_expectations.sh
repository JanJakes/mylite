#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_valid_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_valid_function_expectations: $1" >&2
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

literal_expected=$(cat <<EXPECTED
1	0	1	NULL	1	1	1	1	0	0	0	0
EXPECTED
)
expect_output \
    "literal JSON_VALID values" \
    "$literal_expected" \
    "DO 0; SELECT JSON_VALID('{\"a\":1}'), JSON_VALID('hello'), "\
"JSON_VALID('\"hello\"'), JSON_VALID(NULL), JSON_VALID('1.2'), JSON_VALID('1e2'), "\
"JSON_VALID('92233720368547758081234567890'), JSON_VALID('-0'), JSON_VALID('01'), "\
"JSON_VALID(1), JSON_VALID(TRUE), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
JSON_VALID('{"a":1}')	ok
1	0
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_VALID('{\"a\":1}'), JSON_VALID('bad') AS ok FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	1	0	0
2	NULL	0	0	0
3	1	NULL	0	NULL
4	1	0	0	0
EXPECTED
)
expect_output \
    "table JSON_VALID values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, j JSON, s VARCHAR(64), i INT, b VARBINARY(64)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":1}', '{\"b\":2}', 7, CAST('{\"a\":1}' AS BINARY)), "\
"(2, NULL, 'bad', 0, CAST('bad' AS BINARY)), "\
"(3, 'true', NULL, 1, NULL), "\
"(4, '1.2', '[1,]', NULL, CAST('[1,2]' AS BINARY)); "\
"SELECT id, JSON_VALID(j), JSON_VALID(s), JSON_VALID(i), JSON_VALID(b) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

where_expected=$(cat <<EXPECTED
1
EXPECTED
)
expect_output \
    "WHERE JSON_VALID filters true values" \
    "$where_expected" \
    "SELECT id FROM t WHERE JSON_VALID(s) ORDER BY id;" \
    "$DATABASE"

json_text_expected=$(cat <<EXPECTED
1	1	0	0	0
EXPECTED
)
expect_output \
    "JSON text edge cases" \
    "$json_text_expected" \
    "SELECT JSON_VALID('{\"a\": 1.2e3}'), "\
"JSON_VALID('{\"u\":\"\\\\uD834\\\\uDD1E\"}'), JSON_VALID('{\"a\":}'), "\
"JSON_VALID(''), JSON_VALID('[1,]');" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO JSON_VALID status" \
    "$do_expected" \
    "DO JSON_VALID('{\"a\":1}'), JSON_VALID('bad'), JSON_VALID(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "JSON_VALID zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_VALID();" \
    "$DATABASE"

expect_error \
    "JSON_VALID too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_VALID('{}', '{}');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_valid_function_expectations: ok"
