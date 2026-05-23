#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_quote_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_quote_function_expectations: $1" >&2
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

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
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

quote_expected=$(cat <<EXPECTED
"null"	"\"null\""	"[1, 2, 3]"	NULL
EXPECTED
)
expect_output \
    "literal JSON_QUOTE values" \
    "$quote_expected" \
    "SET @@sql_mode = ''; SELECT JSON_QUOTE('null'), JSON_QUOTE('\"null\"'), "\
"JSON_QUOTE('[1, 2, 3]'), JSON_QUOTE(NULL);" \
    "$DATABASE"

escape_expected=$(cat <<EXPECTED
22615C6E6222	22615C5C6E6222	22715C226222
22615C5C6E6222	22615C5C5C5C6E6222	22715C226222
EXPECTED
)
expect_output \
    "JSON_QUOTE escape decoding and SQL mode literal decoding" \
    "$escape_expected" \
    "SET @@sql_mode = ''; SELECT HEX(JSON_QUOTE('a\\nb')), "\
"HEX(JSON_QUOTE('a\\\\nb')), HEX(JSON_QUOTE('q\"b')); "\
"SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'; SELECT HEX(JSON_QUOTE('a\\nb')), "\
"HEX(JSON_QUOTE('a\\\\nb')), HEX(JSON_QUOTE('q\"b'));" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	"abc"	"a\nb"
2	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_QUOTE values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, s VARCHAR(20), tx TEXT, j JSON, b VARBINARY(5), n INT); "\
"INSERT INTO t VALUES (1, 'abc', 'a\\nb', '{\"a\":1}', _binary 'abc', 42), "\
"(2, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, JSON_QUOTE(s), JSON_QUOTE(tx) FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
q	n
"abc"	NULL
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT JSON_QUOTE('abc') AS q, JSON_QUOTE(NULL) AS n FROM DUAL;" \
    "$DATABASE"

status_expected=$(cat <<EXPECTED
"abc"
-1	0
0	0
EXPECTED
)
expect_output \
    "JSON_QUOTE statement status" \
    "$status_expected" \
    "SELECT JSON_QUOTE('abc'); SELECT ROW_COUNT(), @@warning_count; "\
"DO JSON_QUOTE('abc'), JSON_QUOTE(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

type_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT JSON_QUOTE('abc') AS q, JSON_QUOTE(NULL) AS n FROM DUAL; "\
"SELECT id, JSON_QUOTE(s) AS qs, JSON_QUOTE(tx) AS qt FROM t ORDER BY id;" \
    "$DATABASE")

expect_contains "literal quote metadata label" 'Field   1:  `q`' "$type_output"
expect_contains "literal quote metadata type" 'Type:       VAR_STRING' "$type_output"
expect_contains "literal quote metadata flags" 'Flags:      BINARY ' "$type_output"
expect_contains "literal null quote metadata label" 'Field   2:  `n`' "$type_output"
expect_contains "varchar quote metadata label" 'Field   2:  `qs`' "$type_output"
expect_contains "text quote metadata label" 'Field   3:  `qt`' "$type_output"

expect_error \
    "JSON_QUOTE zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_QUOTE();" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_QUOTE('a', 'b');" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE numeric argument" \
    3064 \
    "HY000" \
    "Incorrect type for argument 1 in function json_quote." \
    "SELECT JSON_QUOTE(123);" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE boolean argument" \
    3064 \
    "HY000" \
    "Incorrect type for argument 1 in function json_quote." \
    "SELECT JSON_QUOTE(TRUE);" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE binary input" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_QUOTE(CAST('abc' AS BINARY));" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE JSON column" \
    3064 \
    "HY000" \
    "Incorrect type for argument 1 in function json_quote." \
    "SELECT JSON_QUOTE(j) FROM t LIMIT 1;" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE numeric column" \
    3064 \
    "HY000" \
    "Incorrect type for argument 1 in function json_quote." \
    "SELECT JSON_QUOTE(n) FROM t LIMIT 1;" \
    "$DATABASE"

expect_error \
    "JSON_QUOTE binary column" \
    3144 \
    "22032" \
    "Cannot create a JSON value from a string with CHARACTER SET 'binary'" \
    "SELECT JSON_QUOTE(b) FROM t LIMIT 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_quote_function_expectations: ok"
