#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_safe_updates_system_variable_expectations: $1" >&2
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

expected_values="0	0	0	0	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_safe_updates, @@global.sql_safe_updates, \
     @@session.sql_safe_updates, @@local.sql_safe_updates, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_safe_updates variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_safe_updates	@@global.sql_safe_updates	@@session.\`sql_safe_updates\`	@@\`sql_safe_updates\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_safe_updates labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_safe_updates, @@global.sql_safe_updates, \
     @@session.\`sql_safe_updates\`, @@\`sql_safe_updates\`;"

expect_output \
    "case-insensitive sql_safe_updates variables" \
    "0	0" \
    "SELECT @@SQL_SAFE_UPDATES, @@Global.Sql_Safe_Updates;"

expect_output \
    "from dual returns sql_safe_updates" \
    "0" \
    "SELECT @@sql_safe_updates FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_safe_updates, @@global.sql_safe_updates; \
     SET SESSION sql_safe_updates=1; \
     SELECT @@sql_safe_updates, @@global.sql_safe_updates, @@session.sql_safe_updates, \
            @@local.sql_safe_updates, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_safe_updates=0;" \
    | tail -n 1)
expect_value \
    "mysql session sql_safe_updates is mutable upstream" \
    "1	0	1	1	0	0	0" \
    "$mutable_values"

show_values=$(run_mysql \
    "SET SESSION sql_safe_updates=1; \
     SHOW VARIABLES LIKE 'sql_safe_updates'; \
     SHOW GLOBAL VARIABLES LIKE 'sql_safe_updates'; \
     SET SESSION sql_safe_updates=0;" \
    | tr '\n' '|')
expect_value \
    "mysql show variables reflects session sql_safe_updates" \
    "sql_safe_updates	ON|sql_safe_updates	OFF|" \
    "$show_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_safe_updates, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_safe_updates variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_safe_updates, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_safe_updates variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_safe_updates variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_safe_updates_variable'" \
    "SELECT @@no_such_sql_safe_updates_variable;"

expect_error \
    "unknown scoped sql_safe_updates variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_safe_updates_variable'" \
    "SELECT @@global.no_such_sql_safe_updates_variable;"

expect_error \
    "quoted sql_safe_updates variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_safe_updates;"

expect_output \
    "mysql accepts sql_safe_updates expressions" \
    "1" \
    "SELECT @@sql_safe_updates + 1;"

run_mysql \
    "DROP DATABASE IF EXISTS mylite_sql_safe_updates_baseline; \
     CREATE DATABASE mylite_sql_safe_updates_baseline; \
     USE mylite_sql_safe_updates_baseline; \
     CREATE TABLE t ( \
       id INT PRIMARY KEY, \
       score INT, \
       category INT, \
       suffix INT, \
       KEY category_key (category), \
       KEY score_suffix_key (score, suffix) \
     ); \
     INSERT INTO t VALUES (1, 10, 100, 1), (2, 20, 200, 2), (3, 30, 300, 3);" \
    >/dev/null

expect_error \
    "safe updates rejects update without key" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     UPDATE t SET score = 99;"

expect_error \
    "safe updates rejects non-leading composite key column" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     UPDATE t SET score = 99 WHERE suffix = 1;"

expect_output \
    "safe updates allows key predicates and limit" \
    "11
22
2
3
1" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     UPDATE t SET score = 11 WHERE id = 1; \
     SELECT score FROM t WHERE id = 1; \
     UPDATE t SET score = 22 WHERE category = 200; \
     SELECT score FROM t WHERE id = 2; \
     UPDATE t SET score = 33 WHERE id = 2 OR id = 3; \
     SELECT COUNT(*) FROM t WHERE score = 33; \
     UPDATE t SET score = 55 WHERE id > 0 AND suffix > 0; \
     SELECT COUNT(*) FROM t WHERE score = 55; \
     UPDATE t SET score = 66 LIMIT 1; \
     SELECT COUNT(*) FROM t WHERE score = 66;"

run_mysql \
    "USE mylite_sql_safe_updates_baseline; \
     CREATE TABLE invisible_t (id INT, marker INT, KEY marker_key (marker)); \
     INSERT INTO invisible_t VALUES (1, 10); \
     ALTER TABLE invisible_t ALTER INDEX marker_key INVISIBLE;" \
    >/dev/null

expect_error \
    "safe updates rejects invisible index predicate" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     UPDATE invisible_t SET id = 2 WHERE marker = 10;"

expect_output \
    "safe updates allows visible index predicate" \
    "2" \
    "USE mylite_sql_safe_updates_baseline; \
     ALTER TABLE invisible_t ALTER INDEX marker_key VISIBLE; \
     SET SESSION sql_safe_updates=1; \
     UPDATE invisible_t SET id = 2 WHERE marker = 10; \
     SELECT id FROM invisible_t WHERE marker = 10;"

expect_error \
    "safe updates rejects key or nonkey predicate" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     UPDATE t SET score = 44 WHERE id = 1 OR suffix = 3;"

expect_error \
    "safe updates rejects delete without key" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     DELETE FROM t;"

expect_error \
    "safe updates rejects delete by non-leading composite key column" \
    1175 \
    HY000 \
    "safe update mode" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     DELETE FROM t WHERE suffix = 1;"

expect_output \
    "safe updates allows delete key predicates and limit" \
    "0" \
    "USE mylite_sql_safe_updates_baseline; \
     SET SESSION sql_safe_updates=1; \
     DELETE FROM t WHERE id = 1; \
     DELETE FROM t WHERE category = 200; \
     DELETE FROM t LIMIT 1; \
     SELECT COUNT(*) FROM t;"

expect_output \
    "disabled safe updates allows full table dml" \
    "2
0" \
    "USE mylite_sql_safe_updates_baseline; \
     INSERT INTO t VALUES (1, 10, 100, 1), (2, 20, 200, 2); \
     SET SESSION sql_safe_updates=0; \
     UPDATE t SET score = 70; \
     SELECT COUNT(*) FROM t WHERE score = 70; \
     DELETE FROM t; \
     SELECT COUNT(*) FROM t;"

run_mysql "DROP DATABASE IF EXISTS mylite_sql_safe_updates_baseline;" >/dev/null

printf '%s\n' "mysql_baseline_sql_safe_updates_system_variable_expectations: ok"
