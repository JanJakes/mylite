#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_concat_ws_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_concat_ws_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<EXPECTED
a,b,c	a,c	NULL		ab	a	1,1,0,-2	é-🙂	${DATABASE}-ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	${DATABASE},x
-1	0
EXPECTED
)
expect_output \
    "scalar concat_ws values" \
    "$scalar_expected" \
    "DO 0; SELECT CONCAT_WS(',', 'a', 'b', 'c'), CONCAT_WS(',', 'a', NULL, 'c'), "\
"CONCAT_WS(NULL, 'a', 'b'), CONCAT_WS(',', NULL, NULL), CONCAT_WS('', 'a', 'b'), "\
"CONCAT_WS(',', 'a'), CONCAT_WS(',', 1, TRUE, FALSE, -2), CONCAT_WS('-', 'é', '🙂'), "\
"CONCAT_WS('-', DATABASE(), @@sql_mode), CONCAT_WS(',', (SELECT DATABASE()), 'x'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "a,b" \
    "SELECT CONCAT_WS (',', 'a', 'b') FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO CONCAT_WS('-', 'a', NULL, 'b'), CONCAT_WS(NULL, 'a'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "\
"dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL); "\
"INSERT INTO t VALUES "\
"(1, 'a', 'b  ', 'hello', 123, 12.30, 2024, '2024-01-02', '01:02:03', "\
"'2024-01-02 13:29:17', '2024-01-02 13:29:17'), "\
"(2, NULL, 'c', '', NULL, -4.50, 70, NULL, NULL, NULL, NULL), "\
"(3, 'x', NULL, NULL, -7, NULL, NULL, '2024-12-31', '00:00:00', "\
"'2024-12-31 23:59:58', '2024-12-31 23:59:58');" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	a-b-hello-123-12.30-2024-2024-01-02-01:02:03-2024-01-02 13:29:17-2024-01-02 13:29:17
2	c---4.50-1970
3	x--7-2024-12-31-00:00:00-2024-12-31 23:59:58-2024-12-31 23:59:58
EXPECTED
)
expect_output \
    "table concat_ws values" \
    "$table_expected" \
    "SELECT id, CONCAT_WS('-', v, c, txt, i, d, y, dt, tm, dttm, ts) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table envelope" \
    "3	x:-7
2	c::-4.50" \
    "SELECT id, CONCAT_WS(':', v, c, txt, i, d) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
CONCAT_WS('-', v, id)	cw
a-1	a-b
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT CONCAT_WS('-', v, id), CONCAT_WS('-', v, c) AS cw FROM t WHERE id = 1;" \
    "$DATABASE"

nested_expected=$(cat <<\EXPECTED
a1-1	a:1-A
EXPECTED
)
expect_output \
    "nested concat_ws values" \
    "$nested_expected" \
    "SELECT CONCAT_WS('-', CONCAT(v, id), id), "\
"CONCAT_WS('-', CONCAT_WS(':', v, id), UPPER(v)) FROM t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "concat_ws rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONCAT_WS'" \
    "SELECT CONCAT_WS();" \
    "$DATABASE"

expect_error \
    "concat_ws rejects one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONCAT_WS'" \
    "SELECT CONCAT_WS(',');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_concat_ws_function_expectations: ok"
