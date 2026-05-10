#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_min_max_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_min_max_aggregate_expectations: $1" >&2
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
     CREATE TABLE t(id INT NOT NULL, n INT NULL, nn INT NOT NULL, u INT UNSIGNED NULL, b BIGINT NULL, bu BIGINT UNSIGNED NULL);
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
     CREATE TABLE null_t(n INT NULL, b BIGINT NULL);
     INSERT INTO t VALUES
       (1, NULL, 10, 1, -1, 1),
       (2, 20, 20, 2, 0, 2),
       (3, 20, 30, 4294967295, 9223372036854775807, 9223372036854775807),
       (4, 30, 40, NULL, NULL, NULL);
     INSERT INTO null_t VALUES (NULL, NULL), (NULL, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT MIN(id) FROM t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT MAX(id) FROM t; SELECT @@warning_count, ROW_COUNT();
     SELECT MIN(n) FROM t; SELECT MAX(n) FROM t;
     SELECT MIN(nn) FROM t; SELECT MAX(nn) FROM t;
     SELECT MIN(u) FROM t; SELECT MAX(u) FROM t;
     SELECT MIN(b) FROM t; SELECT MAX(b) FROM t;
     SELECT MIN(bu) FROM t; SELECT MAX(bu) FROM t;"
)
expect_value "min id" "1" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "min id status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "max id" "4" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "max id status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "min nullable int" "20" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "max nullable int" "30" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "min not null int" "10" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "max not null int" "40" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "min unsigned int" "1" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "max unsigned int" "4294967295" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "min bigint" "-1" "$(printf '%s\n' "$core" | sed -n '11p')"
expect_value "max bigint" "9223372036854775807" "$(printf '%s\n' "$core" | sed -n '12p')"
expect_value "min unsigned bigint supported range" "1" "$(printf '%s\n' "$core" | sed -n '13p')"
expect_value \
    "max unsigned bigint supported range" \
    "9223372036854775807" \
    "$(printf '%s\n' "$core" | sed -n '14p')"

nulls=$(run_mysql \
    "USE ${DATABASE};
     SELECT MIN(n), MAX(n) FROM empty_t;
     SELECT MIN(n), MAX(n), MIN(b), MAX(b) FROM null_t;
     SELECT MIN(n), MAX(n) FROM t WHERE id > 99;
     SELECT MIN(n), MAX(n) FROM t WHERE n IS NULL;
     SELECT MIN(n), MAX(n) FROM t WHERE n IS NOT NULL;"
)
expect_value "empty table min max" "NULL	NULL" "$(printf '%s\n' "$nulls" | sed -n '1p')"
expect_value "all null min max" "NULL	NULL	NULL	NULL" "$(printf '%s\n' "$nulls" | sed -n '2p')"
expect_value "no match min max" "NULL	NULL" "$(printf '%s\n' "$nulls" | sed -n '3p')"
expect_value "matched null-only min max" "NULL	NULL" "$(printf '%s\n' "$nulls" | sed -n '4p')"
expect_value "matched non-null min max" "20	30" "$(printf '%s\n' "$nulls" | sed -n '5p')"

