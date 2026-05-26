#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_intersect_except_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_intersect_except_select_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_names() {
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
    output=$(printf '%s\n' "$output" | tr '\t' '|')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_names() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_names "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "scalar intersect distinct and row count state" \
    "1
-1|0" \
    "SELECT 1 INTERSECT SELECT 1; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "scalar intersect all keeps one matched duplicate" \
    "1" \
    "SELECT 1 INTERSECT ALL SELECT 1;" \
    "$DATABASE"

expect_output \
    "scalar except distinct and row count state" \
    "1
-1|0" \
    "SELECT 1 EXCEPT SELECT 2; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "scalar except all keeps unmatched duplicate" \
    "1" \
    "SELECT 1 EXCEPT ALL SELECT 2;" \
    "$DATABASE"

expect_output \
    "explicit distinct modifiers" \
    "1
1" \
    "SELECT 1 INTERSECT DISTINCT SELECT 1; SELECT 1 EXCEPT DISTINCT SELECT 2;" \
    "$DATABASE"

expect_output \
    "null set operation equality" \
    "1
0" \
    "SELECT COUNT(*) FROM (SELECT NULL AS x INTERSECT SELECT NULL) AS q; "\
"SELECT COUNT(*) FROM (SELECT NULL AS x EXCEPT SELECT NULL) AS q;" \
    "$DATABASE"

expect_output \
    "scalar character intersect uses default collation" \
    "a" \
    "SELECT CAST('a' AS CHAR) INTERSECT SELECT CAST('A' AS CHAR);" \
    "$DATABASE"

expect_output \
    "scalar binary intersect remains bytewise" \
    "" \
    "SELECT CAST('a' AS BINARY) INTERSECT SELECT CAST('A' AS BINARY);" \
    "$DATABASE"

expect_output \
    "scalar first branch binary forces bytewise intersect" \
    "" \
    "SELECT CAST('a' AS BINARY) INTERSECT SELECT CAST('A' AS CHAR);" \
    "$DATABASE"

expect_output \
    "scalar later branch binary forces bytewise intersect" \
    "" \
    "SELECT CAST('a' AS CHAR) INTERSECT SELECT CAST('A' AS BINARY);" \
    "$DATABASE"

expect_output \
    "scalar later branch binary forces bytewise except all" \
    "a" \
    "SELECT CAST('a' AS CHAR) EXCEPT ALL SELECT CAST('A' AS BINARY);" \
    "$DATABASE"

expect_output_with_names \
    "first branch result labels" \
    "left_label
1" \
    "SELECT 1 AS left_label INTERSECT SELECT 1 AS right_label;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE a (x INT, v VARCHAR(10), vb VARBINARY(10)); "\
"CREATE TABLE b (x INT, v VARCHAR(10), vb VARBINARY(10)); "\
"INSERT INTO a VALUES "\
"(1, 'a', 'a'), (1, 'a', 'a'), (2, 'b', 'b'), (3, 'c', 'c'), "\
"(NULL, NULL, NULL), (NULL, NULL, NULL); "\
"INSERT INTO b VALUES "\
"(1, 'A', 'A'), (2, 'b', 'b'), (2, 'b', 'b'), (NULL, NULL, NULL), (4, 'd', 'd');" \
    "$DATABASE" >/dev/null

expect_output_with_names \
    "descriptor intersect distinct" \
    "x
1
2
NULL" \
    "SELECT x FROM a INTERSECT SELECT x FROM b;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor intersect all counts matches" \
    "x
1
2
NULL" \
    "SELECT x FROM a INTERSECT ALL SELECT x FROM b;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor except distinct" \
    "x
3" \
    "SELECT x FROM a EXCEPT SELECT x FROM b;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor except all counts unmatched rows" \
    "x
1
3
NULL" \
    "SELECT x FROM a EXCEPT ALL SELECT x FROM b;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor string collation intersect" \
    "v
a" \
    "SELECT v FROM a WHERE x = 1 INTERSECT SELECT v FROM b WHERE x = 1;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor string collation except all" \
    "v
a" \
    "SELECT v FROM a WHERE x = 1 EXCEPT ALL SELECT v FROM b WHERE x = 1;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor later binary column forces bytewise intersect" \
    "" \
    "SELECT v FROM a WHERE x = 1 INTERSECT SELECT vb FROM b WHERE x = 1;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor later chain binary column forces bytewise intersect before deduplication" \
    "" \
    "SELECT v FROM a WHERE x = 1 INTERSECT SELECT v FROM b WHERE x = 1 "\
"INTERSECT SELECT vb FROM a WHERE x = 1;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor later binary column forces bytewise except all" \
    "v
a
a" \
    "SELECT v FROM a WHERE x = 1 EXCEPT ALL SELECT vb FROM b WHERE x = 1;" \
    "$DATABASE"

expect_output \
    "mixed union intersect uses broader deferred precedence" \
    "1
2" \
    "SELECT 1 UNION ALL SELECT 2 INTERSECT SELECT 2;" \
    "$DATABASE"

expect_output \
    "mixed except intersect uses broader deferred precedence" \
    "1" \
    "SELECT 1 EXCEPT SELECT 2 INTERSECT SELECT 2;" \
    "$DATABASE"

expect_output \
    "except chains group left to right" \
    "0" \
    "SELECT COUNT(*) FROM (SELECT 1 AS n EXCEPT SELECT 2 EXCEPT SELECT 1) AS q;" \
    "$DATABASE"

expect_error \
    "intersect column count mismatch" \
    1222 \
    21000 \
    "The used SELECT statements have a different number of columns" \
    "SELECT 1 INTERSECT SELECT 1, 2;" \
    "$DATABASE"

expect_error \
    "except column count mismatch" \
    1222 \
    21000 \
    "The used SELECT statements have a different number of columns" \
    "SELECT 1 EXCEPT SELECT 1, 2;" \
    "$DATABASE"

expect_error \
    "unparenthesized branch order by before intersect is syntax error" \
    1064 \
    42000 \
    "near 'INTERSECT SELECT 1'" \
    "SELECT 1 ORDER BY 1 INTERSECT SELECT 1;" \
    "$DATABASE"

expect_error \
    "unparenthesized branch limit before except is syntax error" \
    1064 \
    42000 \
    "near 'EXCEPT SELECT 1'" \
    "SELECT 1 LIMIT 1 EXCEPT SELECT 1;" \
    "$DATABASE"

expect_output_with_names \
    "global order by is a broader deferred MySQL surface" \
    "n
2" \
    "SELECT 2 AS n INTERSECT SELECT 2 ORDER BY n;" \
    "$DATABASE"

expect_output \
    "global limit is a broader deferred MySQL surface" \
    "2" \
    "SELECT 2 INTERSECT SELECT 2 LIMIT 1;" \
    "$DATABASE"
