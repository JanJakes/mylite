#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_interval_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_interval_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
    fi
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

core_expected=$(cat <<EXPECTED
3	0	1	2	3	-1	2	1	2	1	2	0
-1	0
EXPECTED
)
expect_output \
    "core integer boolean null and duplicate values" \
    "$core_expected" \
    "DO 0; SELECT INTERVAL(23,1,15,17,30,44,200), INTERVAL(0,1,15,17), "\
"INTERVAL(1,1,15,17), INTERVAL(15,1,15,17), INTERVAL(200,1,15,17), "\
"INTERVAL(NULL,1,2), INTERVAL(TRUE,FALSE,TRUE,2), "\
"INTERVAL(FALSE,FALSE,TRUE,2), INTERVAL(1,1,1,2), "\
"INTERVAL(-5,-5,0,5), "\
"INTERVAL(9223372036854775807,0,9223372036854775807), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
INTERVAL (23,1,15,17,30,44,200)	interval_alias
3	3
EXPECTED
)
expect_output_with_headers \
    "labels and whitespace" \
    "$labels_expected" \
    "SELECT INTERVAL (23,1,15,17,30,44,200), INTERVAL(+3,1,2,3) AS interval_alias "\
"FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	-1	-1
2	1	0
3	2	1
4	2	1
5	3	2
6	3	3
EXPECTED
)
expect_output \
    "table row-scalar projection" \
    "$table_expected" \
    "CREATE TABLE t(id INT, n BIGINT NULL); "\
"INSERT INTO t VALUES (1,NULL),(2,-5),(3,0),(4,1),(5,10),(6,100); "\
"SELECT id, INTERVAL(n,-5,0,10), INTERVAL(n,0,10,20) FROM t ORDER BY id;" \
    "$DATABASE"

where_order_limit_expected=$(cat <<EXPECTED
6	3
5	3
4	2
EXPECTED
)
expect_output \
    "row envelope" \
    "$where_order_limit_expected" \
    "SELECT id, INTERVAL(n,-5,0,10) FROM t WHERE id >= 2 ORDER BY id DESC LIMIT 3;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO status" \
    "$do_expected" \
    "DO INTERVAL(2,0,1,2), INTERVAL(NULL,1,2); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

boundary_expected=$(cat <<EXPECTED
1	2
EXPECTED
)
expect_output \
    "signed boundaries" \
    "$boundary_expected" \
    "SELECT INTERVAL(-9223372036854775808,-9223372036854775808,0), "\
"INTERVAL(9223372036854775807,0,9223372036854775807);" \
    "$DATABASE"

deferred_coercion_expected=$(cat <<EXPECTED
2	1	1	2	1
Warning	1292	Truncated incorrect DOUBLE value: 'x'
EXPECTED
)
expect_output \
    "deferred MySQL coercions" \
    "$deferred_coercion_expected" \
    "SELECT INTERVAL('10','1','10','20'), INTERVAL(1.5,1,2), "\
"INTERVAL(1,NULL,2), INTERVAL(1,NULL,NULL), INTERVAL(10,'x',20); SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "zero arguments are syntax errors" \
    1064 \
    "42000" \
    "You have an error" \
    "SELECT INTERVAL();" \
    "$DATABASE"

expect_error \
    "one argument is syntax error" \
    1064 \
    "42000" \
    "You have an error" \
    "SELECT INTERVAL(1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_interval_function_expectations: ok"
