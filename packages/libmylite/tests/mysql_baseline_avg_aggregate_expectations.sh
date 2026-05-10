#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_avg_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_avg_aggregate_expectations: $1" >&2
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
     CREATE TABLE rounding_t(n INT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE fifth_decimal_t(pos BIGINT NOT NULL, neg BIGINT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE negative_zero_t(n BIGINT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE digits(n INT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE quoted_t(\`weird name\` INT NULL, \`double\"quote\` INT NULL) ENGINE=InnoDB;
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
     INSERT INTO rounding_t VALUES (1), (2), (2), (-1), (0), (0);
     INSERT INTO digits VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
     INSERT INTO fifth_decimal_t
       SELECT CASE WHEN a.n = 0 AND b.n = 0 AND c.n = 0 AND d.n = 0 AND e.n = 0
                   THEN 1 ELSE 0 END,
              CASE WHEN a.n = 0 AND b.n = 0 AND c.n = 0 AND d.n = 0 AND e.n = 0
                   THEN -1 ELSE 0 END
       FROM digits a CROSS JOIN digits b CROSS JOIN digits c CROSS JOIN digits d
       CROSS JOIN digits e
       ORDER BY a.n, b.n, c.n, d.n, e.n
       LIMIT 20000;
     INSERT INTO negative_zero_t
       SELECT CASE WHEN a.n = 0 AND b.n = 0 AND c.n = 0 AND d.n = 0 AND e.n = 0
                   THEN -1 ELSE 0 END
       FROM digits a CROSS JOIN digits b CROSS JOIN digits c CROSS JOIN digits d
       CROSS JOIN digits e
       LIMIT 30000;
     INSERT INTO quoted_t VALUES (1, 1), (NULL, 2), (3, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT AVG(id), AVG(i), AVG(iu), AVG(b), AVG(n), AVG(nn) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT AVG(ti), AVG(ti1), AVG(si), AVG(mi), AVG(bool_col), AVG(boolean_col) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     SELECT AVG(n), AVG(nn) FROM empty_t;
     SELECT AVG(n), AVG(b) FROM all_null_t;"
)
expect_value \
    "integer family averages" \
    "2.5000	1.0000	1431655765.6667	-0.6667	23.3333	25.0000" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer family avg status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value \
    "small integer alias averages" \
    "-0.6667	0.6667	-0.3333	-0.3333	0.3333	0.6667" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "small integer avg status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "empty table averages" "NULL	NULL" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "all null table averages" "NULL	NULL" "$(printf '%s\n' "$core" | sed -n '6p')"

where_avgs=$(run_mysql \
    "USE ${DATABASE};
     SELECT AVG(n) FROM t WHERE id > 99;
     SELECT AVG(n) FROM t WHERE n IS NULL;
     SELECT AVG(n) FROM t WHERE n IS NOT NULL;
     SELECT AVG(n) FROM t WHERE id = 2;
     SELECT AVG(n) FROM t WHERE id <> 1;
     SELECT AVG(n) FROM t WHERE id != 1;
     SELECT AVG(n) FROM t WHERE id < 3;
     SELECT AVG(n) FROM t WHERE id <= 3;
     SELECT AVG(n) FROM t WHERE id > 2;
     SELECT AVG(n) FROM t WHERE id >= 2;
     SELECT AVG(n) FROM t WHERE id <=> 2;
     SELECT AVG(n) FROM t WHERE n = 20;
     SELECT AVG(n) FROM t WHERE n <> 20;
     SELECT AVG(n) FROM t WHERE n <=> 20;
     SELECT AVG(iu) FROM t WHERE iu = 4294967295;
     SELECT AVG(b) FROM t WHERE b = -9223372036854775808;
     SELECT AVG(bu) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "no match average" "NULL" "$(printf '%s\n' "$where_avgs" | sed -n '1p')"
expect_value "matched null-only average" "NULL" "$(printf '%s\n' "$where_avgs" | sed -n '2p')"
expect_value "matched non-null average" "23.3333" "$(printf '%s\n' "$where_avgs" | sed -n '3p')"
expect_value "where equal average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '4p')"
expect_value "where not equal angle average" "23.3333" "$(printf '%s\n' "$where_avgs" | sed -n '5p')"
expect_value "where not equal bang average" "23.3333" "$(printf '%s\n' "$where_avgs" | sed -n '6p')"
expect_value "where less average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '7p')"
expect_value "where less equal average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '8p')"
expect_value "where greater average" "25.0000" "$(printf '%s\n' "$where_avgs" | sed -n '9p')"
expect_value "where greater equal average" "23.3333" "$(printf '%s\n' "$where_avgs" | sed -n '10p')"
expect_value "where null-safe average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '11p')"
expect_value "nullable equal average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '12p')"
expect_value "nullable not equal average" "30.0000" "$(printf '%s\n' "$where_avgs" | sed -n '13p')"
expect_value "nullable null-safe average" "20.0000" "$(printf '%s\n' "$where_avgs" | sed -n '14p')"
expect_value "unsigned int boundary average" "4294967295.0000" \
    "$(printf '%s\n' "$where_avgs" | sed -n '15p')"
expect_value "signed bigint minimum average" "-9223372036854775808.0000" \
    "$(printf '%s\n' "$where_avgs" | sed -n '16p')"
expect_value "unsigned bigint supported max average" "9223372036854775807.0000" \
    "$(printf '%s\n' "$where_avgs" | sed -n '17p')"

rounding=$(run_mysql \
    "USE ${DATABASE};
     SELECT AVG(n) FROM (SELECT 1 AS n UNION ALL SELECT 2 UNION ALL SELECT 2) AS r;
     SELECT AVG(n) FROM (SELECT 1 AS n UNION ALL SELECT 6) AS r;
     SELECT AVG(n) FROM (SELECT -1 AS n UNION ALL SELECT -2 UNION ALL SELECT 0) AS r;
     SELECT AVG(n) FROM (SELECT -1 AS n UNION ALL SELECT 0 UNION ALL SELECT 0) AS r;
     SELECT AVG(pos), AVG(neg) FROM fifth_decimal_t;
     SELECT AVG(n) FROM negative_zero_t;"
)
expect_value "round half away positive" "1.6667" "$(printf '%s\n' "$rounding" | sed -n '1p')"
expect_value "exact half average" "3.5000" "$(printf '%s\n' "$rounding" | sed -n '2p')"
expect_value "negative integer average" "-1.0000" "$(printf '%s\n' "$rounding" | sed -n '3p')"
expect_value "round half away negative" "-0.3333" "$(printf '%s\n' "$rounding" | sed -n '4p')"
expect_value "fifth decimal tie averages" "0.0001	-0.0001" \
    "$(printf '%s\n' "$rounding" | sed -n '5p')"
expect_value "rounded negative zero average" "0.0000" "$(printf '%s\n' "$rounding" | sed -n '6p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT AVG(n), avg(n), Avg( n ), AVG(/*x*/n), (AVG(n)), AVG(N), AVG(t.n),
            AVG(${DATABASE}.t.n)
       FROM ${DATABASE}.t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "avg labels" \
    "AVG(n)	avg(n)	Avg( n )	AVG(/*x*/ n)	(AVG(n))	AVG(N)	AVG(t.n)	AVG(${DATABASE}.t.n)" \
    "$headers"
expect_value \
    "avg label values" \
    "23.3333	23.3333	23.3333	23.3333	23.3333	23.3333	23.3333	23.3333" \
    "$values"

whitespace_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT AVG (n), AVG/**/(n), AVG /*x*/ (n), AVG( /*x*/ n ) FROM t;"
)
whitespace_headers=$(printf '%s\n' "$whitespace_output" | sed -n '1p')
whitespace_values=$(printf '%s\n' "$whitespace_output" | sed -n '2p')
expect_value \
    "avg whitespace labels" \
    "AVG (n)	AVG/**/ (n)	AVG /*x*/ (n)	AVG( /*x*/ n )" \
    "$whitespace_headers"
expect_value \
    "avg whitespace values" \
    "23.3333	23.3333	23.3333	23.3333" \
    "$whitespace_values"

quoted_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT AVG(\`weird name\`), AVG(\`double\"quote\`) FROM quoted_t;"
)
quoted_headers=$(printf '%s\n' "$quoted_output" | sed -n '1p')
quoted_values=$(printf '%s\n' "$quoted_output" | sed -n '2p')
expect_value \
    "quoted avg column labels" \
    'AVG(`weird name`)	AVG(`double"quote`)' \
    "$quoted_headers"
expect_value "quoted avg column values" "2.0000	1.5000" "$quoted_values"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT AVG(1), AVG(NULL), AVG(DISTINCT n) FROM t;
     SELECT AVG(b), AVG(bu) FROM overflow_t;
     SELECT AVG(n) FROM t ORDER BY id;
     SELECT AVG(n) FROM t LIMIT 1;"
)
expect_value \
    "deferred literal distinct forms" \
    "1.0000	NULL	25.0000" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value \
    "deferred exact decimal overflow forms" \
    "4611686018427387904.0000	9223372036854775807.0000" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred order by aggregate" "23.3333" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"
expect_value "deferred limit one returns row" "23.3333" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '4p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT AVG(n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "avg column without source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT AVG(n);"

expect_error \
    "avg column from dual unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT AVG(n) FROM DUAL;"

expect_error \
    "avg column missing column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT AVG(missing) FROM t;"

expect_error \
    "avg no argument syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT AVG();"

expect_error \
    "avg star syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT AVG(*) FROM t;"

expect_error \
    "avg multiple arguments syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT AVG(n, n) FROM t;"
