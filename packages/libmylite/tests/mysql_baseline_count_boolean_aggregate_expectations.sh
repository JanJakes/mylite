#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_boolean_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_boolean_aggregate_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(id INT NOT NULL, n INT NULL);
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL);
     INSERT INTO t VALUES (1, NULL), (2, 20), (3, 20), (4, 30);" \
    >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT COUNT(TRUE), COUNT(FALSE), COUNT(true), COUNT(false); SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(TRUE), COUNT(FALSE) FROM DUAL; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(TRUE), COUNT(FALSE) FROM t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(TRUE), COUNT(FALSE) FROM empty_t; SELECT @@warning_count, ROW_COUNT();"
)
expect_value "no-source count booleans" "1	1	1	1" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "no-source count boolean status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "dual count booleans" "1	1" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "dual count boolean status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "table count booleans" "4	4" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "table count boolean status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "empty table count booleans" "0	0" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "empty table count boolean status" "0	-1" "$(printf '%s\n' "$core" | sed -n '8p')"

where_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(TRUE) FROM t WHERE n IS NULL;
     SELECT COUNT(FALSE) FROM t WHERE n IS NULL;
     SELECT COUNT(TRUE) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(FALSE) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(TRUE) FROM t WHERE id = 2;
     SELECT COUNT(FALSE) FROM t WHERE id = 2;
     SELECT COUNT(TRUE) FROM t WHERE id > 99;
     SELECT COUNT(FALSE) FROM t WHERE id > 99;"
)
expect_value "count true where nullable is null" "1" "$(printf '%s\n' "$where_counts" | sed -n '1p')"
expect_value "count false where nullable is null" "1" "$(printf '%s\n' "$where_counts" | sed -n '2p')"
expect_value "count true where nullable is not null" "3" "$(printf '%s\n' "$where_counts" | sed -n '3p')"
expect_value "count false where nullable is not null" "3" "$(printf '%s\n' "$where_counts" | sed -n '4p')"
expect_value "count true positive comparison" "1" "$(printf '%s\n' "$where_counts" | sed -n '5p')"
expect_value "count false positive comparison" "1" "$(printf '%s\n' "$where_counts" | sed -n '6p')"
expect_value "count true no-match" "0" "$(printf '%s\n' "$where_counts" | sed -n '7p')"
expect_value "count false no-match" "0" "$(printf '%s\n' "$where_counts" | sed -n '8p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(TRUE), COUNT(FALSE), COUNT(true), COUNT(false),
            COUNT(/*x*/TRUE), COUNT(/*x*/FALSE) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "count boolean labels" \
    "COUNT(TRUE)	COUNT(FALSE)	COUNT(true)	COUNT(false)	COUNT(/*x*/ TRUE)	COUNT(/*x*/ FALSE)" \
    "$headers"
expect_value "count boolean label values" "4	4	4	4	4	4" "$values"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(+TRUE), COUNT(-FALSE), COUNT(NOT TRUE), COUNT(TRUE + 1) FROM t;
     SELECT COUNT(TRUE) FROM t ORDER BY id;
     SELECT COUNT(TRUE) FROM t LIMIT 1;"
)
expect_value "deferred count boolean expr forms" "4	4	4	4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred order by aggregate" "4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred limit one returns row" "4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(TRUE) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "count whitespace before paren resolves without selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT COUNT (TRUE);"

expect_error \
    "count comment before paren resolves without selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT COUNT/**/(TRUE);"

expect_error \
    "count whitespace before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT (TRUE);"

expect_error \
    "count comment before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT/**/(TRUE);"
