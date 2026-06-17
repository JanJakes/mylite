#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_tableless_select_expression_clauses_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "literal order by ordinal" \
    "1" \
    "SELECT 1 ORDER BY 1;"

expect_output \
    "literal order by alias and ordinal" \
    "1	2" \
    "SELECT 1 AS one, 2 AS two ORDER BY one DESC, 2 ASC;"

expect_output \
    "scalar function order expression" \
    "2" \
    "SELECT IF(1,2,3) ORDER BY IF(1,2,3) DESC, 1;"

expect_output \
    "case order ordinal" \
    "2" \
    "SELECT CASE WHEN 1 THEN 2 ELSE 3 END ORDER BY 1;"

expect_output \
    "where true order ordinal" \
    "1" \
    "SELECT 1 WHERE TRUE ORDER BY 1;"

expect_output \
    "where false order ordinal" \
    "" \
    "SELECT 1 WHERE FALSE ORDER BY 1;"

expect_output \
    "dual order limit" \
    "1" \
    "SELECT 1 FROM DUAL ORDER BY 1 LIMIT 1;"

expect_error \
    "invalid order ordinal" \
    1054 \
    "42S22" \
    "Unknown column '2' in 'order clause'" \
    "SELECT 1 ORDER BY 2;"

expect_error \
    "invalid order identifier" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'order clause'" \
    "SELECT 1 ORDER BY missing;"

printf '%s\n' "mysql_baseline_tableless_select_expression_clauses_expectations: ok"
