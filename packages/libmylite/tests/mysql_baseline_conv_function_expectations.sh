#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_conv_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_conv_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_force() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names --force "$@" 2>/dev/null
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_force() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_force "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE t(id INT, txt VARCHAR(16), bits BIT(4));
           INSERT INTO t VALUES (10, '10', b'1111'), (-1, '-1', b'0010'), (NULL, NULL, NULL);" \
    >/dev/null

expect_output_with_headers \
    "core conv values" \
    "CONV(NULL,10,2)	CONV(10,NULL,2)	CONV(10,10,NULL)	CONV(TRUE,10,2)	CONV(FALSE,10,2)	CONV(0,10,2)	CONV(10,10,2)	CONV(35,10,36)	CONV(36,10,36)
NULL	NULL	NULL	1	0	0	1010	Z	10" \
    "SELECT CONV(NULL,10,2),CONV(10,NULL,2),CONV(10,10,NULL),
            CONV(TRUE,10,2),CONV(FALSE,10,2),CONV(0,10,2),
            CONV(10,10,2),CONV(35,10,36),CONV(36,10,36);" \
    "$DATABASE"

expect_output_with_headers \
    "input base parsing" \
    "CONV(1010,2,10)	CONV(36,36,10)	CONV(12,2,10)	CONV(12,3,10)	CONV(-17,18,10)	CONV(-17,18,-10)	CONV(-17,10,18)	CONV(-17,10,-18)
10	114	1	5	18446744073709551591	-25	2D3FGB0B9CG4BD1H	-H" \
    "SELECT CONV(1010,2,10),CONV(36,36,10),CONV(12,2,10),
            CONV(12,3,10),CONV(-17,18,10),CONV(-17,18,-10),
            CONV(-17,10,18),CONV(-17,10,-18);" \
    "$DATABASE"

expect_output_with_headers \
    "signed unsigned boundaries" \
    "CONV(-1,10,10)	CONV(-1,10,-10)	CONV(-2,10,10)	CONV(-2,10,-10)	CONV(-9223372036854775808,10,10)	CONV(-9223372036854775808,10,-10)	CONV(9223372036854775807,10,10)	CONV(9223372036854775808,10,10)	CONV(18446744073709551615,10,10)	CONV(18446744073709551615,10,16)
18446744073709551615	-1	18446744073709551614	-2	9223372036854775808	-9223372036854775808	9223372036854775807	9223372036854775808	18446744073709551615	FFFFFFFFFFFFFFFF" \
    "SELECT CONV(-1,10,10),CONV(-1,10,-10),CONV(-2,10,10),
            CONV(-2,10,-10),CONV(-9223372036854775808,10,10),
            CONV(-9223372036854775808,10,-10),
            CONV(9223372036854775807,10,10),
            CONV(9223372036854775808,10,10),
            CONV(18446744073709551615,10,10),
            CONV(18446744073709551615,10,16);" \
    "$DATABASE"

expect_output \
    "invalid bases return null without warnings" \
    "NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	0	0
0	-1" \
    "DO 0;
     SELECT CONV(10,1,10),CONV(10,0,10),CONV(10,-1,10),
            CONV(10,37,10),CONV(10,-37,10),CONV(10,10,1),
            CONV(10,10,0),CONV(10,10,-1),CONV(10,10,37),
            CONV(10,10,-37),@@warning_count,ROW_COUNT();
     SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "child warning staging" \
    "NULL	NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
