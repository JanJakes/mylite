#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_order_limit_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_select_order_limit_lifecycle_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE numbers ("\
"id INT NOT NULL, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO numbers VALUES "\
"(1, -2, 0, -9223372036854775808, 0, NULL, 5), "\
"(2, 1, 2, 3, 4, 9, 6), "\
"(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7), "\
"(4, 0, 8, 8, 8, 9, 8); "\
"CREATE TABLE integer_aliases ("\
"id INT NOT NULL, ii INTEGER, intu INT UNSIGNED, integeru INTEGER UNSIGNED); "\
"INSERT INTO integer_aliases VALUES (1, -3, 4294967295, 7), (2, 5, 0, 8);" \
    "$DATABASE" >/dev/null

expect_error \
    "ordered select without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table ORDER BY id LIMIT 1;"

expect_error \
    "qualified ordered select unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.missing_table ORDER BY id LIMIT 1;"

expected_projection=$(cat <<'EOF'
i	nn
-2	5
0	8
1	6
2147483647	7
EOF
)
expect_output_with_headers \
    "ordered projection labels and rows" \
    "$expected_projection" \
    "SELECT i, nn FROM numbers ORDER BY i;" \
    "$DATABASE"

expect_output \
    "explicit asc order" \
    "-2
0
1
2147483647" \
    "SELECT i FROM numbers ORDER BY i ASC;" \
    "$DATABASE"

expect_output \
    "explicit desc order" \
    "2147483647
1
0
-2" \
    "SELECT i FROM numbers ORDER BY i DESC;" \
    "$DATABASE"

expect_output \
    "nullable asc null ordering" \
    "NULL
NULL
9
9" \
    "SELECT n FROM numbers ORDER BY n;" \
    "$DATABASE"

expect_output \
    "nullable desc null ordering" \
    "9
9
NULL
NULL" \
    "SELECT n FROM numbers ORDER BY n DESC;" \
    "$DATABASE"

expect_output \
    "unsigned int order boundary" \
    "0
2
8
4294967295" \
    "SELECT iu FROM numbers ORDER BY iu;" \
    "$DATABASE"

expect_output \
    "signed bigint order boundary" \
    "-9223372036854775808
3
8
9223372036854775807" \
    "SELECT b FROM numbers ORDER BY b;" \
    "$DATABASE"

expect_output \
    "bigint unsigned signed64 order boundary" \
    "0
4
8
9223372036854775807" \
    "SELECT bu FROM numbers ORDER BY bu;" \
    "$DATABASE"

expect_output \
    "order by nonprojected column" \
    "1
2
3
4" \
    "SELECT id FROM numbers ORDER BY nn;" \
    "$DATABASE"

expect_output \
    "limit zero" \
    "" \
    "SELECT id FROM numbers ORDER BY id LIMIT 0;" \
    "$DATABASE"

expect_output \
    "limit exact row count" \
    "1
2" \
    "SELECT id FROM numbers ORDER BY id LIMIT 2;" \
    "$DATABASE"

expect_output \
    "limit larger than result set" \
    "1
2
3
4" \
    "SELECT id FROM numbers ORDER BY id LIMIT 10;" \
    "$DATABASE"

expect_output \
    "limit offset form" \
    "2
3" \
    "SELECT id FROM numbers ORDER BY id LIMIT 2 OFFSET 1;" \
    "$DATABASE"

expect_output \
    "limit comma form" \
    "2
3" \
    "SELECT id FROM numbers ORDER BY id LIMIT 1, 2;" \
    "$DATABASE"

expect_output \
    "limit offset beyond result set" \
    "" \
    "SELECT id FROM numbers ORDER BY id LIMIT 2 OFFSET 10;" \
    "$DATABASE"

expect_output \
    "limit zero row count after offset" \
    "" \
    "SELECT id FROM numbers ORDER BY id LIMIT 1, 0;" \
    "$DATABASE"

expect_output \
    "where order limit composition" \
    "3" \
    "SELECT id FROM numbers WHERE n IS NULL ORDER BY id DESC LIMIT 1;" \
    "$DATABASE"

expect_output \
    "schema-qualified ordered limited select" \
    "3" \
    "SELECT id FROM ${DATABASE}.numbers WHERE nn >= 6 ORDER BY i DESC LIMIT 1;" \
    "$DATABASE"

expect_output \
    "integer alias ordered limit" \
    "1" \
    "SELECT id FROM integer_aliases ORDER BY ii LIMIT 1;" \
    "$DATABASE"

expect_error \
    "unknown ordered select table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "SELECT * FROM missing_table ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT missing FROM numbers ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing = 1 ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "SELECT id FROM numbers ORDER BY missing LIMIT 1;" \
    "$DATABASE"

expect_error \
    "signed plus limit literal upstream" \
    1064 \
    42000 \
    "near '+1'" \
    "SELECT id FROM numbers ORDER BY id LIMIT +1;" \
    "$DATABASE"

expect_error \
    "signed minus limit literal upstream" \
    1064 \
    42000 \
    "near '-1'" \
    "SELECT id FROM numbers ORDER BY id LIMIT -1;" \
    "$DATABASE"

expect_error \
    "decimal limit literal upstream" \
    1064 \
    42000 \
    "near '1.0'" \
    "SELECT id FROM numbers ORDER BY id LIMIT 1.0;" \
    "$DATABASE"

expect_error \
    "string limit literal upstream" \
    1064 \
    42000 \
    "near ''1''" \
    "SELECT id FROM numbers ORDER BY id LIMIT '1';" \
    "$DATABASE"

expect_error \
    "hex limit literal upstream" \
    1064 \
    42000 \
    "near '0x1'" \
    "SELECT id FROM numbers ORDER BY id LIMIT 0x1;" \
    "$DATABASE"

expect_error \
    "bit limit literal upstream" \
    1064 \
    42000 \
    "near 'b'1''" \
    "SELECT id FROM numbers ORDER BY id LIMIT b'1';" \
    "$DATABASE"

expect_error \
    "parameter limit literal upstream" \
    1064 \
    42000 \
    "near '?'" \
    "SELECT id FROM numbers ORDER BY id LIMIT ?;" \
    "$DATABASE"

expect_error \
    "unsigned64 overflow limit upstream" \
    1064 \
    42000 \
    "near '18446744073709551616'" \
    "SELECT id FROM numbers ORDER BY id LIMIT 18446744073709551616;" \
    "$DATABASE"

expect_output \
    "mysql accepts unsigned64 maximum limit upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers ORDER BY id LIMIT 18446744073709551615;" \
    "$DATABASE"

expect_output \
    "mysql accepts table-qualified order upstream" \
    "1
2" \
    "SELECT id FROM numbers ORDER BY numbers.id LIMIT 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression order upstream" \
    "1
2" \
    "SELECT id FROM numbers ORDER BY id + 1 LIMIT 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts ordinal order upstream" \
    "1
2" \
    "SELECT id FROM numbers ORDER BY 1 LIMIT 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts alias order upstream" \
    "1
2" \
    "SELECT id AS x FROM numbers ORDER BY x LIMIT 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts multiple order keys upstream" \
    "1
3" \
    "SELECT id FROM numbers ORDER BY n, id LIMIT 2;" \
    "$DATABASE"

printf '%s\n' "baseline-select-order-limit-lifecycle MySQL 8.4.9 expectations verified"
