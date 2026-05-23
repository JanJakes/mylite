#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_values_statement_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output_with_headers \
    "values works without default schema and updates result state" \
    "column_0
1
ROW_COUNT()|@@warning_count
-1|0" \
    "VALUES ROW(1); SELECT ROW_COUNT(), @@warning_count;"

expect_output_with_headers \
    "mixed supported literal labels and values" \
    "column_0|column_1|column_2|column_3|column_4|column_5
1|-2|NULL|a|1|0" \
    "VALUES ROW(1, -2, NULL, 'a', TRUE, FALSE);"

expect_output \
    "multi row values preserve constructor order" \
    "1|a
2|b
NULL|NULL" \
    "VALUES ROW(1, 'a'), ROW(2, 'b'), ROW(NULL, NULL);"

expect_output \
    "limit zero returns no rows" \
    "" \
    "VALUES ROW(1), ROW(2), ROW(3) LIMIT 0;"

expect_output \
    "limit row count returns prefix" \
    "1
2" \
    "VALUES ROW(1), ROW(2), ROW(3) LIMIT 2;"

expect_output \
    "limit comma offset form" \
    "2
3" \
    "VALUES ROW(1), ROW(2), ROW(3) LIMIT 1, 2;"

expect_output \
    "limit offset keyword form" \
    "2
3" \
    "VALUES ROW(1), ROW(2), ROW(3) LIMIT 2 OFFSET 1;"

expect_output_with_headers \
    "order by implicit column validates and preserves constructor order" \
    "column_0|column_1|column_2
1|-2|3
5|7|9
4|6|8" \
    "VALUES ROW(1,-2,3), ROW(5,7,9), ROW(4,6,8) ORDER BY column_1;"

expect_output \
    "order by direction and limit preserve constructor prefix" \
    "1|-2|3
5|7|9" \
    "VALUES ROW(1,-2,3), ROW(5,7,9), ROW(4,6,8) ORDER BY Column_1 DESC LIMIT 2;"

expect_output \
    "order by quoted implicit column validates" \
    "1|2|3" \
    'VALUES ROW(1,2,3) ORDER BY `column_1`;'

expect_error \
    "values rejects empty row" \
    3942 \
    HY000 \
    "Each row of a VALUES clause must have at least one column" \
    "VALUES ROW();"

expect_error \
    "values rejects mismatched row width" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 2" \
    "VALUES ROW(1), ROW(1,2);"

expect_error \
    "values rejects default" \
    3943 \
    HY000 \
    "A VALUES clause cannot use DEFAULT values" \
    "VALUES ROW(DEFAULT);"

expect_error \
    "values rejects unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'column_3' in 'order clause'" \
    "VALUES ROW(1,2,3) ORDER BY column_3;"

expect_error \
    "values rejects order ordinal zero" \
    1054 \
    42S22 \
    "Unknown column '0' in 'order clause'" \
    "VALUES ROW(1,2,3) ORDER BY 0;"

expect_error \
    "values rejects alias" \
    1064 \
    42000 \
    "near 'AS t'" \
    "VALUES ROW(1) AS t;"

expect_error \
    "values requires row constructor" \
    1064 \
    42000 \
    "near '(1)'" \
    "VALUES (1);"

expect_error \
    "values rejects where clause" \
    1064 \
    42000 \
    "near 'WHERE TRUE'" \
    "VALUES ROW(1) WHERE TRUE;"

expect_error \
    "values rejects signed limit" \
    1064 \
    42000 \
    "near '+1'" \
    "VALUES ROW(1), ROW(2) LIMIT +1;"

expect_error \
    "values rejects decimal limit" \
    1064 \
    42000 \
    "near '1.0'" \
    "VALUES ROW(1), ROW(2) LIMIT 1.0;"

printf '%s\n' "mysql_baseline_values_statement_expectations: ok"
