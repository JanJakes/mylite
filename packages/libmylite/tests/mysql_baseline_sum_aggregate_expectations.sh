#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_sum_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sum_aggregate_expectations: $1" >&2
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
     ) ENGINE=InnoDB;
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL, nn INT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE all_null_t(id INT NOT NULL, n INT NULL, b BIGINT NULL) ENGINE=InnoDB;
     CREATE TABLE overflow_t(b BIGINT NOT NULL, bu BIGINT UNSIGNED NOT NULL) ENGINE=InnoDB;
     CREATE TABLE quoted_t(\`weird name\` INT NULL, \`double\"quote\` INT NULL) ENGINE=InnoDB;
     CREATE TABLE options(option_value TEXT NULL, autoload VARCHAR(20) NULL) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, -2147483648, 0, -9223372036854775808, 0, NULL, 10,
        -128, 0, -32768, -8388608, FALSE, TRUE),
       (2, 0, 2, -1, 2, 20, 20, -1, 1, 0, 0, TRUE, FALSE),
       (3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807,
        20, 30, 127, NULL, 32767, 8388607, NULL, NULL),
       (4, 5, NULL, NULL, NULL, 30, 40, NULL, 1, NULL, NULL, FALSE, TRUE);
     INSERT INTO all_null_t VALUES (1, NULL, NULL), (2, NULL, NULL);
     INSERT INTO overflow_t VALUES
       (9223372036854775807, 9223372036854775807),
       (1, 9223372036854775807);
     INSERT INTO quoted_t VALUES (1, 1), (NULL, 2), (3, NULL);
     INSERT INTO options VALUES
       ('abc','yes'),('de','on'),('ignored','no'),('', 'auto'),(NULL,'auto-on');" \
    >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT SUM(id), SUM(i), SUM(iu), SUM(b), SUM(n), SUM(nn) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT SUM(ti), SUM(ti1), SUM(si), SUM(mi), SUM(bool_col), SUM(boolean_col) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     SELECT SUM(n), SUM(nn) FROM empty_t;
     SELECT SUM(n), SUM(b) FROM all_null_t;"
)
expect_value \
    "integer family sums" \
    "10	4	4294967297	-2	70	100" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer family sum status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value \
    "small integer alias sums" \
    "-2	2	-1	-1	1	2" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "small integer sum status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "empty table sums" "NULL	NULL" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "all null table sums" "NULL	NULL" "$(printf '%s\n' "$core" | sed -n '6p')"

where_sums=$(run_mysql \
    "USE ${DATABASE};
     SELECT SUM(n) FROM t WHERE id > 99;
     SELECT SUM(n) FROM t WHERE n IS NULL;
     SELECT SUM(n) FROM t WHERE n IS NOT NULL;
     SELECT SUM(n) FROM t WHERE id = 2;
     SELECT SUM(n) FROM t WHERE id <> 1;
     SELECT SUM(n) FROM t WHERE id != 1;
     SELECT SUM(n) FROM t WHERE id < 3;
     SELECT SUM(n) FROM t WHERE id <= 3;
     SELECT SUM(n) FROM t WHERE id > 2;
     SELECT SUM(n) FROM t WHERE id >= 2;
     SELECT SUM(n) FROM t WHERE id <=> 2;
     SELECT SUM(n) FROM t WHERE n = 20;
     SELECT SUM(n) FROM t WHERE n <> 20;
     SELECT SUM(n) FROM t WHERE n <=> 20;
     SELECT SUM(iu) FROM t WHERE iu = 4294967295;
     SELECT SUM(b) FROM t WHERE b = -9223372036854775808;
     SELECT SUM(bu) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "no match sum" "NULL" "$(printf '%s\n' "$where_sums" | sed -n '1p')"
expect_value "matched null-only sum" "NULL" "$(printf '%s\n' "$where_sums" | sed -n '2p')"
expect_value "matched non-null sum" "70" "$(printf '%s\n' "$where_sums" | sed -n '3p')"
expect_value "where equal sum" "20" "$(printf '%s\n' "$where_sums" | sed -n '4p')"
expect_value "where not equal angle sum" "70" "$(printf '%s\n' "$where_sums" | sed -n '5p')"
expect_value "where not equal bang sum" "70" "$(printf '%s\n' "$where_sums" | sed -n '6p')"
expect_value "where less sum" "20" "$(printf '%s\n' "$where_sums" | sed -n '7p')"
expect_value "where less equal sum" "40" "$(printf '%s\n' "$where_sums" | sed -n '8p')"
expect_value "where greater sum" "50" "$(printf '%s\n' "$where_sums" | sed -n '9p')"
expect_value "where greater equal sum" "70" "$(printf '%s\n' "$where_sums" | sed -n '10p')"
expect_value "where null-safe sum" "20" "$(printf '%s\n' "$where_sums" | sed -n '11p')"
expect_value "nullable equal sum" "40" "$(printf '%s\n' "$where_sums" | sed -n '12p')"
expect_value "nullable not equal sum" "30" "$(printf '%s\n' "$where_sums" | sed -n '13p')"
expect_value "nullable null-safe sum" "40" "$(printf '%s\n' "$where_sums" | sed -n '14p')"
expect_value "unsigned int boundary sum" "4294967295" "$(printf '%s\n' "$where_sums" | sed -n '15p')"
expect_value "signed bigint minimum sum" "-9223372036854775808" "$(printf '%s\n' "$where_sums" | sed -n '16p')"
expect_value \
    "unsigned bigint supported max sum" \
    "9223372036854775807" \
    "$(printf '%s\n' "$where_sums" | sed -n '17p')"

length_sum=$(run_mysql \
    "USE ${DATABASE};
     SELECT SUM(LENGTH(option_value)) FROM options
     WHERE autoload IN ('yes','on','auto-on','auto');"
)
expect_value "string length expression sum" "5" "$length_sum"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT SUM(n), sum(n), Sum( n ), SUM(/*x*/n), (SUM(n)), SUM(N), SUM(t.n) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "sum labels" \
    "SUM(n)	sum(n)	Sum( n )	SUM(/*x*/ n)	(SUM(n))	SUM(N)	SUM(t.n)" \
    "$headers"
expect_value "sum label values" "70	70	70	70	70	70	70" "$values"

quoted_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT SUM(\`weird name\`), SUM(\`double\"quote\`) FROM quoted_t;"
)
quoted_headers=$(printf '%s\n' "$quoted_output" | sed -n '1p')
quoted_values=$(printf '%s\n' "$quoted_output" | sed -n '2p')
expect_value \
    "quoted sum column labels" \
    'SUM(`weird name`)	SUM(`double"quote`)' \
    "$quoted_headers"
expect_value "quoted sum column values" "4	3" "$quoted_values"

distinct_output=$(run_mysql \
    "USE ${DATABASE};
     SELECT SUM(DISTINCT n), SUM(DISTINCT n + 1) FROM t;"
)
expect_value "distinct sum forms" "50	52" "$distinct_output"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT SUM(1), SUM(NULL) FROM t;
     SELECT SUM(b), SUM(bu) FROM overflow_t;
     SELECT SUM(n) FROM t ORDER BY id;
     SELECT SUM(n) FROM t LIMIT 1;"
)
expect_value \
    "deferred literal forms" \
    "4	NULL" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value \
    "deferred exact decimal overflow forms" \
    "9223372036854775808	18446744073709551614" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred order by aggregate" "70" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"
expect_value "deferred limit one returns row" "70" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '4p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT SUM(n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "sum column without source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT SUM(n);"

expect_error \
    "sum column from dual unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT SUM(n) FROM DUAL;"

expect_error \
    "sum column missing column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT SUM(missing) FROM t;"

expect_error \
    "sum no argument syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT SUM();"

expect_error \
    "sum star syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT SUM(*) FROM t;"

expect_error \
    "sum multiple arguments syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT SUM(n, n) FROM t;"

expect_error \
    "sum whitespace resolves stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.SUM does not exist" \
    "USE ${DATABASE}; SELECT SUM (n) FROM t;"

expect_error \
    "sum comment resolves stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.SUM does not exist" \
    "USE ${DATABASE}; SELECT SUM/**/(n) FROM t;"
