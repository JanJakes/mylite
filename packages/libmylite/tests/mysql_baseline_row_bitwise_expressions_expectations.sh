#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_bitwise_expressions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_bitwise_expressions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE bits(id INT, mask INT, shift_count INT);
           INSERT INTO bits VALUES
             (NULL,1,1),(0,3,1),(1,3,1),(2,3,1),(5,6,2),(-1,3,1);" \
    >/dev/null

expect_output_with_headers \
    "row bitwise projection" \
    "id	mask	id&mask	id|mask	id^mask	~id	id<<shift_count	id>>shift_count	mask<<id
-1	3	3	18446744073709551615	18446744073709551612	0	18446744073709551614	9223372036854775807	0
0	3	0	3	3	18446744073709551615	0	0	3
1	3	1	3	2	18446744073709551614	2	0	6
2	3	2	3	1	18446744073709551613	4	1	12
5	6	4	7	3	18446744073709551610	20	1	192
NULL	1	NULL	NULL	NULL	NULL	NULL	NULL	NULL" \
    "SELECT id,mask,id&mask,id|mask,id^mask,~id,id<<shift_count,id>>shift_count,mask<<id
     FROM bits ORDER BY id IS NULL,id;" \
    "$DATABASE"

expect_output_with_headers \
    "row bitwise nesting" \
    "id	nested
-1	2
0	0
1	0
2	2
5	6
NULL	NULL" \
    "SELECT id,((id&mask)|1)^(mask>>1) AS nested FROM bits ORDER BY id IS NULL,id;" \
    "$DATABASE"

expect_output_with_headers \
    "row bitwise predicates" \
    "id	mask
-1	3
5	6" \
    "SELECT id,mask FROM bits WHERE (id&mask) >= 3 ORDER BY id&mask,id;" \
    "$DATABASE"

expect_output_with_headers \
    "row bitwise null-safe predicate" \
    "id
NULL" \
    "SELECT id FROM bits WHERE (id&mask) <=> NULL ORDER BY id IS NULL,id;" \
    "$DATABASE"

expect_output_with_headers \
    "row bitwise predicate operator coverage" \
    "id
-1
2
5" \
    "SELECT id FROM bits WHERE (id|mask)=7 OR (id^mask)=1
     OR (~id)=0 OR (id>>shift_count)=1 ORDER BY id;" \
    "$DATABASE"

expect_output_with_headers \
    "row bitwise order key" \
    "id	key_value
5	192
2	12
1	6
0	3
-1	0
NULL	NULL" \
    "SELECT id,mask<<id AS key_value FROM bits ORDER BY mask<<id DESC,id IS NULL,id;" \
    "$DATABASE"

expect_output \
    "row bitwise shift boundaries" \
    "0	0	9223372036854775807	18446744073709551614" \
    "SELECT 1<<64,1<<-1,-1>>1,-1<<1 FROM bits LIMIT 1;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT id&'3',id&3.2,id&X'03',id&b'11'
     FROM bits ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted operands deferred by this slice" \
    "id&'3'	id&3.2	id&X'03'	id&b'11'
3	3	3	3
0	0	0	0
1	1	1	1
2	2	2	2
1	1	1	1
NULL	NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_row_bitwise_expressions_expectations: ok"
