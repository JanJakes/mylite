#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_or_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_or_predicates_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

reset_numbers() {
    run_mysql \
        "DROP TABLE IF EXISTS numbers; "\
"CREATE TABLE numbers ("\
"id INT NOT NULL, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, "\
"n INT NULL, nn INT NOT NULL, tie INT NULL); "\
"INSERT INTO numbers VALUES "\
"(1, -2, 0, -9223372036854775808, 0, NULL, 5, 1), "\
"(2, 1, 2, 3, 4, 9, 6, 1), "\
"(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7, 2), "\
"(4, 0, 8, 8, 8, 9, 8, 2);" \
        "$DATABASE" >/dev/null
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
reset_numbers

expect_error \
    "or predicate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table WHERE id = 1 OR i = 2;"

expect_error \
    "qualified select where or unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id = 1 OR i = 2;"

expected_labels=$(cat <<'EOF'
id	i
2	1
4	0
EOF
)
expect_output_with_headers \
    "or labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "logical or symbol rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 || nn = 8;" \
    "$DATABASE"

expected_symbol_warnings=$(cat <<'EOF'
Warning	1287	'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
Warning	1287	'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
EOF
)
expect_output \
    "logical or symbol warning per token" \
    "$expected_symbol_warnings" \
    "DO (SELECT COUNT(*) FROM numbers WHERE id = 2 || i = 1 || nn = 6); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "and binds tighter than or" \
    "2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 OR nn = 8 AND n IS NULL;" \
    "$DATABASE"

expect_output \
    "parentheses override or precedence" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE (i = 1 OR nn = 8) AND n IS NULL;" \
    "$DATABASE"

expect_output \
    "parenthesized or feeds conjunction" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE (n IS NULL OR nn = 6) AND id > 1;" \
    "$DATABASE"

expect_output \
    "is null or comparison" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NULL OR nn = 6;" \
    "$DATABASE"

expect_output \
    "nullable values or nullable values" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NULL OR n = 9;" \
    "$DATABASE"

expect_output \
    "false or unknown outcomes filter out rows" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n = 8 OR nn = 9;" \
    "$DATABASE"

expect_output \
    "unknown or true includes true rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n = 9 OR nn = 6;" \
    "$DATABASE"

expect_output \
    "nested disjunctions and conjunctions" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE (n = 9 OR nn = 6) AND (id = 2 OR id = 4);" \
    "$DATABASE"

expect_output \
    "unsigned and signed boundary disjunction" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE iu = 4294967295 OR b = -9223372036854775808;" \
    "$DATABASE"

expect_output \
    "source qualified and alias disjunction" \
    "2,4" \
    "SELECT GROUP_CONCAT(n.id ORDER BY n.id) FROM numbers AS n WHERE n.i = 1 OR n.nn = 8;" \
    "$DATABASE"

expect_output \
    "schema table qualified disjunction" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE ${DATABASE}.numbers.i = 1 OR numbers.nn = 8;" \
    "$DATABASE"

expect_output \
    "distinct reuses disjunction" \
    "9" \
    "SELECT GROUP_CONCAT(DISTINCT n ORDER BY n) FROM numbers WHERE n IS NOT NULL OR i = 2147483647;" \
    "$DATABASE"

expect_output \
    "count reuses disjunction" \
    "4" \
    "SELECT COUNT(*) FROM numbers WHERE n IS NULL OR n = 9;" \
    "$DATABASE"

expect_output \
    "column aggregate reuses disjunction" \
    "2147483647" \
    "SELECT MAX(i) FROM numbers WHERE nn = 6 OR n IS NULL;" \
    "$DATABASE"

expected_grouped_rows=$(cat <<'EOF'
1	1
2	2
EOF
)
expect_output \
    "grouped aggregate source where disjunction" \
    "$expected_grouped_rows" \
    "SELECT tie, COUNT(*) FROM numbers WHERE id = 1 OR nn >= 7 GROUP BY tie ORDER BY tie;" \
    "$DATABASE"

expect_output \
    "create table select source disjunction" \
    "2:1:9,4:0:9" \
    "DROP TABLE IF EXISTS copy_numbers; "\
"CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE i = 1 OR nn = 8; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', IFNULL(n, 'N')) ORDER BY id) FROM copy_numbers;" \
    "$DATABASE"

expect_output \
    "insert select source disjunction" \
    "2:1,4:0" \
    "DROP TABLE IF EXISTS inserted_numbers; "\
"CREATE TABLE inserted_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

expect_output \
    "replace select source disjunction" \
    "2:1,4:0" \
    "DROP TABLE IF EXISTS replaced_numbers; "\
"CREATE TABLE replaced_numbers (id INT NOT NULL, i INT); "\
"REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update disjunction affected rows" \
    "2	0	1:N,2:11,3:N,4:11" \
    "UPDATE numbers SET n = 11 WHERE i = 1 OR nn = 8; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update disjunction order limit" \
    "2	0	1:N,2:9,3:99,4:99" \
    "UPDATE numbers SET n = 99 WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete disjunction affected rows" \
    "2	0	1,3" \
    "DELETE FROM numbers WHERE i = 1 OR nn = 8; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete disjunction order limit" \
    "2	0	1,2" \
    "DELETE FROM numbers WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_error \
    "unknown first predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing = 1 OR id = 1;" \
    "$DATABASE"

expect_error \
    "unknown second predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE id = 1 OR missing = 1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts xor upstream" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE id = 1 XOR nn = 8;" \
    "$DATABASE"

expect_output \
    "mysql accepts not upstream" \
    "2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts bare truth upstream" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE TRUE OR id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression predicate upstream" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i + 1 = 2 OR id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts column comparison upstream" \
    "2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE id = nn OR id = 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts string literal predicate upstream" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = '1' OR id = 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_where_or_predicates_expectations: ok"
