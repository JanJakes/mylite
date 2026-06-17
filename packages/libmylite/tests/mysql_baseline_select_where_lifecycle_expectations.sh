#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_where_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_select_where_lifecycle_expectations: $1" >&2
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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
"i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO numbers VALUES "\
"(-2, 0, -9223372036854775808, 0, NULL, 5), "\
"(1, 2, 3, 4, 9, 6), "\
"(2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7); "\
"CREATE TABLE single_values (id INT NOT NULL); "\
"INSERT INTO single_values VALUES (1); "\
"CREATE TABLE null_probe (id INT NOT NULL, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO null_probe VALUES (1, NULL, 10), (2, 20, 20); "\
"CREATE TABLE integer_aliases ("\
"id INT NOT NULL, ii INTEGER, intu INT UNSIGNED, integeru INTEGER UNSIGNED); "\
"INSERT INTO integer_aliases VALUES (1, -3, 4294967295, 7);" \
    "$DATABASE" >/dev/null

expect_error \
    "select where without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table WHERE id = 1;"

expect_error \
    "qualified select where unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.missing_table WHERE id = 1;"

expected_equal=$(cat <<'EOF'
i	nn
1	6
EOF
)
expect_output_with_headers \
    "equals predicate labels and row" \
    "$expected_equal" \
    "SELECT i, nn FROM numbers WHERE i = 1;" \
    "$DATABASE"

expect_output \
    "not equal angle predicate" \
    "1" \
    "SELECT id FROM single_values WHERE id <> 2;" \
    "$DATABASE"

expect_output \
    "not equal bang predicate no rows" \
    "" \
    "SELECT id FROM single_values WHERE id != 1;" \
    "$DATABASE"

expect_output \
    "less than predicate" \
    "-2" \
    "SELECT i FROM numbers WHERE i < 0;" \
    "$DATABASE"

expect_output \
    "less equal predicate" \
    "-2" \
    "SELECT i FROM numbers WHERE i <= -2;" \
    "$DATABASE"

expect_output \
    "greater than predicate" \
    "2147483647" \
    "SELECT i FROM numbers WHERE i > 2147483646;" \
    "$DATABASE"

expect_output \
    "greater equal predicate" \
    "2147483647" \
    "SELECT i FROM numbers WHERE i >= 2147483647;" \
    "$DATABASE"

expect_output \
    "null safe equal predicate" \
    "1" \
    "SELECT i FROM numbers WHERE i <=> 1;" \
    "$DATABASE"

expect_output \
    "is null predicate" \
    "1" \
    "SELECT id FROM null_probe WHERE n IS NULL;" \
    "$DATABASE"

expect_output \
    "is not null predicate" \
    "2" \
    "SELECT id FROM null_probe WHERE n IS NOT NULL;" \
    "$DATABASE"

expect_output \
    "not null column is null predicate" \
    "" \
    "SELECT id FROM null_probe WHERE nn IS NULL;" \
    "$DATABASE"

expect_output \
    "nullable equal predicate" \
    "1" \
    "SELECT i FROM numbers WHERE n = 9;" \
    "$DATABASE"

expect_output \
    "nullable not equal predicate excludes null rows" \
    "" \
    "SELECT i FROM numbers WHERE n <> 9;" \
    "$DATABASE"

expect_output \
    "comparison with null returns no rows upstream" \
    "" \
    "SELECT i FROM numbers WHERE n = NULL;" \
    "$DATABASE"

expect_output \
    "not equal null returns no rows upstream" \
    "" \
    "SELECT i FROM numbers WHERE n <> NULL;" \
    "$DATABASE"

expect_output \
    "parenthesized unary plus predicate" \
    "1" \
    "SELECT i FROM numbers WHERE (i = +1);" \
    "$DATABASE"

expect_output \
    "unsigned int boundary predicate" \
    "2147483647" \
    "SELECT i FROM numbers WHERE iu >= 4294967295;" \
    "$DATABASE"

