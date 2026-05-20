#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
MYSQL_DATABASE="mylite_sql_select_limit_expectations"
MYSQL_DEFAULT_SQL_SELECT_LIMIT="18446744073709551615"

fail() {
    printf '%s\n' "mysql_baseline_sql_select_limit_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS $MYSQL_DATABASE; CREATE DATABASE $MYSQL_DATABASE;"

expected_values="$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_select_limit, @@global.sql_select_limit, \
     @@session.sql_select_limit, @@local.sql_select_limit, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_select_limit variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_select_limit	@@global.sql_select_limit	@@session.\`sql_select_limit\`	@@\`sql_select_limit\`
$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT
EOF
)
expect_output_with_headers \
    "sql_select_limit labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_select_limit, @@global.sql_select_limit, \
     @@session.\`sql_select_limit\`, @@\`sql_select_limit\`;"

expect_output \
    "case-insensitive sql_select_limit variables" \
    "$MYSQL_DEFAULT_SQL_SELECT_LIMIT	$MYSQL_DEFAULT_SQL_SELECT_LIMIT" \
    "SELECT @@SQL_SELECT_LIMIT, @@Global.Sql_Select_Limit;"

expect_output \
    "from dual returns sql_select_limit" \
    "$MYSQL_DEFAULT_SQL_SELECT_LIMIT" \
    "SELECT @@sql_select_limit FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_select_limit, @@global.sql_select_limit; \
     SET SESSION sql_select_limit=1; \
     SELECT @@sql_select_limit, @@global.sql_select_limit, @@session.sql_select_limit, \
            @@local.sql_select_limit, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_select_limit is mutable upstream" \
    "1	$MYSQL_DEFAULT_SQL_SELECT_LIMIT	1	1	0	0	0" \
    "$mutable_values"

hex_values=$(run_mysql \
    "SET SESSION sql_select_limit=2; \
     SELECT HEX(@@sql_select_limit), HEX(@@global.sql_select_limit); \
     SET SESSION sql_select_limit=DEFAULT;")
expect_value \
    "mysql numeric functions observe mutable sql_select_limit" \
    "2	FFFFFFFFFFFFFFFF" \
    "$hex_values"

limited_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     DROP TABLE IF EXISTS t; \
     CREATE TABLE t (id INT); \
     CREATE TABLE a_probe (id INT); \
     CREATE TABLE b_probe (id INT); \
     INSERT INTO t VALUES (1),(2),(3); \
     SET SESSION sql_select_limit=1; \
     SELECT id FROM t ORDER BY id; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value "mysql sql_select_limit caps selects without explicit limit" "1" "$limited_rows"

explicit_limit_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=1; \
     SELECT id FROM t ORDER BY id LIMIT 2; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "mysql explicit limit overrides sql_select_limit" \
    "1|2|" \
    "$explicit_limit_rows"

internal_source_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     DROP TABLE IF EXISTS insert_copy; \
     DROP TABLE IF EXISTS ctas_copy; \
     CREATE TABLE insert_copy (id INT); \
     SET SESSION sql_select_limit=1; \
     INSERT INTO insert_copy SELECT id FROM t ORDER BY id; \
     SELECT ROW_COUNT(), COUNT(*) FROM insert_copy; \
     CREATE TABLE ctas_copy AS SELECT id FROM t ORDER BY id; \
     SELECT COUNT(*) FROM ctas_copy; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "mysql sql_select_limit does not cap insert-select or create-table-select source rows" \
    "3	3|3|" \
    "$internal_source_rows"

