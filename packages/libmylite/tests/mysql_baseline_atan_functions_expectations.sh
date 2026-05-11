#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_atan_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_atan_functions_expectations: $1" >&2
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
    "core one-argument atan values" \
    "ATAN(NULL)	ATAN2(NULL)	ATAN(TRUE)	ATAN2(TRUE)	ATAN(FALSE)	ATAN2(FALSE)	ATAN(1)	ATAN2(1)	ATAN(0)	ATAN(-0)	ATAN(+0)	ATAN(-1)	ATAN2(-1)	ATAN(2)	ATAN(-2)	ATAN(9223372036854775807)	ATAN(-9223372036854775808)	ATAN(18446744073709551615)	ATAN(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	NULL	0.7853981633974483	0.7853981633974483	0	0	0.7853981633974483	0.7853981633974483	0	0	0	-0.7853981633974483	-0.7853981633974483	1.1071487177940904	-1.1071487177940904	1.5707963267948966	-1.5707963267948966	1.5707963267948966	-1.5707963267948966	0	0" \
    "DO 0; SELECT ATAN(NULL),ATAN2(NULL),ATAN(TRUE),ATAN2(TRUE),ATAN(FALSE),
            ATAN2(FALSE),ATAN(1),ATAN2(1),ATAN(0),ATAN(-0),ATAN(+0),
            ATAN(-1),ATAN2(-1),ATAN(2),ATAN(-2),
            ATAN(9223372036854775807),ATAN(-9223372036854775808),
            ATAN(18446744073709551615),ATAN(-18446744073709551615),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "core two-argument atan values" \
    "ATAN(NULL,1)	ATAN(1,NULL)	ATAN(TRUE,FALSE)	ATAN(FALSE,TRUE)	ATAN(1,0)	ATAN(-1,0)	ATAN(0,1)	ATAN(0,-1)	ATAN(1,1)	ATAN(-1,1)	ATAN(1,-1)	ATAN(-1,-1)	ATAN(0,0)	ATAN2(1,0)	ATAN2(1,-1)	@@warning_count	ROW_COUNT()
NULL	NULL	1.5707963267948966	0	1.5707963267948966	-1.5707963267948966	0	3.141592653589793	0.7853981633974483	-0.7853981633974483	2.356194490192345	-2.356194490192345	0	1.5707963267948966	2.356194490192345	0	0" \
    "DO 0; SELECT ATAN(NULL,1),ATAN(1,NULL),ATAN(TRUE,FALSE),ATAN(FALSE,TRUE),
            ATAN(1,0),ATAN(-1,0),ATAN(0,1),ATAN(0,-1),ATAN(1,1),
            ATAN(-1,1),ATAN(1,-1),ATAN(-1,-1),ATAN(0,0),ATAN2(1,0),
            ATAN2(1,-1),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "child operand values" \
    "ATAN(5&3)	ATAN(~0)	ATAN(1<<63)	ATAN(1<<64)	ATAN(5 DIV 2)	ATAN(IFNULL(NULL,1))	ATAN(NULLIF(1,1))	ATAN(1,~0)	ATAN(~0,1)	ATAN(1,1<<64)	ATAN(1<<64,1)	ATAN(1,5 DIV 2)	ATAN2(1,~0)	ATAN2(~0,1)
0.7853981633974483	1.5707963267948966	1.5707963267948966	0	1.1071487177940904	0.7853981633974483	NULL	5.421010862427522e-20	1.5707963267948966	1.5707963267948966	0	0.4636476090008061	5.421010862427522e-20	1.5707963267948966" \
    "SELECT ATAN(5&3),ATAN(~0),ATAN(1<<63),ATAN(1<<64),ATAN(5 DIV 2),
            ATAN(IFNULL(NULL,1)),ATAN(NULLIF(1,1)),ATAN(1,~0),ATAN(~0,1),
            ATAN(1,1<<64),ATAN(1<<64,1),ATAN(1,5 DIV 2),ATAN2(1,~0),
            ATAN2(~0,1);" \
    "$DATABASE"

expect_output_with_headers \
    "dual atan values" \
    "arc_tan	arc_tan2
0.7853981633974483	2.356194490192345" \
    "SELECT ATAN(1) AS arc_tan,ATAN2(1,-1) AS arc_tan2 FROM DUAL;" \
    "$DATABASE"

expect_output \
    "two-argument first-null short circuit" \
    "NULL	NULL	NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