3	-1" \
    "DO 0;
     SELECT CONV(5 DIV 0,10,2),CONV(12,5 DIV 0,2),
            CONV(12,10,5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "null short circuit warning staging" \
    "NULL	NULL	NULL	0
Warning	1365	Division by 0
1	-1" \
    "DO 0;
     SELECT CONV(NULL,5 DIV 0,2),CONV(10,NULL,5 DIV 0),
            CONV(10,5 DIV 0,NULL),@@warning_count;
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "invalid leading digit warning staging" \
    "0	0	1	1	-1	0	0
Warning	1292	Truncated incorrect DECIMAL value: '2'
Warning	1292	Truncated incorrect DECIMAL value: '9'
2	-1" \
    "DO 0;
     SELECT CONV(2,2,10),CONV(9,8,10),CONV(19,8,10),
            CONV(123,2,10),CONV(-123,2,-10),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do warning staging" \
    "Warning	1292	Truncated incorrect DECIMAL value: '2'
Warning	1365	Division by 0
2	-1" \
    "DO CONV(2,2,10),CONV(5 DIV 0,10,2);
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "successful do status" \
    "0	0" \
    "DO CONV(10,10,2),CONV(NULL,10,2); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty conv arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONV'" \
    "SELECT CONV();" \
    "$DATABASE"

expect_error \
    "one argument conv arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONV'" \
    "SELECT CONV(1);" \
    "$DATABASE"

expect_error \
    "two argument conv arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONV'" \
    "SELECT CONV(1,10);" \
    "$DATABASE"

expect_error \
    "four argument conv arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONV'" \
    "SELECT CONV(1,10,2,3);" \
    "$DATABASE"

expect_error \
    "value child overflow under conv" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT CONV(3037000500*3037000500,10,2);" \
    "$DATABASE"

expect_error \
    "from base child overflow under conv" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT CONV(12,3037000500*3037000500,2);" \
    "$DATABASE"

expect_error \
    "to base child overflow under conv" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT CONV(12,10,3037000500*3037000500);" \
    "$DATABASE"

expect_output_force \
    "select warning before later error" \
    "Warning	1292	Truncated incorrect DECIMAL value: '2'
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     SELECT CONV(2,2,10),CONV(3037000500*3037000500,10,2);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "do warning before later error" \
    "Warning	1292	Truncated incorrect DECIMAL value: '2'
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     DO CONV(2,2,10),CONV(3037000500*3037000500,10,2);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "row-backed conv values" \
    "id	CONV(id,10,2)	CONV(id,10,16)	CONV(id,10,-10)	CASE WHEN id=10 THEN CONV(id,10,16) END	CONCAT('x',CONV(id+1,10,36))
-1	1111111111111111111111111111111111111111111111111111111111111111	FFFFFFFFFFFFFFFF	-1	NULL	x0
10	1010	A	10	A	xB
NULL	NULL	NULL	NULL	NULL	NULL" \
    "SELECT id,CONV(id,10,2),CONV(id,10,16),CONV(id,10,-10),
            CASE WHEN id=10 THEN CONV(id,10,16) END,
            CONCAT('x',CONV(id+1,10,36))
       FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"

expect_output_with_headers \
    "row-backed conv predicates" \
    "id
10" \
    "SELECT id FROM t
      WHERE CONV(id,10,16)='A' OR CONV(id,10,16)='23'
      ORDER BY id;" \
    "$DATABASE"

expect_output_with_headers \
    "row-backed conv order key" \
    "id	CONV(id,10,2)
NULL	NULL
10	1010
-1	1111111111111111111111111111111111111111111111111111111111111111" \
    "SELECT id,CONV(id,10,2) FROM t
      ORDER BY CONV(id,10,2),id;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT CONV('6E',18,8),CONV('a',16,2),CONV('zz',36,10),
            CONV(12.75,10,2),CONV(-12.75,10,-2),CONV(1e1,10,2),
            CONV(X'40',10,2),CONV(0x40,10,2),CONV(b'1111',10,2);
     SELECT id,txt,CONV(txt,10,2),CONV(bits,10,2)
       FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "CONV('6E',18,8)	CONV('a',16,2)	CONV('zz',36,10)	CONV(12.75,10,2)	CONV(-12.75,10,-2)	CONV(1e1,10,2)	CONV(X'40',10,2)	CONV(0x40,10,2)	CONV(b'1111',10,2)
172	1010	1295	1100	-1100	1010	1000000	1000000	1111
id	txt	CONV(txt,10,2)	CONV(bits,10,2)
-1	-1	1111111111111111111111111111111111111111111111111111111111111111	10
10	10	1010	1111
NULL	NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_conv_function_expectations: ok"
