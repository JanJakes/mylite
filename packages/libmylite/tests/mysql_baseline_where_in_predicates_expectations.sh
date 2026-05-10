#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_in_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_in_predicates_expectations: $1" >&2
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
    "in predicate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM numbers WHERE id IN (1, 2);"

expect_error \
    "qualified select where in unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id IN (1, 2);"

expect_error \
    "in unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing IN (1, 2);" \
    "$DATABASE"

expect_error \
    "empty in list" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT id FROM numbers WHERE id IN ();" \
    "$DATABASE"

expected_labels=$(cat <<'EOF'
id	i
1	-2
2	1
4	0
EOF
)
expect_output_with_headers \
    "in labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i IN (-2, 1, 0) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "in matching rows" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (-2, 1, 0);" \
    "$DATABASE"

expect_output \
    "in duplicate rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (1, 1, 0);" \
    "$DATABASE"

expect_output \
    "not in matching rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i NOT IN (-2, 1, 0);" \
    "$DATABASE"

expect_output \
    "prefix not in matching rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i IN (-2, 1, 0);" \
    "$DATABASE"

expect_output \
    "nullable tested value in rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IN (9);" \
    "$DATABASE"

expect_output \
    "null list value with match" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IN (NULL, 9);" \
    "$DATABASE"

expect_output \
    "null list value without match" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n IN (NULL, 8);" \
    "$DATABASE"

expect_output \
    "not in with null list value" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n NOT IN (NULL, 9);" \
    "$DATABASE"

expect_output \
    "not in without null list value over nullable column" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n NOT IN (9);" \
    "$DATABASE"

expect_output \
    "all null list" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (NULL);" \
    "$DATABASE"

expect_output \
    "boolean in list values" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (FALSE, TRUE);" \
    "$DATABASE"

expect_output \
    "signed plus in list values" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (+0, +1);" \
    "$DATABASE"

expect_output \
    "unsigned int in rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE iu IN (0, 4294967295);" \
    "$DATABASE"

expect_output \
    "bigint in rows" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers "\
"WHERE b IN (-9223372036854775808, 9223372036854775807);" \
    "$DATABASE"

expect_output \
    "in binds tighter than later and and or" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (-2, 1) AND nn = 5 OR id = 3;" \
    "$DATABASE"

expect_output \
    "not parenthesized in disjunction" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT (i IN (-2, 1, 0) OR id = 3);" \
    "$DATABASE"

expected_plain_warning_count=$(cat <<'EOF'
1,2,4
0
EOF
)
expect_output \
    "plain in warning count" \
    "$expected_plain_warning_count" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (-2, 1, 0); "\
"SELECT @@warning_count;" \
    "$DATABASE"

expected_symbolic_and_warning=$(cat <<'EOF'
1	1
Warning	1287	'&&' is deprecated and will be removed in a future release. Please use AND instead
EOF
)
expect_output \
    "symbolic and warning" \
    "$expected_symbolic_and_warning" \
    "SELECT GROUP_CONCAT(id ORDER BY id), @@warning_count FROM numbers "\
"WHERE i IN (-2, 1) && nn = 5; SHOW WARNINGS;" \
    "$DATABASE"

reset_numbers
run_mysql "CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE i IN (-2, 1, 0);" \
    "$DATABASE" >/dev/null
expect_output \
    "create table select in copied rows" \
    "1:-2:NULL,2:1:9,4:0:9" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', COALESCE(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM copy_numbers;" \
    "$DATABASE"

run_mysql "CREATE TABLE inserted_numbers (id INT NOT NULL, i INT);" "$DATABASE" >/dev/null
run_mysql "INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i IN (-2, 1, 0);" \
    "$DATABASE" >/dev/null
expect_output \
    "insert select in copied rows" \
    "1:-2,2:1,4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

run_mysql "CREATE TABLE replaced_numbers (id INT NOT NULL, i INT);" "$DATABASE" >/dev/null
run_mysql "REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE i IN (-2, 1, 0);" \
    "$DATABASE" >/dev/null
expect_output \
    "replace select in copied rows" \
    "1:-2,2:1,4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "UPDATE numbers SET n = 11 WHERE i IN (-2, 1, 0);" "$DATABASE" >/dev/null
expect_output \
    "update in rows" \
    "1:11,2:11,3:NULL,4:11" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', COALESCE(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "UPDATE numbers SET n = 9;" "$DATABASE" >/dev/null
run_mysql "UPDATE numbers SET n = NULL WHERE i IN (-2, 1, 0) ORDER BY id DESC LIMIT 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "update in order limit rows" \
    "1:9,2:9,3:9,4:NULL" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', COALESCE(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "DELETE FROM numbers WHERE i IN (-2, 1, 0);" "$DATABASE" >/dev/null
expect_output \
    "delete in remaining rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "DELETE FROM numbers WHERE i IN (-2, 1, 0) ORDER BY id DESC LIMIT 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "delete in order limit remaining rows" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

printf '%s\n' 'mysql_baseline_where_in_predicates_expectations: ok'