2	0	-1" \
    "DO 0; SELECT ATAN(NULL,5 DIV 0),ATAN(5 DIV 0,NULL),
            ATAN2(NULL,5 DIV 0),ATAN2(5 DIV 0,NULL),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	NULL	NULL	NULL	NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
6	0	-1" \
    "DO 0; SELECT ATAN(5 DIV 0),ATAN(5 DIV 0,1),ATAN(1,5 DIV 0),
            ATAN2(5 DIV 0),ATAN2(5 DIV 0,1),ATAN2(1,5 DIV 0),
            @@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do conversion status" \
    "0	0" \
    "DO ATAN(NULL),ATAN(1),ATAN2(NULL),ATAN2(1),ATAN(1,-1),ATAN2(1,-1);
     SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
6	-1" \
    "DO ATAN(5 DIV 0),ATAN(5 DIV 0,1),ATAN(1,5 DIV 0),
        ATAN2(5 DIV 0),ATAN2(5 DIV 0,1),ATAN2(1,5 DIV 0);
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty atan arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ATAN'" \
    "SELECT ATAN();" \
    "$DATABASE"

expect_error \
    "extra atan arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ATAN'" \
    "DO ATAN(1,2,3);" \
    "$DATABASE"

expect_error \
    "empty atan2 arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ATAN2'" \
    "SELECT ATAN2();" \
    "$DATABASE"

expect_error \
    "extra atan2 arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ATAN2'" \
    "DO ATAN2(1,2,3);" \
    "$DATABASE"

expect_error \
    "bare atan identifier" \
    1054 \
    42S22 \
    "Unknown column 'ATAN' in 'field list'" \
    "SELECT ATAN;" \
    "$DATABASE"

expect_error \
    "bare atan2 identifier" \
    1054 \
    42S22 \
    "Unknown column 'ATAN2' in 'field list'" \
    "SELECT ATAN2;" \
    "$DATABASE"

expect_error \
    "child overflow under atan" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ATAN(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "second child overflow under atan" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ATAN(1,3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "child overflow under atan2" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ATAN2(3037000500*3037000500,1);" \
    "$DATABASE"

expect_output_force \
    "select warning before later error" \
    "Warning	1365	Division by 0
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     SELECT ATAN(5 DIV 0),ATAN(3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "two-argument first-null skips second overflow" \
    "NULL
Warning	1365	Division by 0
1	0	-1" \
    "DO 0;
     SELECT ATAN(5 DIV 0,3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred forms" \
    "0.7853981633974483	0	0.7853981633974483	1.5083775167989393	0.7853981633974483	0.4636476090008061	0.7853981633974483	1.5707963267948966	0
Warning	1292	Truncated incorrect DOUBLE value: 'foo'" \
    "DO 0; SELECT ATAN('1'),ATAN('foo'),ATAN(_binary '1'),ATAN(X'10'),ATAN(b'1'),
            ATAN(0.5),ATAN(1e0),ATAN(18446744073709551616),ATAN(@@warning_count);
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred two-argument forms" \
    "1.5707963267948966	0	1.5707963267948966	0	0.4636476090008061	0.4636476090008061	0.7853981633974483	0.7853981633974483	1.5707963267948966	1.5707963267948966
Warning	1292	Truncated incorrect DOUBLE value: 'foo'
Warning	1292	Truncated incorrect DOUBLE value: 'foo'
Warning	1292	Truncated incorrect DOUBLE value: 'foo'
Warning	1292	Truncated incorrect DOUBLE value: 'foo'" \
    "DO 0; SELECT ATAN(1,'foo'),ATAN('foo',1),ATAN2(1,'foo'),ATAN2('foo',1),
            ATAN(0.5,1),ATAN2(0.5,1),ATAN(1e0,1),ATAN2(1e0,1),
            ATAN(18446744073709551616,1),ATAN2(18446744073709551616,1);
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output_with_headers \
    "table-backed accepted by mysql but deferred" \
    "id	ATAN(id)	ATAN(id,1)	ATAN2(id,1)
NULL	NULL	NULL	NULL
-1	-0.7853981633974483	-0.7853981633974483	-0.7853981633974483
0	0	0	0
1	0.7853981633974483	0.7853981633974483	0.7853981633974483
2	1.1071487177940904	1.1071487177940904	1.1071487177940904" \
    "SELECT id,ATAN(id),ATAN(id,1),ATAN2(id,1) FROM t ORDER BY id;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_atan_functions_expectations: ok"
