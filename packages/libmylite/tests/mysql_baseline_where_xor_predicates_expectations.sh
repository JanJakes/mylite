#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_xor_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_xor_predicates_expectations: $1" >&2
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
"id INT NOT NULL, i INT, ia INTEGER, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, "\
"n INT NULL, nn INT NOT NULL, tie INT NULL); "\
"INSERT INTO numbers VALUES "\
"(1, -2, -2, 0, -9223372036854775808, 0, NULL, 5, 1), "\
"(2, 1, 1, 2, 3, 4, 9, 6, 1), "\
"(3, 2147483647, 2147483647, 4294967295, 9223372036854775807, "\
"9223372036854775807, NULL, 7, 2), "\
"(4, 0, 0, 8, 8, 8, 9, 8, 2);" \
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
    "xor without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM numbers WHERE id = 1 XOR id = 2;"

expect_error \
    "qualified select where xor unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id = 1 XOR id = 2;"

expect_error \
    "xor unknown left predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing_left' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing_left = 1 XOR id = 2;" \
    "$DATABASE"

expect_error \
    "xor unknown right predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing_right' in 'where clause'" \
    "SELECT id FROM numbers WHERE id = 1 XOR missing_right = 2;" \
    "$DATABASE"

expected_labels=$(cat <<'EOF'
id	i
2	1
4	0
EOF
)
expect_output_with_headers \
    "xor labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i = 1 XOR nn = 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "simple xor matching rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR nn = 8;" \
    "$DATABASE"

expect_output \
    "xor null right operand behavior" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR n = 9;" \
    "$DATABASE"

expect_output \
    "xor both true and null behavior" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NULL XOR nn = 5;" \
    "$DATABASE"

expect_output \
    "and binds tighter than xor" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR nn = 8 AND n = 9;" \
    "$DATABASE"

expect_output \
    "xor binds tighter than or" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR nn = 8 OR id = 1;" \
    "$DATABASE"

expect_output \
    "and xor precedence with left conjunction" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 AND nn = 6 XOR id = 4;" \
    "$DATABASE"

expect_output \
    "parenthesized xor operands" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE (i = 1 OR nn = 8) XOR n IS NULL;" \
    "$DATABASE"

expect_output \
    "xor with in and unknown predicates" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (-2, 1) XOR n IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "prefix not binds tighter than xor" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i = 1 XOR nn = 8;" \
    "$DATABASE"

expect_output \
    "left associative repeated xor" \
    "2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = -2 XOR i = 1 XOR n IS NULL;" \
    "$DATABASE"

expect_output \
    "xor with null safe equality" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR id <=> 4;" \
    "$DATABASE"

expect_output \
    "xor with between" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN 0 AND 1 XOR n IS NULL;" \
    "$DATABASE"

expect_output \
    "xor warning count" \
    "2,4
0" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 1 XOR nn = 8; SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "distinct source reuse" \
    "9" \
    "SELECT GROUP_CONCAT(DISTINCT n ORDER BY n) FROM numbers WHERE i = 1 XOR nn = 8;" \
    "$DATABASE"

expect_output \
    "count source reuse" \
    "3" \
    "SELECT COUNT(*) FROM numbers WHERE i = 1 XOR n IS NULL;" \
    "$DATABASE"

expect_output \
    "grouped source reuse" \
    "1	1
2	1" \
    "SELECT tie, COUNT(*) FROM numbers WHERE i = 1 XOR nn = 8 GROUP BY tie ORDER BY tie;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS copy_numbers; CREATE TABLE copy_numbers AS SELECT id, n FROM numbers WHERE n IS NULL XOR i = 1;" "$DATABASE" >/dev/null
expect_output \
    "create table select source reuse" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM copy_numbers;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS inserted_numbers; CREATE TABLE inserted_numbers (id INT NOT NULL, n INT NULL);" "$DATABASE" >/dev/null
run_mysql "INSERT INTO inserted_numbers SELECT id, n FROM numbers WHERE i = 1 XOR nn = 8;" "$DATABASE" >/dev/null
expect_output \
    "insert select source reuse" \
    "2:9,4:9" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', n) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS replaced_numbers; CREATE TABLE replaced_numbers (id INT NOT NULL, n INT NULL);" "$DATABASE" >/dev/null
run_mysql "REPLACE INTO replaced_numbers SELECT id, n FROM numbers WHERE i = 1 XOR nn = 8;" "$DATABASE" >/dev/null
expect_output \
    "replace select source reuse" \
    "2:9,4:9" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', n) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update with xor predicate" \
    "3	0	1:11,2:11,3:11,4:9" \
    "UPDATE numbers SET n = 11 WHERE n IS NULL XOR i = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "ordered limited update with xor predicate" \
    "1	0	1:NULL,2:9,3:NULL,4:22" \
    "UPDATE numbers SET n = 22 WHERE i = 1 XOR nn = 8 ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "ordered limited delete with xor predicate" \
    "1	0	1,2,3" \
    "DELETE FROM numbers WHERE n IS NOT UNKNOWN XOR id = 1 ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"
