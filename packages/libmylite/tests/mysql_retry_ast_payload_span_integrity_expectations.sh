#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_retry_ast_spans_$$"

fail() {
    printf '%s\n' "mysql_retry_ast_payload_span_integrity_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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

create_sql=$(printf '%s' \
    "USE ${DATABASE}; CREATE TABLE payload_types (" \
    "id INT NOT NULL, i INT(11), v VARCHAR(12), c CHAR(3), t TEXT(12), " \
    "b BINARY(4), vb VARBINARY(5), bits BIT(6), y YEAR(4), " \
    "d DECIMAL(10,2), f FLOAT(9,3), dt DATETIME(4), ts TIMESTAMP(5), tm TIME(6), " \
    "PRIMARY KEY(id)) PARTITION BY HASH(id) PARTITIONS 2;"
)
metadata_sql=$(printf '%s' \
    "USE ${DATABASE}; SELECT COLUMN_NAME, COLUMN_TYPE, " \
    "COALESCE(NUMERIC_PRECISION,'NULL'), COALESCE(NUMERIC_SCALE,'NULL'), " \
    "COALESCE(DATETIME_PRECISION,'NULL'), COALESCE(CHARACTER_MAXIMUM_LENGTH,'NULL') " \
    "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() " \
    "AND TABLE_NAME='payload_types' ORDER BY ORDINAL_POSITION;"
)

expect_output \
    "deprecated type warnings" \
    "$(printf '%b' "Warning\t1681\tInteger display width is deprecated and will be removed in a future release.\nWarning\t1287\t'YEAR(4)' is deprecated and will be removed in a future release. Please use YEAR instead\nWarning\t1681\tSpecifying number of digits for floating point data types is deprecated and will be removed in a future release.")" \
    "${create_sql} SHOW WARNINGS;"

expect_output \
    "payload type metadata" \
    "$(printf '%b' "id\tint\t10\t0\tNULL\tNULL\ni\tint\t10\t0\tNULL\tNULL\nv\tvarchar(12)\tNULL\tNULL\tNULL\t12\nc\tchar(3)\tNULL\tNULL\tNULL\t3\nt\ttinytext\tNULL\tNULL\tNULL\t255\nb\tbinary(4)\tNULL\tNULL\tNULL\t4\nvb\tvarbinary(5)\tNULL\tNULL\tNULL\t5\nbits\tbit(6)\t6\tNULL\tNULL\tNULL\ny\tyear\tNULL\tNULL\tNULL\tNULL\nd\tdecimal(10,2)\t10\t2\tNULL\tNULL\nf\tfloat(9,3)\t9\t3\tNULL\tNULL\ndt\tdatetime(4)\tNULL\tNULL\t4\tNULL\nts\ttimestamp(5)\tNULL\tNULL\t5\tNULL\ntm\ttime(6)\tNULL\tNULL\t6\tNULL")" \
    "$metadata_sql"

expect_output "connection reuse" "1" "SELECT 1;"

cleanup

printf '%s\n' "mysql_retry_ast_payload_span_integrity_expectations: ok"
