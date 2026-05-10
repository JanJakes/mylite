#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_between_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_between_predicates_expectations: $1" >&2
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
    "between predicate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM numbers WHERE id BETWEEN 1 AND 2;"

expect_error \
    "qualified select where between unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id BETWEEN 1 AND 2;"

expect_error \
    "between unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing BETWEEN 1 AND 2;" \
    "$DATABASE"

expected_labels=$(cat <<'EOF'
id	i
1	-2
2	1
4	0
EOF
)
expect_output_with_headers \
    "between labels and rows" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "between inclusive rows" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE"

expect_output \
    "between reversed rows" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN 1 AND -2;" \
    "$DATABASE"

expect_output \
    "between nullable rows" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n BETWEEN 1 AND 9;" \
    "$DATABASE"

expect_output \
    "not between nullable rows" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n NOT BETWEEN 1 AND 9;" \
    "$DATABASE"

expect_output \
    "not between rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i NOT BETWEEN -2 AND 1;" \
    "$DATABASE"

expect_output \
    "prefix not between rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT i BETWEEN -2 AND 1;" \
    "$DATABASE"

expect_output \
    "not parenthesized between disjunction" \
    "NULL" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NOT (i BETWEEN -2 AND 1 OR id = 3);" \
    "$DATABASE"

expect_output \
    "between binds tighter than later and and or" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN -2 AND 1 AND nn = 5 OR id = 3;" \
    "$DATABASE"

expect_output \
    "boolean between bounds" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN FALSE AND TRUE;" \
    "$DATABASE"

expect_output \
    "signed plus between bounds" \
    "2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN +0 AND +1;" \
    "$DATABASE"

expect_output \
    "unsigned int between rows" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE iu BETWEEN 0 AND 2;" \
    "$DATABASE"

expect_output \
    "bigint between rows" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers "\
"WHERE b BETWEEN -9223372036854775808 AND 8;" \
    "$DATABASE"

expect_output \
    "unsigned bigint between rows" \
    "1,2,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE bu BETWEEN 0 AND 8;" \
    "$DATABASE"

expected_plain_warning_count=$(cat <<'EOF'
1,2,4
0
EOF
)
expect_output \
    "between warning count" \
    "$expected_plain_warning_count" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN -2 AND 1; "\
"SELECT @@warning_count;" \
    "$DATABASE"

expected_and_warning=$(cat <<'EOF'
1
Warning	1287	'&&' is deprecated and will be removed in a future release. Please use AND instead
EOF
)
expect_output \
    "between symbolic and warning" \
    "$expected_and_warning" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN -2 AND 1 && nn = 5; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "distinct source between predicate" \
    "NULL
9" \
    "SELECT DISTINCT n FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY n;" \
    "$DATABASE"

expect_output \
    "count source between predicate" \
    "3" \
    "SELECT COUNT(*) FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE"

expect_output \
    "aggregate source between predicate" \
    "1" \
    "SELECT MAX(i) FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE"

expect_output \
    "grouped aggregate source between predicate" \
    "1	2
2	1" \
    "SELECT tie, COUNT(*) FROM numbers WHERE i BETWEEN -2 AND 1 GROUP BY tie ORDER BY tie;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE IF EXISTS copy_between; "\
"CREATE TABLE copy_between SELECT id, i, n FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "create table select between rows" \
    "1:-2:NULL,2:1:9,4:0:9" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', IFNULL(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM copy_between;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE IF EXISTS inserted_between; "\
"CREATE TABLE inserted_between (id INT NOT NULL, i INT); "\
"INSERT INTO inserted_between SELECT id, i FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "insert select between rows" \
    "1:-2,2:1,4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_between;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE IF EXISTS replaced_between; "\
"CREATE TABLE replaced_between (id INT NOT NULL, i INT); "\
"REPLACE INTO replaced_between SELECT id, i FROM numbers WHERE i BETWEEN -2 AND 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "replace select between rows" \
    "1:-2,2:1,4:0" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_between;" \
    "$DATABASE"

reset_numbers
run_mysql "UPDATE numbers SET n = 11 WHERE i BETWEEN -2 AND 1;" "$DATABASE" >/dev/null
expect_output \
    "update between rows" \
    "1:11,2:11,3:NULL,4:11" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "UPDATE numbers SET n = NULL WHERE i BETWEEN -2 AND 1 ORDER BY id DESC LIMIT 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "update ordered limited between rows" \
    "1:NULL,2:9,3:NULL,4:NULL" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(CAST(n AS CHAR), 'NULL')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "DELETE FROM numbers WHERE i BETWEEN -2 AND 1;" "$DATABASE" >/dev/null
expect_output \
    "delete between remaining rows" \
    "3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
run_mysql "DELETE FROM numbers WHERE i BETWEEN -2 AND 1 ORDER BY id DESC LIMIT 1;" \
    "$DATABASE" >/dev/null
expect_output \
    "delete ordered limited between remaining rows" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_where_between_predicates_expectations: ok"
