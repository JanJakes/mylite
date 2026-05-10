#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_is_boolean_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_is_boolean_predicates_expectations: $1" >&2
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
    "is true without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM numbers WHERE id IS TRUE;"

expect_error \
    "qualified select where is true unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id IS TRUE;"

expect_error \
    "is true unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing IS TRUE;" \
    "$DATABASE"

expect_error \
    "invalid is truth target" \
    1064 \
    42000 \
    "near '1' at line 1" \
    "SELECT id FROM numbers WHERE id IS 1;" \
    "$DATABASE"

expected_labels=$(cat <<'EOF'
id	i
1	-2
2	1
3	2147483647
EOF
)
expect_output_with_headers \
    "is true labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i IS TRUE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "is true matching rows" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS TRUE;" \
    "$DATABASE"

expect_output \
    "is false matching rows" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS FALSE;" \
    "$DATABASE"

expect_output \
    "is unknown matching rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "is not true matching rows" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS NOT TRUE;" \
    "$DATABASE"

expect_output \
    "nullable is not true matching rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NOT TRUE;" \
    "$DATABASE"

expect_output \
    "is not false matching rows" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS NOT FALSE;" \
    "$DATABASE"

expect_output \
    "nullable is not false matching rows" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NOT FALSE;" \
    "$DATABASE"

expect_output \
    "is not unknown matching rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IS NOT UNKNOWN;" \
    "$DATABASE"

expect_output \
    "prefix not is true" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i IS TRUE;" \
    "$DATABASE"

expect_output \
    "prefix not is unknown" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT n IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "is predicate precedence" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS TRUE AND nn = 5 OR id = 4;" \
    "$DATABASE"

expect_output \
    "is predicate with or and and precedence" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IS FALSE OR nn = 6 AND n IS TRUE;" \
    "$DATABASE"

expect_output \
    "unsigned integer is false" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE iu IS FALSE;" \
    "$DATABASE"

expect_output \
    "integer alias is true" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE ia IS TRUE;" \
    "$DATABASE"

expect_output \
    "bigint is true" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE b IS TRUE;" \
    "$DATABASE"

expect_output \
    "unsigned bigint is true" \
    "2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE bu IS TRUE;" \
    "$DATABASE"

expect_output \
    "distinct source reuse" \
    "1,2" \
    "SELECT GROUP_CONCAT(DISTINCT tie ORDER BY tie) FROM numbers WHERE i IS TRUE;" \
    "$DATABASE"

expect_output \
    "count source reuse" \
    "2" \
    "SELECT COUNT(*) FROM numbers WHERE n IS UNKNOWN;" \
    "$DATABASE"

expect_output \
    "grouped source reuse" \
    "1:2,2:1" \
    "SELECT GROUP_CONCAT(CONCAT(tie, ':', c) ORDER BY tie) FROM ("\
"SELECT tie, COUNT(*) AS c FROM numbers WHERE i IS TRUE GROUP BY tie) grouped;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS copy_numbers; "\
"CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE n IS UNKNOWN ORDER BY id;" \
    "$DATABASE" >/dev/null
expect_output \
    "create table select source reuse" \
    "1:NULL,3:NULL" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', COALESCE(n, 'NULL')) ORDER BY id) FROM copy_numbers;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS inserted_numbers; "\
"CREATE TABLE inserted_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i IS FALSE;" \
    "$DATABASE" >/dev/null
expect_output \
    "insert select source reuse" \
    "4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS replaced_numbers; "\
"CREATE TABLE replaced_numbers (id INT NOT NULL, i INT); "\
"REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE n IS NOT UNKNOWN;" \
    "$DATABASE" >/dev/null
expect_output \
    "replace select source reuse" \
    "2:1,4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update is unknown affected rows" \
    "2	0	1:11,2:9,3:11,4:9" \
    "UPDATE numbers SET n = 11 WHERE n IS UNKNOWN; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', n) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update ordered limited is true" \
    "1	0	1:NULL,2:9,3:22,4:9" \
    "UPDATE numbers SET n = 22 WHERE i IS TRUE ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', COALESCE(n, 'NULL')) "\
"ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete ordered limited is not unknown" \
    "1	0	1,2,3" \
    "DELETE FROM numbers WHERE n IS NOT UNKNOWN ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_output \
    "mysql accepts literal-left truth tests" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 1 IS TRUE;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression-left truth tests" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE id + 1 IS TRUE;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts table-qualified delete predicate" \
    "4	NULL" \
    "DELETE FROM numbers WHERE numbers.id IS TRUE; "\
"SELECT ROW_COUNT(), GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_where_is_boolean_predicates_expectations: ok"
