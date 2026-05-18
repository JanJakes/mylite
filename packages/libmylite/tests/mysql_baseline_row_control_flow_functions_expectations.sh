#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_row_control_flow_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_control_flow_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE t ("\
"id INT, v VARCHAR(20), n VARCHAR(20), i INT, nn INT NOT NULL, "\
"d DATE, dt DATETIME, txt TEXT); "\
"INSERT INTO t VALUES "\
"(1, 'a', NULL, 7, 1, '2024-01-02', '2024-01-02 03:04:05', 'alpha'), "\
"(2, NULL, 'fallback', NULL, 0, NULL, NULL, NULL), "\
"(3, 'A', 'a', 0, 5, '2024-12-31', '2024-12-31 23:59:58', 'beta');" >/dev/null

projection_expected=$(cat <<\EXPECTED
1	a	a	a	0	a
2	x	fallback	NULL	1	fallback
3	A	a	NULL	0	A
EXPECTED
)
expect_output \
    "string control-flow projection" \
    "$projection_expected" \
    "SELECT id, IFNULL(v,'x'), COALESCE(n,v,'z'), NULLIF(v,n), ISNULL(v), "\
"IF(nn, v, n) FROM t ORDER BY id;" \
    "$DATABASE"

integer_expected=$(cat <<\EXPECTED
1	7	7	7	0	yes
2	-1	0	NULL	1	no
3	0	0	NULL	0	no
EXPECTED
)
expect_output \
    "integer control-flow projection" \
    "$integer_expected" \
    "SELECT id, IFNULL(i,-1), COALESCE(i,nn,99), NULLIF(i,0), ISNULL(i), "\
"IF(i, 'yes', 'no') FROM t ORDER BY id;" \
    "$DATABASE"

temporal_expected=$(cat <<\EXPECTED
1	2024-01-02	2024-01-02 03:04:05	alpha
2	2000-01-01	2000-01-01 00:00:00	missing
3	2024-12-31	2024-12-31 23:59:58	beta
EXPECTED
)
expect_output \
    "temporal and text control-flow projection" \
    "$temporal_expected" \
    "SELECT id, IFNULL(d,'2000-01-01'), "\
"COALESCE(dt,'2000-01-01 00:00:00'), IFNULL(txt,'missing') FROM t ORDER BY id;" \
    "$DATABASE"

envelope_expected=$(cat <<\EXPECTED
3	A	0
2	x	0
EXPECTED
)
expect_output \
    "row envelope control-flow projection" \
    "$envelope_expected" \
    "SELECT id, IFNULL(v,'x'), ISNULL(n) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

nested_expected=$(cat <<\EXPECTED
1	a
2	fallback
3	a
EXPECTED
)
expect_output \
    "nested row control-flow projection" \
    "$nested_expected" \
    "SELECT id, IFNULL(NULLIF(v,n), COALESCE(n,'z')) FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
IFNULL(v,'x')	alias_name	ISNULL(n)
a	a	1
EXPECTED
)
expect_output_with_headers \
    "control-flow result labels" \
    "$labels_expected" \
    "SELECT IFNULL(v,'x'), COALESCE(n,v) AS alias_name, ISNULL(n) "\
"FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "row count and warnings" \
    "a
-1	0" \
    "SELECT IFNULL(v,'x') FROM t WHERE id = 1; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_control_flow_functions_expectations: ok"
