#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_table_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_table_literal_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

expect_output \
    "literal JSON_TABLE rows" \
    "$(printf '%b' '1\tapple\t3\t1\t{"a": 1}\n2\tpear\tNULL\t0\tNULL\n3\t42\t5\t1\t[1, 2]')" \
    "SELECT jt.ord, jt.name, jt.price, jt.has_price, jt.payload
       FROM JSON_TABLE(
            '[{\"name\":\"apple\",\"price\":3,\"payload\":{\"a\":1}},
              {\"name\":\"pear\"},{\"name\":42,\"price\":5,\"payload\":[1,2]}]',
            '\$[*]' COLUMNS (
                ord FOR ORDINALITY,
                name VARCHAR(20) PATH '\$.name',
                price INT PATH '\$.price' NULL ON EMPTY NULL ON ERROR,
                has_price INT EXISTS PATH '\$.price',
                payload JSON PATH '\$.payload'
            )
       ) AS jt;" \
    "$DATABASE"

expect_output \
    "root JSON_TABLE row" \
    "$(printf '%b' '1\troot')" \
    "SELECT ord, name
       FROM JSON_TABLE('{\"name\":\"root\"}', '\$'
            COLUMNS (ord FOR ORDINALITY, name VARCHAR(20) PATH '\$.name')) AS jt;" \
    "$DATABASE"

expect_output \
    "JSON_TABLE filter order" \
    "$(printf '%b' '1\tapple')" \
    "SELECT jt.ord, jt.name
       FROM JSON_TABLE('[{\"name\":\"apple\",\"price\":3},{\"name\":\"pear\"}]', '\$[*]'
            COLUMNS (
                ord FOR ORDINALITY,
                name VARCHAR(20) PATH '\$.name',
                price INT PATH '\$.price'
            )
       ) AS jt
      WHERE jt.price IS NOT NULL
      ORDER BY jt.ord DESC;" \
    "$DATABASE"

expect_output \
    "JSON_TABLE empty rowset" \
    "" \
    "SELECT *
       FROM JSON_TABLE('[]', '\$[*]'
            COLUMNS (ord FOR ORDINALITY, name VARCHAR(20) PATH '\$.name')) AS jt;" \
    "$DATABASE"

run_mysql \
    "CREATE VIEW ${DATABASE}.v_json_table AS
     SELECT jt.ord, jt.name, jt.price, jt.has_price, jt.payload
       FROM JSON_TABLE('[{\"name\":\"apple\",\"price\":3,\"payload\":{\"a\":1}}]', '\$[*]'
            COLUMNS (
                ord FOR ORDINALITY,
                name VARCHAR(20) PATH '\$.name',
                price INT PATH '\$.price',
                has_price INT EXISTS PATH '\$.price',
                payload JSON PATH '\$.payload'
            )
       ) AS jt;" >/dev/null

expect_output \
    "JSON_TABLE view metadata" \
    "$(printf '%b' 'ord\tbigint\tbigint\tYES\tNULL\tNULL\t19\t0\nname\tvarchar(20)\tvarchar\tYES\tNULL\t20\tNULL\tNULL\nprice\tint\tint\tYES\tNULL\tNULL\t10\t0\nhas_price\tint\tint\tYES\tNULL\tNULL\t10\t0\npayload\tjson\tjson\tYES\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT,
            CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'v_json_table'
      ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "JSON_TABLE alias required" \
    3667 \
    42000 \
    "Every table function must have an alias" \
    "SELECT * FROM JSON_TABLE('[1]', '\$[*]' COLUMNS (ord FOR ORDINALITY));" \
    "$DATABASE"

expect_error \
    "JSON_TABLE invalid JSON document" \
    3141 \
    22032 \
    "Invalid JSON text" \
    "SELECT * FROM JSON_TABLE('[bad]', '\$[*]' COLUMNS (ord FOR ORDINALITY)) AS jt;" \
    "$DATABASE"

expect_error \
    "JSON_TABLE invalid row path" \
    3143 \
    42000 \
    "Invalid JSON path expression" \
    "SELECT * FROM JSON_TABLE('[1]', 'bad' COLUMNS (ord FOR ORDINALITY)) AS jt;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_table_literal_expectations: ok"