accepted_assignment_values=$(run_mysql \
    "SET @@SESSION.sql_select_limit=+2; \
     SELECT @@sql_select_limit LIMIT 1; \
     SET LOCAL sql_select_limit=TRUE; \
     SELECT @@sql_select_limit LIMIT 1; \
     SET @@sql_select_limit=FALSE; \
     SELECT @@sql_select_limit LIMIT 1; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tr '\n' '|')
expect_value \
    "mysql accepts qualified signed and boolean sql_select_limit assignments" \
    "2|1|0|" \
    "$accepted_assignment_values"

negative_assignment_values=$(run_mysql \
    "SET SESSION sql_select_limit=-1; \
     SELECT @@sql_select_limit, @@warning_count LIMIT 1; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | head -n 1)
expect_value \
    "mysql negative sql_select_limit clamps to zero with warning" \
    "0	1" \
    "$negative_assignment_values"

user_variable_values=$(run_mysql \
    "SET @limit_value=1; \
     SET SESSION sql_select_limit=@limit_value; \
     SELECT @@sql_select_limit LIMIT 1; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql accepts integer user variable sql_select_limit assignment" \
    "1" \
    "$user_variable_values"

expect_error \
    "mysql rejects string user variable sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET @limit_text='2'; SET SESSION sql_select_limit=@limit_text;"

zero_limit_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=0; \
     SELECT id FROM t ORDER BY id; \
     SET SESSION sql_select_limit=DEFAULT;")
expect_value "mysql sql_select_limit zero returns no select rows" "" "$zero_limit_rows"

expect_output \
    "mysql sql_select_limit zero caps scalar selects" \
    "" \
    "SET SESSION sql_select_limit=0; SELECT 1; SET SESSION sql_select_limit=DEFAULT;"

expect_output \
    "mysql sql_select_limit zero caps aggregate selects" \
    "" \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=0; \
     SELECT COUNT(*) FROM t; \
     SET SESSION sql_select_limit=DEFAULT;"

grouped_limited_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=1; \
     SELECT id, COUNT(*) FROM t GROUP BY id ORDER BY id; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value "mysql sql_select_limit caps grouped selects" "1	1" "$grouped_limited_rows"

union_limited_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=1; \
     SELECT id FROM t WHERE id = 1 UNION ALL SELECT id FROM t WHERE id = 2; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value "mysql sql_select_limit caps final union rows" "1" "$union_limited_rows"

information_schema_limited_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=1; \
     SELECT TABLE_NAME FROM information_schema.tables \
       WHERE TABLE_SCHEMA = '$MYSQL_DATABASE' \
       AND TABLE_NAME IN ('a_probe', 'b_probe') \
       ORDER BY TABLE_NAME; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql sql_select_limit caps information_schema selects" \
    "a_probe" \
    "$information_schema_limited_rows"

information_schema_explicit_limit_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION sql_select_limit=1; \
     SELECT TABLE_NAME FROM information_schema.tables \
       WHERE TABLE_SCHEMA = '$MYSQL_DATABASE' \
       AND TABLE_NAME IN ('a_probe', 'b_probe') \
       ORDER BY TABLE_NAME LIMIT 2; \
     SET SESSION sql_select_limit=DEFAULT;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "mysql information_schema explicit limit overrides sql_select_limit" \
    "a_probe|b_probe|" \
    "$information_schema_explicit_limit_rows"

expect_error \
    "mysql rejects string sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET SESSION sql_select_limit='2';"

expect_error \
    "mysql rejects decimal sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET SESSION sql_select_limit=1.5;"

expect_error \
    "mysql rejects null sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET SESSION sql_select_limit=NULL;"

expect_error \
    "mysql rejects on sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET SESSION sql_select_limit=ON;"

expect_error \
    "mysql rejects overflowing sql_select_limit assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'sql_select_limit'" \
    "SET SESSION sql_select_limit=18446744073709551616;"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_select_limit, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_select_limit variable reads and clears warning diagnostics" \
    "$MYSQL_DEFAULT_SQL_SELECT_LIMIT	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_select_limit, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_select_limit variable reads and clears error diagnostics" \
    "$MYSQL_DEFAULT_SQL_SELECT_LIMIT	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_select_limit variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_select_limit_variable'" \
    "SELECT @@no_such_sql_select_limit_variable;"

expect_error \
    "unknown scoped sql_select_limit variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_select_limit_variable'" \
    "SELECT @@global.no_such_sql_select_limit_variable;"

expect_error \
    "quoted sql_select_limit variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_select_limit;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "$MYSQL_DEFAULT_SQL_SELECT_LIMIT" \
    "SELECT @@sql_select_limit + 0;"

run_mysql "DROP DATABASE IF EXISTS $MYSQL_DATABASE;"
