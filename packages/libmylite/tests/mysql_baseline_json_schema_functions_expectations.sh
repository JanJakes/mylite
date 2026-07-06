#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_schema_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_schema_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

valid_expected=$(cat <<EXPECTED
1	0	1	1	NULL	NULL
EXPECTED
)
expect_output \
    "JSON_SCHEMA_VALID base values" \
    "$valid_expected" \
    "SELECT JSON_SCHEMA_VALID('{\"type\":\"object\"}', '{\"a\":1}'), "\
"JSON_SCHEMA_VALID('{\"type\":\"object\"}', '[1]'), "\
"JSON_SCHEMA_VALID('{\"type\":[\"object\",\"null\"]}', 'null'), "\
"JSON_SCHEMA_VALID('{}', '123'), JSON_SCHEMA_VALID(NULL, '{}'), "\
"JSON_SCHEMA_VALID('{}', NULL);" \
    "$DATABASE"

properties_expected=$(cat <<EXPECTED
1	0	0	1	0
EXPECTED
)
expect_output \
    "JSON_SCHEMA_VALID object properties" \
    "$properties_expected" \
    "SELECT JSON_SCHEMA_VALID('{\"type\":\"object\",\"required\":[\"id\"],\"properties\":"\
"{\"id\":{\"type\":\"integer\"},\"name\":{\"type\":\"string\"}}}', "\
"'{\"id\":7,\"name\":\"Ada\"}'), "\
"JSON_SCHEMA_VALID('{\"type\":\"object\",\"required\":[\"id\"],\"properties\":"\
"{\"id\":{\"type\":\"integer\"},\"name\":{\"type\":\"string\"}}}', "\
"'{\"name\":\"Ada\"}'), "\
"JSON_SCHEMA_VALID('{\"type\":\"object\",\"required\":[\"id\"],\"properties\":"\
"{\"id\":{\"type\":\"integer\"},\"name\":{\"type\":\"string\"}}}', "\
"'{\"id\":\"7\"}'), "\
"JSON_SCHEMA_VALID('{\"minimum\":2}', '3'), JSON_SCHEMA_VALID('{\"maximum\":2}', '3');" \
    "$DATABASE"

report_expected=$(cat <<EXPECTED
{"valid": true}	{"valid": false, "reason": "The JSON document location '#' failed requirement 'type' at JSON Schema location '#'", "schema-location": "#", "document-location": "#", "schema-failed-keyword": "type"}	{"valid": false, "reason": "The JSON document location '#' failed requirement 'required' at JSON Schema location '#'", "schema-location": "#", "document-location": "#", "schema-failed-keyword": "required"}
EXPECTED
)
expect_output \
    "JSON_SCHEMA_VALIDATION_REPORT values" \
    "$report_expected" \
    "SELECT JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\"}', '{\"a\":1}'), "\
"JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\"}', '[1]'), "\
"JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\",\"required\":[\"id\"]}', '{}');" \
    "$DATABASE"

minimum_expected=$(cat <<EXPECTED
1	1	0	0
EXPECTED
)
expect_output \
    "JSON_SCHEMA_VALID keyword applicability" \
    "$minimum_expected" \
    "SELECT JSON_SCHEMA_VALID('{\"required\":[\"a\"]}', '1'), "\
"JSON_SCHEMA_VALID('{\"properties\":{\"a\":{\"type\":\"integer\"}}}', '1'), "\
"JSON_SCHEMA_VALID('{\"minimum\":2}', '1'), JSON_SCHEMA_VALID('{\"maximum\":2}', '3');" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE js_rows (id INT PRIMARY KEY, doc JSON, schema_text TEXT);" \
    "$DATABASE" >/dev/null
run_mysql \
    "INSERT INTO js_rows VALUES "\
"(1, '{\"id\":1,\"score\":7}', '{\"type\":\"object\",\"required\":[\"id\"]}'), "\
"(2, '{\"score\":11}', '{\"type\":\"object\",\"required\":[\"id\"]}');" \
    "$DATABASE" >/dev/null

row_backed_expected=$(cat <<EXPECTED
1	1	{"valid": true}
2	0	{"valid": false, "reason": "The JSON document location '#/score' failed requirement 'maximum' at JSON Schema location '#/properties/score'", "schema-location": "#/properties/score", "document-location": "#/score", "schema-failed-keyword": "maximum"}
EXPECTED
)
expect_output \
    "JSON_SCHEMA row-backed values" \
    "$row_backed_expected" \
    "SELECT id, JSON_SCHEMA_VALID(schema_text, doc) AS ok, "\
"JSON_SCHEMA_VALIDATION_REPORT('{\"properties\":{\"score\":{\"maximum\":10}}}', doc) "\
"AS report FROM js_rows ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_SCHEMA_VALID();" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALIDATION_REPORT one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_SCHEMA_VALIDATION_REPORT('{}');" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID invalid schema text" \
    3141 \
    "22032" \
    "Invalid JSON text in argument 1" \
    "SELECT JSON_SCHEMA_VALID('{bad}', '{}');" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID invalid document text" \
    3141 \
    "22032" \
    "Invalid JSON text in argument 2" \
    "SELECT JSON_SCHEMA_VALID('{}', '{bad}');" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID invalid schema type" \
    3853 \
    "22032" \
    "an object is required" \
    "SELECT JSON_SCHEMA_VALID('[]', '{}');" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID invalid data type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data in argument 2" \
    "SELECT JSON_SCHEMA_VALID('{}', 1);" \
    "$DATABASE"

expect_error \
    "JSON_SCHEMA_VALID unsupported ref" \
    1235 \
    "42000" \
    "references in JSON Schema" \
    "SELECT JSON_SCHEMA_VALID('{\"\$ref\":\"x\"}', '{}');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_schema_functions_expectations: ok"
