#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_sql_quote_show_create_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sql_quote_show_create_system_variable_expectations: $1" >&2
    exit 1
}

cleanup() {
    printf 'DROP DATABASE IF EXISTS `%s`;\n' "$DATABASE" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS >/dev/null 2>&1 || true
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

trap cleanup EXIT INT TERM
cleanup

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_values="1	1	1	1	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_quote_show_create, @@global.sql_quote_show_create, \
     @@session.sql_quote_show_create, @@local.sql_quote_show_create, \
     @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_quote_show_create variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_quote_show_create	@@global.sql_quote_show_create	@@session.\`sql_quote_show_create\`	@@\`sql_quote_show_create\`
1	1	1	1
EOF
)
expect_output_with_headers \
    "sql_quote_show_create labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_quote_show_create, @@global.sql_quote_show_create, \
     @@session.\`sql_quote_show_create\`, @@\`sql_quote_show_create\`;"

expect_output \
    "case-insensitive sql_quote_show_create variables" \
    "1	1" \
    "SELECT @@SQL_QUOTE_SHOW_CREATE, @@Global.Sql_Quote_Show_Create;"

expect_output \
    "from dual returns sql_quote_show_create" \
    "1" \
    "SELECT @@sql_quote_show_create FROM DUAL;"

setup_sql=$(cat <<SQL
CREATE DATABASE \`$DATABASE\`;
USE \`$DATABASE\`;
CREATE TABLE normal_name (normal_col INT);
SHOW CREATE DATABASE \`$DATABASE\`;
SHOW CREATE TABLE normal_name;
SQL
)
quoted_output=$(run_mysql_with_headers "$setup_sql" | tail -n 6)
expected_quoted=$(cat <<EOF
Database	Create Database
$DATABASE	CREATE DATABASE \`$DATABASE\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
Table	Create Table
normal_name	CREATE TABLE \`normal_name\` (
  \`normal_col\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EOF
)
expect_value "default SHOW CREATE output is quoted" "$expected_quoted" "$quoted_output"

unquoted_output=$(run_mysql_with_headers \
    "USE \`$DATABASE\`; SET SESSION sql_quote_show_create=0; \
     SELECT @@sql_quote_show_create, @@global.sql_quote_show_create, \
            @@session.sql_quote_show_create, @@local.sql_quote_show_create, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW CREATE DATABASE \`$DATABASE\`; SHOW CREATE TABLE normal_name; \
     SET SESSION sql_quote_show_create=1;" \
    | tail -n 8)
expected_unquoted=$(cat <<EOF
@@sql_quote_show_create	@@global.sql_quote_show_create	@@session.sql_quote_show_create	@@local.sql_quote_show_create	@@warning_count	@@error_count	ROW_COUNT()
0	1	0	0	0	0	0
Database	Create Database
$DATABASE	CREATE DATABASE $DATABASE /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
Table	Create Table
normal_name	CREATE TABLE normal_name (
  normal_col int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EOF
)
expect_value "mysql session sql_quote_show_create is mutable upstream" "$expected_unquoted" "$unquoted_output"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_quote_show_create, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_quote_show_create variable reads and clears warning diagnostics" \
    "1	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_quote_show_create, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_quote_show_create variable reads and clears error diagnostics" \
    "1	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_quote_show_create variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_quote_show_create_variable'" \
    "SELECT @@no_such_sql_quote_show_create_variable;"

expect_error \
    "unknown scoped sql_quote_show_create variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_quote_show_create_variable'" \
    "SELECT @@global.no_such_sql_quote_show_create_variable;"

expect_error \
    "quoted sql_quote_show_create variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_quote_show_create;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "2" \
    "SELECT @@sql_quote_show_create + 1;"
