#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_column_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_column_aggregate_expectations: $1" >&2
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
     CREATE TABLE t(
       id INT NOT NULL,
       i INTEGER NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL,
       n INT NULL,
       nn INT NOT NULL,
       ti TINYINT NULL,
       ti1 TINYINT(1) NULL,
       si SMALLINT NULL,
       mi MEDIUMINT NULL,
       bool_col BOOL NULL,
       boolean_col BOOLEAN NULL
     );
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
     CREATE TABLE all_null_t(id INT NOT NULL, n INT NULL);
     INSERT INTO t VALUES
       (1, -2147483648, 0, -9223372036854775808, 0, NULL, 10, -128, -1, -32768, -8388608, 1, 0),
       (2, 0, 2, -1, 2, 20, 20, 0, 0, 0, 0, NULL, 1),
       (3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, 20, 30, 127, 1, 32767, 8388607, 0, NULL),
       (4, 5, NULL, NULL, NULL, 30, 40, NULL, NULL, NULL, NULL, 1, 1);
     INSERT INTO all_null_t VALUES (1, NULL), (2, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT COUNT(id), COUNT(i), COUNT(iu), COUNT(b), COUNT(bu), COUNT(n), COUNT(nn) FROM t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(ti), COUNT(ti1), COUNT(si), COUNT(mi), COUNT(bool_col), COUNT(boolean_col) FROM t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(n) FROM empty_t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(n) FROM all_null_t; SELECT @@warning_count, ROW_COUNT();"
)
expect_value "integer family counts" "4	4	3	3	3	3	4" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer family count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "small integer alias counts" "3	3	3	3	3	3" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "small integer alias count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "empty table count column" "0" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "empty table count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "all-null table count column" "0" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "all-null table count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '8p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(n), count(n), Count( n ), COUNT(/*x*/n), (COUNT(n)), COUNT(N) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "count column labels" \
    "COUNT(n)	count(n)	Count( n )	COUNT(/*x*/ n)	(COUNT(n))	COUNT(N)" \
    "$headers"
expect_value "count column label values" "3	3	3	3	3	3" "$values"

where_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(id) FROM t WHERE n IS NULL;
     SELECT COUNT(n) FROM t WHERE n IS NULL;
     SELECT COUNT(n) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(n) FROM t WHERE id = 1;
     SELECT COUNT(n) FROM t WHERE id <> 1;
     SELECT COUNT(n) FROM t WHERE id != 1;
     SELECT COUNT(n) FROM t WHERE id < 3;
     SELECT COUNT(n) FROM t WHERE id <= 3;
     SELECT COUNT(n) FROM t WHERE id > 2;
     SELECT COUNT(n) FROM t WHERE id >= 2;
     SELECT COUNT(n) FROM t WHERE id <=> 2;
     SELECT COUNT(n) FROM t WHERE n = 20;
     SELECT COUNT(n) FROM t WHERE n <> 20;
     SELECT COUNT(n) FROM t WHERE n <=> 20;
     SELECT COUNT(n) FROM t WHERE iu = 4294967295;
     SELECT COUNT(n) FROM t WHERE b = -9223372036854775808;
     SELECT COUNT(n) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "count non-null id where n is null" "1" "$(printf '%s\n' "$where_counts" | sed -n '1p')"
expect_value "count nullable n where n is null" "0" "$(printf '%s\n' "$where_counts" | sed -n '2p')"
expect_value "count nullable n where n is not null" "3" "$(printf '%s\n' "$where_counts" | sed -n '3p')"
expect_value "where equal count column" "0" "$(printf '%s\n' "$where_counts" | sed -n '4p')"
expect_value "where not equal angle count column" "3" "$(printf '%s\n' "$where_counts" | sed -n '5p')"
expect_value "where not equal bang count column" "3" "$(printf '%s\n' "$where_counts" | sed -n '6p')"
expect_value "where less count column" "1" "$(printf '%s\n' "$where_counts" | sed -n '7p')"
expect_value "where less equal count column" "2" "$(printf '%s\n' "$where_counts" | sed -n '8p')"
expect_value "where greater count column" "2" "$(printf '%s\n' "$where_counts" | sed -n '9p')"
expect_value "where greater equal count column" "3" "$(printf '%s\n' "$where_counts" | sed -n '10p')"
expect_value "where null-safe count column" "1" "$(printf '%s\n' "$where_counts" | sed -n '11p')"
expect_value "where nullable equal count column" "2" "$(printf '%s\n' "$where_counts" | sed -n '12p')"
expect_value "where nullable not equal count column" "1" "$(printf '%s\n' "$where_counts" | sed -n '13p')"
expect_value "where nullable null-safe count column" "2" "$(printf '%s\n' "$where_counts" | sed -n '14p')"
expect_value "where unsigned int boundary count column" "1" "$(printf '%s\n' "$where_counts" | sed -n '15p')"
expect_value "where signed bigint minimum count column" "0" "$(printf '%s\n' "$where_counts" | sed -n '16p')"
expect_value "where unsigned bigint supported max count column" "1" "$(printf '%s\n' "$where_counts" | sed -n '17p')"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(1), COUNT(NULL), COUNT(DISTINCT n), COUNT(t.n) FROM t;
     SELECT COUNT(n) FROM t ORDER BY id;
     SELECT COUNT(n) FROM t LIMIT 1;"
)
expect_value "deferred count expr forms" "4	0	2	3" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred order by aggregate" "3" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred limit one returns row" "3" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "count column without source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(n);"

expect_error \
    "count column from dual unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(n) FROM DUAL;"

expect_error \
    "count column missing column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(missing) FROM t;"

expect_error \
    "count no argument syntax error" \
    1064 \
    42000 \
    "near ') FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT() FROM t;"

expect_error \
    "count two arguments syntax error" \
    1064 \
    42000 \
    "near ', id) FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT(n, id) FROM t;"

expect_error \
    "count whitespace before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT (n) FROM t;"

expect_error \
    "count comment before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.COUNT does not exist" \
    "USE ${DATABASE}; SELECT COUNT/**/(n) FROM t;"
