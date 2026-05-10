#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_not_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_not_predicates_expectations: $1" >&2
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
    "not predicate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table WHERE NOT id = 1;"

expect_error \
    "qualified select where not unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE NOT id = 1;"

expected_labels=$(cat <<'EOF'
id	i
1	-2
3	2147483647
4	0
EOF
)
expect_output_with_headers \
    "not labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE NOT i = 1 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "not comparison rows" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i = 1;" \
    "$DATABASE"

expect_output \
    "not null safe comparison rows" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i <=> 1;" \
    "$DATABASE"

expect_output \
    "not nullable null safe comparison rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n <=> 9;" \
    "$DATABASE"

expect_output \
    "not is null rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n IS NULL;" \
    "$DATABASE"

expect_output \
    "not is not null rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n IS NOT NULL;" \
    "$DATABASE"

expect_output \
    "not parenthesized disjunction" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT (i = 1 OR nn = 8);" \
    "$DATABASE"

expect_output \
    "not binds tighter than and and or" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i = 1 AND nn = 5 OR id = 4;" \
    "$DATABASE"

expect_output \
    "not binds tighter than or" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i = 1 OR nn = 8;" \
    "$DATABASE"

expect_output \
    "repeated not rows" \
    "2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT NOT i = 1;" \
    "$DATABASE"

expect_output \
    "unknown after not filters out rows" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n = 9;" \
    "$DATABASE"

expect_output \
    "unknown or true through not" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n = 8 OR nn = 6;" \
    "$DATABASE"

expect_output \
    "source qualified and alias negation" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(n.id ORDER BY n.id) FROM numbers AS n WHERE NOT n.i = 1;" \
    "$DATABASE"

expect_output \
    "schema table qualified negation" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT ${DATABASE}.numbers.i = 1;" \
    "$DATABASE"

expect_output \
    "distinct reuses negation" \
    "9" \
    "SELECT GROUP_CONCAT(DISTINCT n ORDER BY n) FROM numbers WHERE NOT n IS NULL;" \
    "$DATABASE"

expect_output \
    "count reuses negation" \
    "2" \
    "SELECT COUNT(*) FROM numbers WHERE NOT n IS NULL;" \
    "$DATABASE"

expect_output \
    "column aggregate reuses negation" \
    "2147483647" \
    "SELECT MAX(i) FROM numbers WHERE NOT nn = 6;" \
    "$DATABASE"

expected_grouped_rows=$(cat <<'EOF'
1	1
2	2
EOF
)
expect_output \
    "grouped aggregate source where negation" \
    "$expected_grouped_rows" \
    "SELECT tie, COUNT(*) FROM numbers WHERE NOT id = 2 GROUP BY tie ORDER BY tie;" \
    "$DATABASE"

expect_output \
    "create table select source negation" \
    "1:-2:N,3:2147483647:N,4:0:9" \
    "DROP TABLE IF EXISTS copy_numbers; "\
"CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE NOT i = 1; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', IFNULL(n, 'N')) ORDER BY id) FROM copy_numbers;" \
    "$DATABASE"

expect_output \
    "insert select source negation" \
    "1:-2,3:2147483647,4:0" \
    "DROP TABLE IF EXISTS inserted_numbers; "\
"CREATE TABLE inserted_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE NOT i = 1 ORDER BY id; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

expect_output \
    "replace select source negation" \
    "1:-2,3:2147483647,4:0" \
    "DROP TABLE IF EXISTS replaced_numbers; "\
"CREATE TABLE replaced_numbers (id INT NOT NULL, i INT); "\
"REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE NOT i = 1 ORDER BY id; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update negation affected rows" \
    "2	0	1:11,2:9,3:11,4:9" \
    "UPDATE numbers SET n = 11 WHERE NOT (i = 1 OR nn = 8); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update negation order limit" \
    "1	0	1:N,2:9,3:N,4:N" \
    "UPDATE numbers SET n = NULL WHERE NOT (n IS NULL) ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete negation affected rows" \
    "1	0	1,2,3" \
    "DELETE FROM numbers WHERE NOT (n IS NULL OR nn = 6); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete negation order limit" \
    "1	0	1,3,4" \
    "DELETE FROM numbers WHERE NOT (id = 1 OR nn >= 7) ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_error \
    "unknown predicate column under not" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE NOT missing = 1;" \
    "$DATABASE"

expect_error \
    "unknown nested predicate column under not" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE NOT (id = 1 OR missing = 1);" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts symbolic not upstream" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE !(i = 1 OR nn = 8);" \
    "$DATABASE"

expected_symbolic_not_warnings=$(cat <<'EOF'
Warning	1287	'!' is deprecated and will be removed in a future release. Please use NOT instead
EOF
)
expect_output \
    "mysql symbolic not warning" \
    "$expected_symbolic_not_warnings" \
    "DO (SELECT COUNT(*) FROM numbers WHERE !(i = 1)); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "mysql symbolic not direct precedence upstream" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE ! i = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts xor upstream" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE id = 1 XOR nn = 8;" \
    "$DATABASE"

expect_output \
    "mysql accepts bare truth upstream" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT FALSE;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression predicate upstream" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i + 1 = 2;" \
    "$DATABASE"

expect_output \
    "mysql accepts column comparison upstream" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT id = nn;" \
    "$DATABASE"

expect_output \
    "mysql accepts string literal predicate upstream" \
    "1,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i = '1';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_where_not_predicates_expectations: ok"
