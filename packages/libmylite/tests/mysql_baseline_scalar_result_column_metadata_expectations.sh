#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_result_column_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_result_column_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
}

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

literal_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT 1 AS one, TRUE AS truthy, NULL AS nil, 'abc' AS str FROM DUAL;" \
    "$DATABASE")

expect_contains "integer alias" 'Field   1:  `one`' "$literal_output"
expect_contains "integer type" 'Type:       LONGLONG' "$literal_output"
expect_contains "integer length" 'Length:     2' "$literal_output"
expect_contains "integer flags" 'Flags:      NOT_NULL BINARY NUM ' "$literal_output"
expect_contains "boolean alias" 'Field   2:  `truthy`' "$literal_output"
expect_contains "boolean length" 'Length:     1' "$literal_output"
expect_contains "null alias" 'Field   3:  `nil`' "$literal_output"
expect_contains "null type" 'Type:       NULL' "$literal_output"
expect_contains "null flags" 'Flags:      BINARY NUM ' "$literal_output"
expect_contains "string alias" 'Field   4:  `str`' "$literal_output"
expect_contains "string type" 'Type:       VAR_STRING' "$literal_output"
expect_contains "string collation" 'Collation:  utf8mb4_0900_ai_ci (255)' "$literal_output"
expect_contains "string length" 'Length:     12' "$literal_output"
expect_contains "string decimals" 'Decimals:   31' "$literal_output"

integer_boundary_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT 18446744073709551615 AS umax, -9223372036854775808 AS nmin, "\
"-9223372036854775809 AS below_min FROM DUAL;" \
    "$DATABASE")

expect_contains "unsigned max alias" 'Field   1:  `umax`' "$integer_boundary_output"
expect_contains "unsigned max type" 'Type:       LONGLONG' "$integer_boundary_output"
expect_contains "unsigned max flags" \
    'Flags:      NOT_NULL UNSIGNED BINARY NUM ' "$integer_boundary_output"
expect_contains "signed min alias" 'Field   2:  `nmin`' "$integer_boundary_output"
expect_contains "signed min length" 'Length:     20' "$integer_boundary_output"
expect_contains "below signed min alias" 'Field   3:  `below_min`' "$integer_boundary_output"
expect_contains "below signed min type" 'Type:       NEWDECIMAL' "$integer_boundary_output"

large_integer_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT "\
"999999999999999999999999999999999999999999999999999999999999999999999999999999999 "\
"AS big, "\
"-999999999999999999999999999999999999999999999999999999999999999999999999999999999 "\
"AS neg_big FROM DUAL;" \
    "$DATABASE")

expect_contains "large integer alias" 'Field   1:  `big`' "$large_integer_output"
expect_contains "large integer type" 'Type:       NEWDECIMAL' "$large_integer_output"
expect_contains "large integer length" 'Length:     82' "$large_integer_output"
expect_contains "large integer flags" 'Flags:      NOT_NULL BINARY NUM ' "$large_integer_output"
expect_contains "negative large integer alias" 'Field   2:  `neg_big`' "$large_integer_output"

session_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT DATABASE(), SCHEMA(), USER(), SESSION_USER(), SYSTEM_USER(), CURRENT_USER(), "\
"VERSION(), ROW_COUNT(), LAST_INSERT_ID(), RAND(), RAND(0) FROM DUAL;" \
    "$DATABASE")

expect_contains "database type" 'Field   1:  `DATABASE()`' "$session_output"
expect_contains "database length" 'Length:     256' "$session_output"
expect_contains "schema type" 'Field   2:  `SCHEMA()`' "$session_output"
expect_contains "schema length" 'Length:     256' "$session_output"
expect_contains "user length" 'Field   3:  `USER()`' "$session_output"
expect_contains "user metadata length" 'Length:     1152' "$session_output"
expect_contains "session user label" 'Field   4:  `SESSION_USER()`' "$session_output"
expect_contains "system user label" 'Field   5:  `SYSTEM_USER()`' "$session_output"
expect_contains "current user label" 'Field   6:  `CURRENT_USER()`' "$session_output"
expect_contains "version label" 'Field   7:  `VERSION()`' "$session_output"
expect_contains "version length" 'Length:     20' "$session_output"
expect_contains "version flags" 'Flags:      NOT_NULL ' "$session_output"
expect_contains "row count label" 'Field   8:  `ROW_COUNT()`' "$session_output"
expect_contains "row count length" 'Length:     21' "$session_output"
expect_contains "last insert flags" 'Flags:      NOT_NULL UNSIGNED BINARY NUM ' "$session_output"
expect_contains "rand type" 'Field  10:  `RAND()`' "$session_output"
expect_contains "rand double" 'Type:       DOUBLE' "$session_output"
expect_contains "rand length" 'Length:     23' "$session_output"
expect_contains "seeded rand type" 'Field  11:  `RAND(0)`' "$session_output"

