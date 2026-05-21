#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_select_order_by_multiple_columns_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_order_by_multiple_columns_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE ordered_rows (id INT, g INT NULL, n INT, s VARCHAR(10)); "\
"INSERT INTO ordered_rows VALUES "\
"(1, 2, 10, 'b'), (2, 1, 20, 'a'), (3, 1, 10, 'b'), "\
"(4, NULL, 5, 'a'), (5, NULL, 8, 'c'); "\
"CREATE TABLE lefts (id INT, k INT); "\
"CREATE TABLE rights (id INT, k INT); "\
"INSERT INTO lefts VALUES (1, 10), (2, 20), (3, 10); "\
"INSERT INTO rights VALUES (10, 10), (11, 10), (20, 20);" \
    "$DATABASE" >/dev/null

expect_output \
    "default and later-key desc ordering" \
    "5
4
2
3
1" \
    "SELECT id FROM ordered_rows ORDER BY g, n DESC, id;" \
    "$DATABASE"

expect_output \
    "mixed directions and desc null ordering" \
    "1
3
2
4
5" \
    "SELECT id FROM ordered_rows ORDER BY g DESC, n ASC, id DESC;" \
    "$DATABASE"

expect_output \
    "order before limit" \
    "5
4
3" \
    "SELECT id FROM ordered_rows ORDER BY g ASC, id DESC LIMIT 3;" \
    "$DATABASE"

expect_output \
    "later nullable key direction" \
    "2
4
1
3
5" \
    "SELECT id FROM ordered_rows ORDER BY s ASC, g DESC, id;" \
    "$DATABASE"

expect_output \
    "alias shadows source column" \
    "5	4
8	5
10	1
10	3
20	2" \
    "SELECT n AS id, id AS source_id FROM ordered_rows ORDER BY id, source_id;" \
    "$DATABASE"

expect_output \
    "qualified joined order keys" \
    "1	11
3	11
1	10
3	10
2	20" \
    "SELECT l.id, r.id FROM lefts AS l JOIN rights AS r ON l.k = r.k "\
"ORDER BY l.k, r.id DESC, l.id;" \
    "$DATABASE"

expect_error \
    "unknown later order key" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "SELECT id FROM ordered_rows ORDER BY g, missing;" \
    "$DATABASE"

expect_error \
    "duplicate alias ambiguity" \
    1052 \
    23000 \
    "Column 'x' in order clause is ambiguous" \
    "SELECT n AS x, id AS x FROM ordered_rows ORDER BY x, id;" \
    "$DATABASE"

cleanup
printf '%s\n' "mysql_baseline_select_order_by_multiple_columns_expectations: ok"
