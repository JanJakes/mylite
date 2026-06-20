#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_distinct_column_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_distinct_column_aggregate_expectations: $1" >&2
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
       bool_col BOOL NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       body TEXT NULL,
       raw VARBINARY(4) NULL
     );
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL);
     CREATE TABLE all_null_t(id INT NOT NULL, n INT NULL);
     CREATE TABLE quoted_t(\`weird name\` INT NULL, \`double\"quote\` INT NULL);
     INSERT INTO t VALUES
       (1, -2147483648, 0, -9223372036854775808, 0, NULL, 10, TRUE, NULL, NULL, NULL, NULL),
       (2, 0, 2, -1, 2, 20, 20, FALSE, 'alice', 'A', 'essay', X'41'),
       (3, 2147483647, 4294967295, -1, 9223372036854775807, 20, 30, FALSE,
        'Alice', 'A   ', 'Essay', X'41'),
       (4, 5, NULL, 9223372036854775807, NULL, 30, 40, NULL, 'bob', 'B', 'note', X'42'),
       (5, NULL, 0, NULL, 0, NULL, 50, TRUE, 'BOB', 'B    ', 'Note', X'42');
     INSERT INTO all_null_t VALUES (1, NULL), (2, NULL);
     INSERT INTO quoted_t VALUES (1, 1), (1, 2), (NULL, 2), (3, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT COUNT(DISTINCT i), COUNT(DISTINCT iu), COUNT(DISTINCT b), COUNT(DISTINCT bu), COUNT(DISTINCT n), COUNT(DISTINCT nn), COUNT(DISTINCT bool_col) FROM t; SELECT @@warning_count, ROW_COUNT();
     SELECT COUNT(DISTINCT name), COUNT(DISTINCT label), COUNT(DISTINCT body), COUNT(DISTINCT raw) FROM t;
     DO 0; SELECT COUNT(DISTINCT n) FROM empty_t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(DISTINCT n) FROM all_null_t; SELECT @@warning_count, ROW_COUNT();"
)
expect_value "integer family distinct counts" "4	3	3	3	2	5	2" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer family distinct status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "string and binary family distinct counts" "2	2	2	2" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "empty table distinct count" "0" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "empty table distinct status" "0	-1" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "all-null table distinct count" "0" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "all-null table distinct status" "0	-1" "$(printf '%s\n' "$core" | sed -n '7p')"

quoted_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT \`weird name\`), COUNT(DISTINCT \`double\"quote\`) FROM quoted_t;"
)
quoted_headers=$(printf '%s\n' "$quoted_output" | sed -n '1p')
quoted_values=$(printf '%s\n' "$quoted_output" | sed -n '2p')
expect_value \
    "quoted count distinct labels" \
    'COUNT(DISTINCT `weird name`)	COUNT(DISTINCT `double"quote`)' \
    "$quoted_headers"
expect_value "quoted count distinct values" "2	2" "$quoted_values"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT n), count(distinct n), Count( DISTINCT n ),
            COUNT(DISTINCT /*x*/n), COUNT(DISTINCT/*x*/n),
            COUNT(/*x*/DISTINCT n), (COUNT(DISTINCT n)), COUNT(DISTINCT N),
            COUNT(DISTINCT(t.n)), COUNT(DISTINCT (t.n)) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "count distinct labels" \
    "COUNT(DISTINCT n)	count(distinct n)	Count( DISTINCT n )	COUNT(DISTINCT /*x*/ n)	COUNT(DISTINCT/*x*/ n)	COUNT(/*x*/ DISTINCT n)	(COUNT(DISTINCT n))	COUNT(DISTINCT N)	COUNT(DISTINCT(t.n))	COUNT(DISTINCT (t.n))" \
    "$headers"
expect_value "count distinct label values" "2	2	2	2	2	2	2	2	2	2" "$values"

where_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT n) FROM t WHERE n IS NULL;
     SELECT COUNT(DISTINCT n) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(DISTINCT n) FROM t WHERE id = 2;
     SELECT COUNT(DISTINCT n) FROM t WHERE id > 99;
     SELECT COUNT(DISTINCT n) FROM t WHERE n = 20;
     SELECT COUNT(DISTINCT n) FROM t WHERE n <> 20;
     SELECT COUNT(DISTINCT n) FROM t WHERE iu = 4294967295;
     SELECT COUNT(DISTINCT n) FROM t WHERE b = -9223372036854775808;
     SELECT COUNT(DISTINCT n) FROM t WHERE bu = 9223372036854775807;
     SELECT COUNT(DISTINCT name) FROM t WHERE id >= 3;"
)
expect_value "distinct where nullable is null" "0" "$(printf '%s\n' "$where_counts" | sed -n '1p')"
expect_value "distinct where nullable is not null" "2" "$(printf '%s\n' "$where_counts" | sed -n '2p')"
expect_value "distinct where equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '3p')"
expect_value "distinct where no match" "0" "$(printf '%s\n' "$where_counts" | sed -n '4p')"
expect_value "distinct where nullable equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '5p')"
expect_value "distinct where nullable not equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '6p')"
expect_value "distinct where unsigned int boundary" "1" "$(printf '%s\n' "$where_counts" | sed -n '7p')"
expect_value "distinct where signed bigint minimum" "0" "$(printf '%s\n' "$where_counts" | sed -n '8p')"
expect_value "distinct where unsigned bigint supported max" "1" "$(printf '%s\n' "$where_counts" | sed -n '9p')"
expect_value "distinct string where integer predicate" "2" "$(printf '%s\n' "$where_counts" | sed -n '10p')"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(DISTINCT n, nn), COUNT(DISTINCT t.n), COUNT(DISTINCT 1), COUNT(DISTINCT TRUE), COUNT(DISTINCT n + 1) FROM t;
     SELECT COUNT(DISTINCT n) FROM t ORDER BY id;
     SELECT COUNT(DISTINCT n) FROM t LIMIT 1;"
)
expect_value "deferred count distinct expr forms" "3	2	1	1	2" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred order by aggregate" "2" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred limit one returns row" "2" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(DISTINCT n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "count distinct without source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT n);"

expect_error \
    "count distinct from dual unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT n) FROM DUAL;"

expect_error \
    "count distinct missing column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT missing) FROM t;"

expect_error \
    "count whitespace before distinct syntax error" \
    1064 \
    42000 \
    "near 'DISTINCT n) FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT (DISTINCT n) FROM t;"

expect_error \
    "count comment before distinct syntax error" \
    1064 \
    42000 \
    "near 'DISTINCT n) FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT/**/(DISTINCT n) FROM t;"

expect_error \
    "count distinct star syntax error" \
    1064 \
    42000 \
    "near '*) FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT *) FROM t;"
