#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_statement_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_table_statement_expectations: $1" >&2
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
    "CREATE TABLE ${DATABASE}.numbers ("\
"id INT NOT NULL, v INT, n INT NULL, s VARCHAR(10), hidden INT INVISIBLE); "\
"INSERT INTO ${DATABASE}.numbers (id, v, n, s, hidden) VALUES "\
"(1, 20, NULL, 'b', 100), "\
"(2, 10, 7, 'a', 200), "\
"(3, 15, NULL, 'c', 300), "\
"(4, NULL, 9, 'd', 400);" >/dev/null

expected_all=$(cat <<'EOF'
id	v	n	s
1	20	NULL	b
2	10	7	a
3	15	NULL	c
4	NULL	9	d
EOF
)
plain_output=$(run_mysql_with_headers "TABLE numbers;" "$DATABASE")
plain_header=$(printf '%s\n' "$plain_output" | sed -n '1p')
plain_line_count=$(printf '%s\n' "$plain_output" | awk 'END { print NR }')
expected_header=$(printf 'id\tv\tn\ts')
if [ "$plain_header" != "$expected_header" ]; then
    fail "plain table header: expected [$expected_header], got [$plain_header]"
fi
if [ "$plain_line_count" != "5" ]; then
    fail "plain table line count: expected [5], got [$plain_line_count]"
fi

expect_output_with_headers \
    "ordered table returns visible columns" \
    "$expected_all" \
    "TABLE numbers ORDER BY id;" \
    "$DATABASE"

expect_output \
    "row count and warning state" \
    "1	20	NULL	b
-1	0" \
    "TABLE numbers ORDER BY id LIMIT 1; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "default ascending null order" \
"4	NULL	9	d
2	10	7	a
3	15	NULL	c
1	20	NULL	b" \
    "TABLE numbers ORDER BY v;" \
    "$DATABASE"

expect_output \
    "explicit ascending order" \
"4	NULL	9	d
2	10	7	a
3	15	NULL	c
1	20	NULL	b" \
    "TABLE numbers ORDER BY v ASC;" \
    "$DATABASE"

expect_output \
    "explicit descending null order" \
"1	20	NULL	b
3	15	NULL	c
2	10	7	a
4	NULL	9	d" \
    "TABLE numbers ORDER BY v DESC;" \
    "$DATABASE"

expect_output \
    "multiple order keys" \
    "4	NULL	9	d
2	10	7	a
3	15	NULL	c
1	20	NULL	b" \
    "TABLE numbers ORDER BY v, id;" \
    "$DATABASE"

expect_output \
    "table-qualified order key" \
    "1	20	NULL	b
2	10	7	a" \
    "TABLE numbers ORDER BY numbers.id LIMIT 2;" \
    "$DATABASE"

expect_output \
    "schema-qualified table statement" \
    "1	20	NULL	b
2	10	7	a" \
    "TABLE ${DATABASE}.numbers ORDER BY id LIMIT 2;"

expect_output \
    "limit zero" \
    "" \
    "TABLE numbers ORDER BY id LIMIT 0;" \
    "$DATABASE"

expect_output \
    "limit exact row count" \
    "1	20	NULL	b
2	10	7	a" \
    "TABLE numbers ORDER BY id LIMIT 2;" \
    "$DATABASE"

expect_output \
    "limit larger than row count" \
"1	20	NULL	b
2	10	7	a
3	15	NULL	c
4	NULL	9	d" \
    "TABLE numbers ORDER BY id LIMIT 10;" \
    "$DATABASE"

expect_output \
    "limit offset" \
"2	10	7	a
3	15	NULL	c" \
    "TABLE numbers ORDER BY id LIMIT 2 OFFSET 1;" \
    "$DATABASE"

expect_output \
    "limit comma offset" \
"2	10	7	a
3	15	NULL	c" \
    "TABLE numbers ORDER BY id LIMIT 1, 2;" \
    "$DATABASE"

expect_output \
    "sql_select_limit caps table without explicit limit" \
    "1	20	NULL	b
-1	0" \
    "SET sql_select_limit = 1; TABLE numbers ORDER BY id; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "explicit limit overrides sql_select_limit" \
    "1	20	NULL	b
2	10	7	a" \
    "SET sql_select_limit = 1; TABLE numbers ORDER BY id LIMIT 2;" \
    "$DATABASE"

temp_output=$(run_mysql \
    "CREATE TEMPORARY TABLE numbers (id INT, v INT); "\
"INSERT INTO numbers VALUES (9, 9); "\
"TABLE numbers; "\
"DROP TEMPORARY TABLE numbers; "\
"TABLE numbers ORDER BY id LIMIT 1;" \
    "$DATABASE")
expected_temp=$(cat <<'EOF'
9	9
1	20	NULL	b
EOF
)
if [ "$temp_output" != "$expected_temp" ]; then
    fail "temporary shadowing: expected [$expected_temp], got [$temp_output]"
fi

expect_error \
    "table without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "TABLE numbers;"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "TABLE ${MISSING_DATABASE}.numbers;" \
    "$DATABASE"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "TABLE missing;" \
    "$DATABASE"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "TABLE numbers ORDER BY missing;" \
    "$DATABASE"

expect_error \
    "where is not admitted" \
    1064 \
    42000 \
    "near 'WHERE id=1'" \
    "TABLE numbers WHERE id=1;" \
    "$DATABASE"

expect_error \
    "alias is not admitted" \
    1064 \
    42000 \
    "near 'AS n'" \
    "TABLE numbers AS n;" \
    "$DATABASE"

expect_error \
    "signed positive limit is syntax error" \
    1064 \
    42000 \
    "near '+1'" \
    "TABLE numbers LIMIT +1;" \
    "$DATABASE"

expect_error \
    "signed negative limit is syntax error" \
    1064 \
    42000 \
    "near '-1'" \
    "TABLE numbers LIMIT -1;" \
    "$DATABASE"

expect_error \
    "limit literal beyond unsigned range is syntax error" \
    1064 \
    42000 \
    "near '18446744073709551616'" \
    "TABLE numbers LIMIT 18446744073709551616;" \
    "$DATABASE"