expect_output \
    "unsigned negative equality predicate no rows" \
    "" \
    "SELECT i FROM numbers WHERE iu = -1;" \
    "$DATABASE"

expect_output \
    "unsigned negative range predicate" \
    "3" \
    "SELECT COUNT(*) FROM numbers WHERE iu > -1;" \
    "$DATABASE"

expect_output \
    "signed bigint minimum predicate" \
    "-2" \
    "SELECT i FROM numbers WHERE b = -9223372036854775808;" \
    "$DATABASE"

expect_output \
    "bigint unsigned signed64 maximum predicate" \
    "2147483647" \
    "SELECT i FROM numbers WHERE bu = 9223372036854775807;" \
    "$DATABASE"

expect_output \
    "integer alias signed predicate" \
    "1" \
    "SELECT id FROM integer_aliases WHERE ii = -3;" \
    "$DATABASE"

expect_output \
    "int unsigned predicate" \
    "1" \
    "SELECT id FROM integer_aliases WHERE intu = 4294967295;" \
    "$DATABASE"

expect_output \
    "integer unsigned predicate" \
    "1" \
    "SELECT id FROM integer_aliases WHERE integeru = 7;" \
    "$DATABASE"

expect_output \
    "schema-qualified filtered select" \
    "1" \
    "SELECT i FROM ${DATABASE}.numbers WHERE nn = 6;"

expect_error \
    "unknown filtered select table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "SELECT * FROM missing_table WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT missing FROM numbers WHERE i = 1;" \
    "$DATABASE"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT i FROM numbers WHERE missing = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts out of assignment range predicate upstream" \
    "3" \
    "SELECT COUNT(*) FROM numbers WHERE i < 2147483648;" \
    "$DATABASE"

expect_output \
    "mysql accepts table-qualified predicate column upstream" \
    "1" \
    "SELECT i FROM numbers WHERE numbers.i = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts literal-left predicate upstream" \
    "1" \
    "SELECT i FROM numbers WHERE 1 = i;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression predicate upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i + 1 = 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts tableless expression predicate upstream" \
    "visible" \
    "SELECT 'visible' WHERE 1 + 2 * 3 = 7;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized tableless expression predicate upstream" \
    "visible" \
    "SELECT 'visible' WHERE (1 + 2 * 3) = 7;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested tableless expression predicate upstream" \
    "visible" \
    "SELECT 'visible' WHERE ((1 + 2) * 3) = 9;" \
    "$DATABASE"

expect_output \
    "mysql accepts string predicate literal upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i = '1';" \
    "$DATABASE"

expect_output \
    "mysql accepts logical and upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i = 1 AND nn = 6;" \
    "$DATABASE"

expect_output \
    "mysql accepts logical or upstream" \
    "2" \
    "SELECT COUNT(*) FROM numbers WHERE i = 1 OR nn = 5;" \
    "$DATABASE"

expect_output \
    "mysql accepts logical not upstream" \
    "2" \
    "SELECT COUNT(*) FROM numbers WHERE NOT i = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts logical xor upstream" \
    "2" \
    "SELECT COUNT(*) FROM numbers WHERE i = 1 XOR nn = 5;" \
    "$DATABASE"

expect_output \
    "mysql accepts function predicate upstream" \
    "1" \
    "SELECT i FROM numbers WHERE ABS(i) = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts bit literal predicate upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i = b'1';" \
    "$DATABASE"

expect_output \
    "mysql accepts regexp upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i REGEXP '^1$';" \
    "$DATABASE"

expect_output \
    "mysql accepts join upstream" \
    "2" \
    "SELECT COUNT(*) FROM numbers JOIN null_probe WHERE numbers.i = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts order by upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i = 1 ORDER BY nn;" \
    "$DATABASE"

expect_output \
    "mysql accepts limit upstream" \
    "1" \
    "SELECT i FROM numbers WHERE i = 1 LIMIT 1;" \
    "$DATABASE"

printf '%s\n' "baseline-select-where-lifecycle MySQL 8.4.9 expectations verified"
