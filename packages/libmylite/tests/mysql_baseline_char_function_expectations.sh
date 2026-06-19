#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_char_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_char_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" "${MYSQL_BIN:-mysql}" --protocol=TCP -h127.0.0.1 \
            -uroot --batch --raw --skip-column-names "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" >/dev/null

scalar_expected=$(cat <<EXPECTED
41	binary	binary	1	0	0
4D7953514C	0
	0	0	0
4142	2	0
0100	2	0
00	1	0
0100	010000	FFFFFFFF	0
FFFFFFFF	FFFFFF00	0
-1	0
EXPECTED
)
expect_output \
    "scalar CHAR bytes" \
    "$scalar_expected" \
    "DO 0; SELECT HEX(CHAR(65)), CHARSET(CHAR(65)), COLLATION(CHAR(65)), "\
"LENGTH(CHAR(65)), CHAR(65) IS NULL, @@warning_count; "\
"SELECT HEX(CHAR(77,121,83,81,76)), @@warning_count; "\
"SELECT HEX(CHAR(NULL)), LENGTH(CHAR(NULL)), CHAR(NULL) IS NULL, @@warning_count; "\
"SELECT HEX(CHAR(65,NULL,66)), LENGTH(CHAR(65,NULL,66)), @@warning_count; "\
"SELECT HEX(CHAR(TRUE,FALSE)), LENGTH(CHAR(TRUE,FALSE)), @@warning_count; "\
"SELECT HEX(CHAR(0)), LENGTH(CHAR(0)), @@warning_count; "\
"SELECT HEX(CHAR(256)), HEX(CHAR(65536)), HEX(CHAR(4294967295)), @@warning_count; "\
"SELECT HEX(CHAR(-1)), HEX(CHAR(-256)), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

charset_expected=$(cat <<EXPECTED
41	41	41	utf8mb4	utf8mb4_0900_ai_ci	4	0
C3A9	2	1	0
41	42
2
41	0
1
NULL	1
2
EXPECTED
)
expect_output \
    "scalar CHAR USING charset" \
    "$charset_expected" \
    "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; "\
"SELECT HEX(CHAR(65 USING binary)), HEX(CHAR(65 USING utf8mb4)), "\
"HEX(CHAR(65 USING latin1)), CHARSET(CHAR(65 USING utf8mb4)), "\
"COLLATION(CHAR(65 USING utf8mb4)), COERCIBILITY(CHAR(65 USING utf8mb4)), "\
"@@warning_count; "\
"SELECT HEX(CHAR(195,169 USING utf8mb4)), LENGTH(CHAR(195,169 USING utf8mb4)), "\
"CHAR_LENGTH(CHAR(195,169 USING utf8mb4)), @@warning_count; "\
"SELECT HEX(CHAR(65 USING utf8)), HEX(CHAR(66 USING utf8mb3)); "\
"SELECT @@warning_count; "\
"SELECT HEX(CHAR(65,255,66 USING utf8mb4)), CHAR(65,255,66 USING utf8mb4) IS NULL; "\
"SELECT @@warning_count; "\
"SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"SELECT HEX(CHAR(65,255,66 USING utf8mb4)), CHAR(65,255,66 USING utf8mb4) IS NULL; "\
"SELECT @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t(id INT, n INT, b BIGINT, nullable INT); "\
"INSERT INTO t VALUES "\
"(1, 65, 256, NULL), "\
"(2, -1, 65536, 66), "\
"(3, 0, 4294967295, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	41	0100		0
2	FFFFFFFF	010000	42	0
3	00	FFFFFFFF		0
3	00
2	FFFFFFFF
EXPECTED
)
expect_output \
    "table CHAR bytes" \
    "$table_expected" \
    "SELECT id, HEX(CHAR(n)), HEX(CHAR(b)), HEX(CHAR(nullable)), "\
"CHAR(nullable) IS NULL FROM t ORDER BY id; "\
"SELECT id, HEX(CHAR(n)) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_error \
    "CHAR zero arguments" \
    1064 \
    "42000" \
    "near ')'" \
    "SELECT CHAR();" \
    "$DATABASE"

expect_upstream_accepts \
    "CHAR string coercion is accepted by MySQL but deferred by MyLite" \
    "SELECT HEX(CHAR('77')), @@warning_count; SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "CHAR decimal coercion is accepted by MySQL but deferred by MyLite" \
    "SELECT HEX(CHAR(77.3)), @@warning_count; SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "CHAR table-backed charset USING is accepted by MySQL but deferred by MyLite" \
    "SELECT HEX(CHAR(n USING utf8mb4)) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_char_function_expectations: ok"
