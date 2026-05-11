#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_acos_asin_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_acos_asin_functions_expectations: $1" >&2
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
           CREATE TABLE t(id INT);
           INSERT INTO t VALUES (0), (1), (2), (NULL), (-1);" \
    >/dev/null

expect_output_with_headers \
    "core acos values" \
    "ACOS(NULL)	ACOS(TRUE)	ACOS(FALSE)	ACOS(1)	ACOS(0)	ACOS(-0)	ACOS(+0)	ACOS(-1)	ACOS(2)	ACOS(-2)	ACOS(9223372036854775807)	ACOS(-9223372036854775808)	ACOS(18446744073709551615)	ACOS(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	0	1.5707963267948966	0	1.5707963267948966	1.5707963267948966	1.5707963267948966	3.141592653589793	NULL	NULL	NULL	NULL	NULL	NULL	0	0" \
    "DO 0; SELECT ACOS(NULL),ACOS(TRUE),ACOS(FALSE),ACOS(1),ACOS(0),
            ACOS(-0),ACOS(+0),ACOS(-1),ACOS(2),ACOS(-2),
            ACOS(9223372036854775807),ACOS(-9223372036854775808),
            ACOS(18446744073709551615),ACOS(-18446744073709551615),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "core asin values" \
    "ASIN(NULL)	ASIN(TRUE)	ASIN(FALSE)	ASIN(1)	ASIN(0)	ASIN(-0)	ASIN(+0)	ASIN(-1)	ASIN(2)	ASIN(-2)	ASIN(9223372036854775807)	ASIN(-9223372036854775808)	ASIN(18446744073709551615)	ASIN(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	1.5707963267948966	0	1.5707963267948966	0	0	0	-1.5707963267948966	NULL	NULL	NULL	NULL	NULL	NULL	0	0" \
    "DO 0; SELECT ASIN(NULL),ASIN(TRUE),ASIN(FALSE),ASIN(1),ASIN(0),
            ASIN(-0),ASIN(+0),ASIN(-1),ASIN(2),ASIN(-2),
            ASIN(9223372036854775807),ASIN(-9223372036854775808),
            ASIN(18446744073709551615),ASIN(-18446744073709551615),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "child operand values" \
    "ACOS(5&3)	ACOS(~0)	ACOS(1<<63)	ACOS(1<<64)	ACOS(5 DIV 2)	ACOS(IFNULL(NULL,1))	ACOS(NULLIF(1,1))	ASIN(5&3)	ASIN(~0)	ASIN(1<<63)	ASIN(1<<64)	ASIN(5 DIV 2)	ASIN(IFNULL(NULL,1))	ASIN(NULLIF(1,1))
0	NULL	NULL	1.5707963267948966	NULL	0	NULL	1.5707963267948966	NULL	NULL	0	NULL	1.5707963267948966	NULL" \
    "SELECT ACOS(5&3),ACOS(~0),ACOS(1<<63),ACOS(1<<64),
            ACOS(5 DIV 2),ACOS(IFNULL(NULL,1)),ACOS(NULLIF(1,1)),
            ASIN(5&3),ASIN(~0),ASIN(1<<63),ASIN(1<<64),
            ASIN(5 DIV 2),ASIN(IFNULL(NULL,1)),ASIN(NULLIF(1,1));" \
    "$DATABASE"

expect_output_with_headers \
    "dual inverse trig values" \
    "arc_cos	arc_sin
0	1.5707963267948966" \
    "SELECT ACOS(1) AS arc_cos,ASIN(1) AS arc_sin FROM DUAL;" \
    "$DATABASE"

expect_output \
    "successful no warnings" \
    "0	1.5707963267948966
0	0	-1" \
    "SELECT ACOS(1),ASIN(1); SHOW WARNINGS; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
2	0	-1" \
    "DO 0; SELECT ACOS(5 DIV 0),ASIN(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do conversion status" \
    "0	0" \
    "DO ACOS(NULL),ACOS(1),ASIN(NULL),ASIN(1); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO ACOS(5 DIV 0),ASIN(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty acos arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ACOS'" \
    "SELECT ACOS();" \
    "$DATABASE"

expect_error \
    "extra acos arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ACOS'" \
    "DO ACOS(1,2);" \
    "$DATABASE"

expect_error \
    "empty asin arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ASIN'" \
    "SELECT ASIN();" \
    "$DATABASE"

expect_error \
    "extra asin arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ASIN'" \
    "DO ASIN(1,2);" \
    "$DATABASE"

expect_error \
    "bare acos identifier" \
    1054 \
    42S22 \
    "Unknown column 'ACOS' in 'field list'" \
    "SELECT ACOS;" \
    "$DATABASE"

expect_error \
    "bare asin identifier" \
    1054 \
    42S22 \
    "Unknown column 'ASIN' in 'field list'" \
    "SELECT ASIN;" \
    "$DATABASE"

expect_error \
    "child overflow under acos" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ACOS(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "child overflow under asin" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ASIN(3037000500*3037000500);" \
    "$DATABASE"

expect_output_force \
    "select warning before later error" \
    "Warning	1365	Division by 0
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     SELECT ACOS(5 DIV 0),ASIN(3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "do warning before later error" \
    "Warning	1365	Division by 0
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     DO ACOS(5 DIV 0),ASIN(3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "DO 0;
     SELECT ACOS('1'),ACOS('foo'),ACOS(_binary '1'),ACOS(X'10'),ACOS(b'1'),
            ACOS(0.5),ACOS(1e0),ACOS(18446744073709551616),
            ACOS(@@warning_count);
     DO 0;
     SELECT ASIN('1'),ASIN('foo'),ASIN(_binary '1'),ASIN(X'10'),ASIN(b'1'),
            ASIN(0.5),ASIN(1e0),ASIN(18446744073709551616),
            ASIN(@@warning_count);
     SELECT id,ACOS(id),ASIN(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
"ACOS('1')	ACOS('foo')	ACOS(_binary '1')	ACOS(X'10')	ACOS(b'1')	ACOS(0.5)	ACOS(1e0)	ACOS(18446744073709551616)	ACOS(@@warning_count)
0	1.5707963267948966	0	NULL	0	1.0471975511965979	0	NULL	1.5707963267948966
ASIN('1')	ASIN('foo')	ASIN(_binary '1')	ASIN(X'10')	ASIN(b'1')	ASIN(0.5)	ASIN(1e0)	ASIN(18446744073709551616)	ASIN(@@warning_count)
1.5707963267948966	0	1.5707963267948966	NULL	1.5707963267948966	0.5235987755982989	1.5707963267948966	NULL	0
id	ACOS(id)	ASIN(id)
-1	3.141592653589793	-1.5707963267948966
0	1.5707963267948966	0
1	0	1.5707963267948966
2	NULL	NULL
NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_acos_asin_functions_expectations: ok"
