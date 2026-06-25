#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_search_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_search_function_expectations: $1" >&2
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
"$[0]"	["$[0]", "$[2].x"]	"$[1][0].k"	["$.a", "$.b", "$.c"]	"$.a"	"$.a"	NULL	"$[2].x"	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "literal JSON_SEARCH values" \
    "$literal_expected" \
    "SELECT "\
"JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', "\
"'one', 'abc'), "\
"JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', "\
"'all', 'abc'), "\
"JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', "\
"'all', '10'), "\
"JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\",\"c\":\"axc\"}', 'all', 'a_c'), "\
"JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a\\\\_c'), "\
"JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a!_c', '!'), "\
"JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a\\\\_c', ''), "\
"JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}]', 'one', 'abc', "\
"NULL, '$[2]'), "\
"JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'ABC'), "\
"JSON_SEARCH(NULL, 'one', 'abc'), "\
"JSON_SEARCH('{\"a\":\"abc\"}', NULL, 'abc'), "\
"JSON_SEARCH('{\"a\":\"abc\"}', 'one', NULL), "\
"JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', NULL, NULL);" \
    "$DATABASE"

wildcard_escape_expected=$(cat <<EXPECTED
"$[0]"	"$[0]"	"$[2]"	"$[3]"	"$[0]"	"$[1]"
EXPECTED
)
expect_output \
    "JSON_SEARCH wildcard escape characters" \
    "$wildcard_escape_expected" \
    "SELECT "\
"JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '%a', '%'), "\
"JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '_a', '_'), "\
"JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '%%', '%'), "\
"JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '__', '_'), "\
"JSON_SEARCH('[\"%\",\"_\",\"a\"]', 'all', '%', '%'), "\
"JSON_SEARCH('[\"%\",\"_\",\"a\"]', 'all', '_', '_');" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	["$.a", "$.b[1]"]	"$.tags[0]"
2	NULL	NULL
EXPECTED
)
expect_output \
    "table JSON_SEARCH projection" \
    "$table_expected" \
    "CREATE TABLE t(id INT PRIMARY KEY, j JSON, s TEXT, pat VARCHAR(10)); "\
"INSERT INTO t VALUES "\
"(1, '{\"a\":\"abc\",\"b\":[\"def\",\"abc\"]}', '{\"tags\":[\"blue\",\"red\"]}', 'abc'), "\
"(2, '{\"a\":\"zzz\"}', '{\"tags\":[\"green\"]}', 'abc'); "\
"SELECT id, JSON_SEARCH(j, 'all', pat), JSON_SEARCH(s, 'one', 'blue', NULL, '$.tags') "\
"FROM t ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<EXPECTED
1
EXPECTED
)
expect_output \
    "table JSON_SEARCH predicate" \
    "$predicate_expected" \
    "SELECT id FROM t WHERE JSON_SEARCH(j, 'one', pat) IS NOT NULL ORDER BY id;" \
    "$DATABASE"

updated_expected=$(cat <<EXPECTED
1	"$.a"
2	{"tags":["green"]}
EXPECTED
)
expect_output \
    "JSON_SEARCH update assignment" \
    "$updated_expected" \
    "UPDATE t SET s = JSON_SEARCH(j, 'one', pat) WHERE id = 1; "\
"SELECT id, s FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT JSON_SEARCH();" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid one_or_all" \
    3154 \
    "42000" \
    "oneOrAll argument to json_search" \
    "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'bad', 'abc');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid JSON text" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_SEARCH('{bad}', 'one', 'abc');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid JSON before NULL mode" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_SEARCH('{bad}', NULL, 'abc');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid JSON before NULL path" \
    3141 \
    "22032" \
    "Invalid JSON text" \
    "SELECT JSON_SEARCH('{bad}', 'one', 'abc', NULL, NULL);" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH binary document" \
    3144 \
    "22032" \
    "CHARACTER SET 'binary'" \
    "SELECT JSON_SEARCH(CAST('{\"a\":\"abc\"}' AS BINARY), 'one', 'abc');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid document type" \
    3146 \
    "22032" \
    "Invalid data type for JSON data" \
    "SELECT JSON_SEARCH(1, 'one', 'abc');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid escape width" \
    1210 \
    "HY000" \
    "Incorrect arguments to ESCAPE" \
    "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', 'xx');" \
    "$DATABASE"

expect_error \
    "JSON_SEARCH invalid path" \
    3143 \
    "42000" \
    "Invalid JSON path expression" \
    "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', NULL, '$.');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_search_function_expectations: ok"
