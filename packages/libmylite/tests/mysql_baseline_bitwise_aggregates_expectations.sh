#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bitwise_aggregates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bitwise_aggregates_expectations: $1" >&2
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
     CREATE TABLE quoted_t(\`weird name\` INT NULL, \`double\"quote\` INT NULL) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, -2147483648, 0, -9223372036854775808, 0, NULL, 1,
        -128, 0, -32768, -8388608, FALSE, TRUE),
       (2, -2, 2, -2, 2, 20, 2, -1, 1, 0, 0, TRUE, FALSE),
       (3, 4, 4294967295, 4, 9223372036854775807,
        30, 4, 127, NULL, 32767, 8388607, NULL, NULL),
       (4, NULL, NULL, NULL, NULL, 20, 8, NULL, 1, NULL, NULL, FALSE, TRUE);
     INSERT INTO all_null_t VALUES (1, NULL, NULL), (2, NULL, NULL);
     INSERT INTO quoted_t VALUES (1, 1), (NULL, 2), (3, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT BIT_AND(i), BIT_OR(i), BIT_XOR(i),
                  BIT_AND(iu), BIT_OR(iu), BIT_XOR(iu) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT BIT_AND(b), BIT_OR(b), BIT_XOR(b),
                  BIT_AND(bu), BIT_OR(bu), BIT_XOR(bu) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT BIT_AND(ti), BIT_OR(ti), BIT_XOR(ti),
                  BIT_AND(ti1), BIT_OR(ti1), BIT_XOR(ti1) FROM t;
     SELECT @@warning_count, ROW_COUNT();
     SELECT BIT_AND(si), BIT_OR(si), BIT_XOR(si),
            BIT_AND(mi), BIT_OR(mi), BIT_XOR(mi) FROM t;
     SELECT BIT_AND(bool_col), BIT_OR(bool_col), BIT_XOR(bool_col),
            BIT_AND(boolean_col), BIT_OR(boolean_col), BIT_XOR(boolean_col) FROM t;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM empty_t;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n), BIT_AND(b), BIT_OR(b), BIT_XOR(b)
       FROM all_null_t;"
)
expect_value \
    "integer bitwise aggregates" \
    "0	18446744073709551614	2147483642	0	4294967295	4294967293" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer bitwise status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value \
    "bigint bitwise aggregates" \
    "0	18446744073709551614	9223372036854775802	0	9223372036854775807	9223372036854775805" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "bigint bitwise status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value \
    "tinyint bitwise aggregates" \
    "0	18446744073709551615	0	0	1	0" \
    "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "tinyint bitwise status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value \
    "small medium bitwise aggregates" \
    "0	18446744073709551615	18446744073709551615	0	18446744073709551615	18446744073709551615" \
    "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value \
    "boolean bitwise aggregates" \
    "0	1	1	0	1	0" \
    "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value \
    "empty table neutral bitwise aggregates" \
    "18446744073709551615	0	0" \
    "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value \
    "all null neutral bitwise aggregates" \
    "18446744073709551615	0	0	18446744073709551615	0	0" \
    "$(printf '%s\n' "$core" | sed -n '10p')"

where_bits=$(run_mysql \
    "USE ${DATABASE};
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id > 99;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE n IS NULL;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE n IS NOT NULL;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id = 2;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id <> 1;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id != 1;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id < 3;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id <= 3;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id > 2;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id >= 2;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE id <=> 2;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE n = 20;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE n <> 20;
     SELECT BIT_AND(n), BIT_OR(n), BIT_XOR(n) FROM t WHERE n <=> 20;
     SELECT BIT_AND(iu), BIT_OR(iu), BIT_XOR(iu) FROM t WHERE iu = 4294967295;
     SELECT BIT_AND(b), BIT_OR(b), BIT_XOR(b) FROM t WHERE b = -9223372036854775808;
     SELECT BIT_AND(bu), BIT_OR(bu), BIT_XOR(bu) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "no match neutral bits" "18446744073709551615	0	0" \
    "$(printf '%s\n' "$where_bits" | sed -n '1p')"
expect_value "matched null-only neutral bits" "18446744073709551615	0	0" \
    "$(printf '%s\n' "$where_bits" | sed -n '2p')"
expect_value "matched non-null bits" "20	30	30" "$(printf '%s\n' "$where_bits" | sed -n '3p')"
expect_value "where equal bits" "20	20	20" "$(printf '%s\n' "$where_bits" | sed -n '4p')"
expect_value "where not equal angle bits" "20	30	30" "$(printf '%s\n' "$where_bits" | sed -n '5p')"
expect_value "where not equal bang bits" "20	30	30" "$(printf '%s\n' "$where_bits" | sed -n '6p')"
expect_value "where less bits" "20	20	20" "$(printf '%s\n' "$where_bits" | sed -n '7p')"
expect_value "where less equal bits" "20	30	10" "$(printf '%s\n' "$where_bits" | sed -n '8p')"
expect_value "where greater bits" "20	30	10" "$(printf '%s\n' "$where_bits" | sed -n '9p')"
expect_value "where greater equal bits" "20	30	30" "$(printf '%s\n' "$where_bits" | sed -n '10p')"
expect_value "where null-safe bits" "20	20	20" "$(printf '%s\n' "$where_bits" | sed -n '11p')"
expect_value "nullable equal bits" "20	20	0" "$(printf '%s\n' "$where_bits" | sed -n '12p')"
expect_value "nullable not equal bits" "30	30	30" "$(printf '%s\n' "$where_bits" | sed -n '13p')"
expect_value "nullable null-safe bits" "20	20	0" "$(printf '%s\n' "$where_bits" | sed -n '14p')"
expect_value "unsigned int boundary bits" "4294967295	4294967295	4294967295" \
    "$(printf '%s\n' "$where_bits" | sed -n '15p')"
expect_value \
    "signed bigint minimum bits" \
    "9223372036854775808	9223372036854775808	9223372036854775808" \
    "$(printf '%s\n' "$where_bits" | sed -n '16p')"
expect_value \
    "unsigned bigint supported max bits" \
    "9223372036854775807	9223372036854775807	9223372036854775807" \
    "$(printf '%s\n' "$where_bits" | sed -n '17p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT BIT_AND(n), bit_or(n), Bit_Xor( n ), BIT_AND(/*x*/n), (BIT_OR(n)),
            BIT_XOR(N), BIT_AND(t.n), BIT_AND(${DATABASE}.t.n)
       FROM ${DATABASE}.t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "bitwise aggregate labels" \
    "BIT_AND(n)	bit_or(n)	Bit_Xor( n )	BIT_AND(/*x*/ n)	(BIT_OR(n))	BIT_XOR(N)	BIT_AND(t.n)	BIT_AND(${DATABASE}.t.n)" \
    "$headers"
expect_value \
    "bitwise aggregate label values" \
    "20	30	30	20	30	30	20	20" \
    "$values"

quoted_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT BIT_AND(\`weird name\`), BIT_OR(\`double\"quote\`), BIT_XOR(\`weird name\`)
       FROM quoted_t;"
)
quoted_headers=$(printf '%s\n' "$quoted_output" | sed -n '1p')
quoted_values=$(printf '%s\n' "$quoted_output" | sed -n '2p')
expect_value \
    "quoted bitwise column labels" \
    'BIT_AND(`weird name`)	BIT_OR(`double"quote`)	BIT_XOR(`weird name`)' \
    "$quoted_headers"
expect_value "quoted bitwise column values" "1	3	2" "$quoted_values"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT BIT_AND(1), BIT_OR(NULL), BIT_XOR(1) FROM t;
     SELECT BIT_AND(i + 1), BIT_OR(i + 1), BIT_XOR(i + 1) FROM t;
     SELECT BIT_AND(n) FROM t ORDER BY id;
     SELECT BIT_AND(n) FROM t LIMIT 1;"
)
expect_value "deferred literal null forms" "1	0	0" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "deferred expression forms" "1	18446744073709551615	2147483643" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "deferred order by aggregate" "20" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"
expect_value "deferred limit one returns row" "20" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '4p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT BIT_AND(n) FROM t LIMIT 0; SELECT 'after';")
expect_value "deferred limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "deferred limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "deferred limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

expect_error \
    "bitwise column without source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT BIT_AND(n);"

expect_error \
    "bitwise column from dual unknown column" \
    1054 \
    42S22 \
    "Unknown column 'n' in 'field list'" \
    "USE ${DATABASE}; SELECT BIT_AND(n) FROM DUAL;"

expect_error \
    "bitwise column missing column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT BIT_AND(missing) FROM t;"

expect_error \
    "bitwise no argument syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT BIT_AND() FROM t;"

expect_error \
    "bitwise star syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT BIT_AND(*) FROM t;"

expect_error \
    "bitwise multiple arguments syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT BIT_AND(n, n) FROM t;"

expect_error \
    "bitwise distinct syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT BIT_XOR(DISTINCT n) FROM t;"

expect_error \
    "bitwise whitespace before paren stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.BIT_AND does not exist" \
    "USE ${DATABASE}; SELECT BIT_AND (n) FROM t;"

expect_error \
    "bitwise block comment before paren stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.BIT_AND does not exist" \
    "USE ${DATABASE}; SELECT BIT_AND/**/(n) FROM t;"
