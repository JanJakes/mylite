#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_literal_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_literal_aggregate_expectations: $1" >&2
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
     CREATE TABLE t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL);
     INSERT INTO t VALUES (1, NULL, 10), (2, 20, 20), (3, 20, 30), (4, 30, 40);" \
    >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT COUNT(1), COUNT(0), COUNT(-1), COUNT(+1), COUNT(NULL); SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(1), COUNT(0), COUNT(-1), COUNT(+1), COUNT(NULL) FROM DUAL; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(1), COUNT(0), COUNT(-1), COUNT(+1), COUNT(NULL) FROM t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(1), COUNT(NULL) FROM empty_t; SELECT @@warning_count, ROW_COUNT();"
)
expect_value "no-source count literals" "1	1	1	1	0" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "no-source count literal status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "dual count literals" "1	1	1	1	0" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "dual count literal status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "table count literals" "4	4	4	4	0" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "table count literal status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "empty table count literals" "0	0" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "empty table count literal status" "0	-1" "$(printf '%s\n' "$core" | sed -n '8p')"

where_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(1) FROM t WHERE n IS NULL;
     SELECT COUNT(NULL) FROM t WHERE n IS NULL;
     SELECT COUNT(1) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(NULL) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(1) FROM t WHERE id = 2;
     SELECT COUNT(NULL) FROM t WHERE id = 2;
     SELECT COUNT(1) FROM t WHERE id > 99;
     SELECT COUNT(NULL) FROM t WHERE id > 99;"
)
expect_value "count literal where nullable is null" "1" "$(printf '%s\n' "$where_counts" | sed -n '1p')"
expect_value "count null where nullable is null" "0" "$(printf '%s\n' "$where_counts" | sed -n '2p')"
expect_value "count literal where nullable is not null" "3" "$(printf '%s\n' "$where_counts" | sed -n '3p')"
expect_value "count null where nullable is not null" "0" "$(printf '%s\n' "$where_counts" | sed -n '4p')"
expect_value "count literal positive comparison" "1" "$(printf '%s\n' "$where_counts" | sed -n '5p')"
expect_value "count null positive comparison" "0" "$(printf '%s\n' "$where_counts" | sed -n '6p')"
expect_value "count literal no-match" "0" "$(printf '%s\n' "$where_counts" | sed -n '7p')"
expect_value "count null no-match" "0" "$(printf '%s\n' "$where_counts" | sed -n '8p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(1), COUNT(0), COUNT(-1), COUNT(+1), COUNT(NULL), COUNT(/*x*/1),
            COUNT(/*x*/-1), COUNT(/*x*/+1), COUNT(/*x*/NULL) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "count literal labels" \
    "COUNT(1)	COUNT(0)	COUNT(-1)	COUNT(+1)	COUNT(NULL)	COUNT(/*x*/ 1)	COUNT(/*x*/ -1)	COUNT(/*x*/ +1)	COUNT(/*x*/ NULL)" \
    "$headers"
expect_value "count literal label values" "4	4	4	4	0	4	4	4	0" "$values"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(1.0), COUNT('x'), COUNT(TRUE), COUNT(FALSE), COUNT(id + 1) FROM t;
     SELECT COUNT(1) FROM t ORDER BY id;
     SELECT COUNT(1) FROM t LIMIT 1;"
)
expect_value "deferred count expr forms" "4	4	4	4	4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred order by aggregate" "4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred limit one returns row" "4" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"

large_literal=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(999999999999999999999999999999999999999999999999999999999999999);"
)
expect_value "large count literal" "1" "$large_literal"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(1) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "count whitespace before paren resolves without selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT COUNT (1);"

expect_error \
    "count comment before paren resolves without selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT COUNT/**/(1);"

expect_error \
    "count whitespace before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT (1);"

expect_error \
    "count comment before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT/**/(1);"