json_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT JSON_VALID('{}') AS valid_json, JSON_TYPE('{}') AS json_type, "\
"JSON_LENGTH('{}') AS json_length, JSON_CONTAINS('{}','{}') AS contains_json, "\
"JSON_CONTAINS_PATH('{}','one','$') AS contains_path, "\
"JSON_QUOTE('abc') AS quoted_json, "\
"JSON_EXTRACT('{\"a\":1}','$.a') AS extracted_json, JSON_ARRAY(1,'a') AS array_json, "\
"JSON_OBJECT('a',1) AS object_json FROM DUAL;" \
    "$DATABASE")

expect_contains "json valid label" 'Field   1:  `valid_json`' "$json_output"
expect_contains "json valid type" 'Type:       LONGLONG' "$json_output"
expect_contains "json valid flags" 'Flags:      BINARY NUM ' "$json_output"
expect_contains "json type label" 'Field   2:  `json_type`' "$json_output"
expect_contains "json type type" 'Type:       VAR_STRING' "$json_output"
expect_contains "json type length" 'Length:     68' "$json_output"
expect_contains "json type flags" 'Flags:      BINARY ' "$json_output"
expect_contains "json length label" 'Field   3:  `json_length`' "$json_output"
expect_contains "json contains path label" 'Field   5:  `contains_path`' "$json_output"
expect_contains "json quote label" 'Field   6:  `quoted_json`' "$json_output"
expect_contains "json quote type" 'Type:       VAR_STRING' "$json_output"
expect_contains "json quote flags" 'Flags:      BINARY ' "$json_output"
expect_contains "json extract label" 'Field   7:  `extracted_json`' "$json_output"
expect_contains "json extract type" 'Type:       JSON' "$json_output"
expect_contains "json extract length" 'Length:     4294967292' "$json_output"
expect_contains "json extract flags" 'Flags:      BINARY ' "$json_output"
expect_contains "json array label" 'Field   8:  `array_json`' "$json_output"
expect_contains "json object label" 'Field   9:  `object_json`' "$json_output"

row_scalar_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE t(id INT NOT NULL, j JSON, s VARCHAR(20), PRIMARY KEY(id)); "\
"INSERT INTO t VALUES (1, '{\"a\":1}', '{\"a\":1}'); "\
"SELECT id, JSON_VALID(j) AS valid_j, JSON_LENGTH(j) AS len_j, "\
"JSON_CONTAINS(j,'1','$.a') AS c, JSON_CONTAINS_PATH(j,'one','$.a') AS cp, "\
"JSON_TYPE(j) AS jt, JSON_QUOTE(s) AS jq, JSON_EXTRACT(j,'$.a') AS je, "\
"JSON_ARRAY(id,s) AS ja, "\
"JSON_OBJECT('a',id) AS jo FROM t AS src LIMIT 0;" \
    "$DATABASE")

expect_contains "row scalar descriptor label" 'Field   1:  `id`' "$row_scalar_output"
expect_contains "row scalar descriptor table alias" 'Table:      `src`' "$row_scalar_output"
expect_contains "row scalar descriptor origin table" 'Org_table:  `t`' "$row_scalar_output"
expect_contains "row scalar descriptor primary flags" \
    'Flags:      NOT_NULL PRI_KEY NO_DEFAULT_VALUE NUM PART_KEY ' "$row_scalar_output"
expect_contains "row scalar json valid label" 'Field   2:  `valid_j`' "$row_scalar_output"
expect_contains "row scalar json length label" 'Field   3:  `len_j`' "$row_scalar_output"
expect_contains "row scalar json contains label" 'Field   4:  `c`' "$row_scalar_output"
expect_contains "row scalar json contains length" 'Length:     21' "$row_scalar_output"
expect_contains "row scalar json contains path label" 'Field   5:  `cp`' "$row_scalar_output"
expect_contains "row scalar json type label" 'Field   6:  `jt`' "$row_scalar_output"
expect_contains "row scalar json quote label" 'Field   7:  `jq`' "$row_scalar_output"
expect_contains "row scalar json extract label" 'Field   8:  `je`' "$row_scalar_output"
expect_contains "row scalar json array label" 'Field   9:  `ja`' "$row_scalar_output"
expect_contains "row scalar json object label" 'Field  10:  `jo`' "$row_scalar_output"
expect_contains "row scalar metadata warning count" '0 rows in set' "$row_scalar_output"

printf '%s\n' "mysql_baseline_scalar_result_column_metadata_expectations: ok"