where_values=$(run_mysql \
    "USE ${DATABASE};
     SELECT MIN(n) FROM t WHERE id = 2;
     SELECT MAX(n) FROM t WHERE id <> 1;
     SELECT MIN(n) FROM t WHERE id != 1;
     SELECT MAX(n) FROM t WHERE id < 4;
     SELECT MIN(n) FROM t WHERE id <= 3;
     SELECT MAX(n) FROM t WHERE id > 2;
     SELECT MIN(n) FROM t WHERE id >= 2;
     SELECT MAX(n) FROM t WHERE id <=> 2;
     SELECT MIN(n) FROM t WHERE n = 20;
     SELECT MAX(n) FROM t WHERE n <> 20;
     SELECT MIN(n) FROM t WHERE n <=> 20;
     SELECT MAX(u) FROM t WHERE u = 4294967295;
     SELECT MIN(b) FROM t WHERE b = -1;
     SELECT MAX(bu) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "where equal min" "20" "$(printf '%s\n' "$where_values" | sed -n '1p')"
expect_value "where not equal max" "30" "$(printf '%s\n' "$where_values" | sed -n '2p')"
expect_value "where bang not equal min" "20" "$(printf '%s\n' "$where_values" | sed -n '3p')"
expect_value "where less max" "20" "$(printf '%s\n' "$where_values" | sed -n '4p')"
expect_value "where less equal min" "20" "$(printf '%s\n' "$where_values" | sed -n '5p')"
expect_value "where greater max" "30" "$(printf '%s\n' "$where_values" | sed -n '6p')"
expect_value "where greater equal min" "20" "$(printf '%s\n' "$where_values" | sed -n '7p')"
expect_value "where null safe max" "20" "$(printf '%s\n' "$where_values" | sed -n '8p')"
expect_value "nullable equal min" "20" "$(printf '%s\n' "$where_values" | sed -n '9p')"
expect_value "nullable not equal max" "30" "$(printf '%s\n' "$where_values" | sed -n '10p')"
expect_value "nullable null-safe min" "20" "$(printf '%s\n' "$where_values" | sed -n '11p')"
expect_value "unsigned boundary max" "4294967295" "$(printf '%s\n' "$where_values" | sed -n '12p')"
expect_value "bigint min" "-1" "$(printf '%s\n' "$where_values" | sed -n '13p')"
expect_value \
    "unsigned bigint supported boundary max" \
    "9223372036854775807" \
    "$(printf '%s\n' "$where_values" | sed -n '14p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT MIN(id), max(id), Min( n ), MAX(/*x*/n), (MIN(id)), (MAX(id)) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "min max labels" \
    "MIN(id)	max(id)	Min( n )	MAX(/*x*/ n)	(MIN(id))	(MAX(id))" \
    "$headers"
expect_value "min max label values" "1	4	20	30	1	4" "$values"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT MIN(1), MAX(1), MIN(NULL), MAX(NULL);
     SELECT MIN(DISTINCT n), MAX(DISTINCT n) FROM t;
     SELECT MIN(n) FROM t ORDER BY id;
     SELECT MIN(n) FROM t LIMIT 1;"
)
expect_value "deferred literal aggregate arguments" "1	1	NULL	NULL" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred distinct aggregate arguments" "20	30" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred order by aggregate" "20" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"
expect_value "deferred limit one aggregate" "20" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '4p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT MIN(n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

grouped=$(run_mysql "USE ${DATABASE}; SELECT MIN(n) FROM t GROUP BY n;")
expect_value "deferred group null row" "NULL" "$(printf '%s\n' "$grouped" | sed -n '1p')"
expect_value "deferred group 20 row" "20" "$(printf '%s\n' "$grouped" | sed -n '2p')"
expect_value "deferred group 30 row" "30" "$(printf '%s\n' "$grouped" | sed -n '3p')"

expect_error \
    "min column from dual is unknown" \
    1054 \
    42S22 \
    "Unknown column 'id'" \
    "USE ${DATABASE}; SELECT MIN(id) FROM DUAL;"

expect_error \
    "min empty argument is syntax error" \
    1064 \
    42000 \
    "near ')'" \
    "USE ${DATABASE}; SELECT MIN();"

expect_error \
    "min multiple arguments is syntax error" \
    1064 \
    42000 \
    "near ', n) FROM t'" \
    "USE ${DATABASE}; SELECT MIN(id, n) FROM t;"

expect_error \
    "min star argument is syntax error" \
    1064 \
    42000 \
    "near '*) FROM t'" \
    "USE ${DATABASE}; SELECT MIN(*) FROM t;"

expect_error \
    "min whitespace before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.MIN does not exist" \
    "USE ${DATABASE}; SELECT MIN (n) FROM t;"

expect_error \
    "min comment before paren resolves as stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.MIN does not exist" \
    "USE ${DATABASE}; SELECT MIN/**/(n) FROM t;"
