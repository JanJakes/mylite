#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_ascii_ord_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_ascii_ord_functions_expectations: $1" >&2
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

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" \
    >/dev/null

scalar_expected=$(cat <<\EXPECTED
0	0	1	1	50	50	50	50	49	48	195	50089	240	4036991362	195	195	0	0	0	0	0
-1	0
EXPECTED
)
expect_output \
    "scalar ascii ord values" \
    "$scalar_expected" \
    "DO 0; SELECT ASCII(''), ORD(''), ASCII(NULL) IS NULL, ORD(NULL) IS NULL, "\
"ASCII('2'), ORD('2'), ASCII(2), ORD(2), ASCII(TRUE), ORD(FALSE), "\
"ASCII('é'), ORD('é'), ASCII('🙂'), ORD('🙂'), ASCII(X'C3A9'), ORD(X'C3A9'), "\
"ASCII(X''), ORD(X''), ASCII('\\0ab'), ORD('\\0ab'), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; CREATE TABLE t ("\
"id INT, v VARCHAR(20), txt TEXT, b VARBINARY(20), bl BLOB, b1 BIT(1), b9 BIT(9), "\
"i INT, d DECIMAL(6,2), dt DATE, ts DATETIME"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'ABC', 'Hello', X'C3A9', X'00FF', b'1', b'100000001', "\
"123, 12.30, '2024-01-02', '2024-01-02 03:04:05'), "\
"(2, 'é', 'é', X'', X'', b'0', b'000000000', -7, -4.50, NULL, NULL), "\
"(3, '🙂', '🙂', NULL, NULL, NULL, NULL, NULL, NULL, "\
"'2000-01-01', '2000-01-01 00:00:00'), "\
"(4, '', '', X'410042', X'C3A9', b'1', b'010000001', "\
"0, 0.00, '2001-02-03', '2001-02-03 04:05:06');" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	65	65	72	72	195	195	0	0	1	1	1	1	49	49	49	49	50	50	50	50
2	195	50089	195	50089	0	0	0	0	0	0	0	0	45	45	45	45	NULL	NULL	NULL	NULL
3	240	4036991362	240	4036991362	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	50	50	50	50
4	0	0	0	0	65	65	195	195	1	1	0	0	48	48	48	48	50	50	50	50
EXPECTED
)
expect_output \
    "table ascii ord values" \
    "$table_expected" \
    "SELECT id, ASCII(v), ORD(v), ASCII(txt), ORD(txt), ASCII(b), ORD(b), "\
"ASCII(bl), ORD(bl), ASCII(b1), ORD(b1), ASCII(b9), ORD(b9), ASCII(i), ORD(i), "\
"ASCII(d), ORD(d), ASCII(dt), ORD(dt), ASCII(ts), ORD(ts) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "4	0	0
3	240	4036991362" \
    "SELECT id, ASCII(v) AS a, ORD(v) AS o FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
ASCII(v)	a	ORD(v)	o
65	65	65	65
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT ASCII(v), ASCII(v) AS a, ORD(v), ORD(v) AS o FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "warning count" \
    "0" \
    "DO 0; SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "ascii rejects zero arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT ASCII();" \
    "$DATABASE"

expect_error \
    "ord rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ORD'" \
    "SELECT ORD('a', 'b');" \
    "$DATABASE"

expect_error \
    "ord rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ORD'" \
    "SELECT ORD();" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_ascii_ord_functions_expectations: ok"
