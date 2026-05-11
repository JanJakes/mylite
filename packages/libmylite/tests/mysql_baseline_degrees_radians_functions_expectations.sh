#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_degrees_radians_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_degrees_radians_functions_expectations: $1" >&2
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
           INSERT INTO t VALUES (0), (1), (4), (NULL), (-1);" \
    >/dev/null

expect_output_with_headers \
    "core degrees values" \
    "DEGREES(NULL)	DEGREES(TRUE)	DEGREES(FALSE)	DEGREES(0)	DEGREES(-0)	DEGREES(+0)	DEGREES(1)	DEGREES(-1)	DEGREES(2)	DEGREES(3)	DEGREES(90)	DEGREES(180)	DEGREES(360)	@@warning_count	ROW_COUNT()
NULL	57.29577951308232	0	0	0	0	57.29577951308232	-57.29577951308232	114.59155902616465	171.88733853924697	5156.620156177409	10313.240312354817	20626.480624709635	0	0" \
    "DO 0; SELECT DEGREES(NULL),DEGREES(TRUE),DEGREES(FALSE),DEGREES(0),DEGREES(-0),
            DEGREES(+0),DEGREES(1),DEGREES(-1),DEGREES(2),DEGREES(3),
            DEGREES(90),DEGREES(180),DEGREES(360),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "core radians values" \
    "RADIANS(NULL)	RADIANS(TRUE)	RADIANS(FALSE)	RADIANS(0)	RADIANS(-0)	RADIANS(+0)	RADIANS(1)	RADIANS(-1)	RADIANS(2)	RADIANS(3)	RADIANS(90)	RADIANS(180)	RADIANS(360)	@@warning_count	ROW_COUNT()
NULL	0.017453292519943295	0	0	0	0	0.017453292519943295	-0.017453292519943295	0.03490658503988659	0.05235987755982989	1.5707963267948966	3.141592653589793	6.283185307179586	0	0" \
    "DO 0; SELECT RADIANS(NULL),RADIANS(TRUE),RADIANS(FALSE),RADIANS(0),RADIANS(-0),
            RADIANS(+0),RADIANS(1),RADIANS(-1),RADIANS(2),RADIANS(3),
            RADIANS(90),RADIANS(180),RADIANS(360),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "boundary degrees values" \
    "DEGREES(9223372036854775807)	DEGREES(9223372036854775808)	DEGREES(18446744073709551615)	DEGREES(-9223372036854775808)	DEGREES(-18446744073709551615)	@@warning_count	ROW_COUNT()
5.2846029059076024e20	5.2846029059076024e20	1.0569205811815205e21	-5.2846029059076024e20	-1.0569205811815205e21	0	0" \
    "DO 0; SELECT DEGREES(9223372036854775807),DEGREES(9223372036854775808),
            DEGREES(18446744073709551615),DEGREES(-9223372036854775808),
            DEGREES(-18446744073709551615),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "boundary radians values" \
    "RADIANS(9223372036854775807)	RADIANS(9223372036854775808)	RADIANS(18446744073709551615)	RADIANS(-9223372036854775808)	RADIANS(-18446744073709551615)	@@warning_count	ROW_COUNT()
1.6097821017949162e17	1.6097821017949162e17	3.2195642035898323e17	-1.6097821017949162e17	-3.2195642035898323e17	0	0" \
    "DO 0; SELECT RADIANS(9223372036854775807),RADIANS(9223372036854775808),
            RADIANS(18446744073709551615),RADIANS(-9223372036854775808),
            RADIANS(-18446744073709551615),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "child operand values" \
    "DEGREES(5&3)	DEGREES(~0)	DEGREES(1<<63)	DEGREES(1<<64)	DEGREES(5 DIV 2)	DEGREES(IFNULL(NULL,9))	DEGREES(NULLIF(1,1))	RADIANS(5&3)	RADIANS(~0)	RADIANS(1<<63)	RADIANS(1<<64)	RADIANS(5 DIV 2)	RADIANS(IFNULL(NULL,9))	RADIANS(NULLIF(1,1))
57.29577951308232	1.0569205811815205e21	5.2846029059076024e20	0	114.59155902616465	515.662015617741	NULL	0.017453292519943295	3.2195642035898323e17	1.6097821017949162e17	0	0.03490658503988659	0.15707963267948966	NULL" \
    "SELECT DEGREES(5&3),DEGREES(~0),DEGREES(1<<63),DEGREES(1<<64),
            DEGREES(5 DIV 2),DEGREES(IFNULL(NULL,9)),DEGREES(NULLIF(1,1)),
            RADIANS(5&3),RADIANS(~0),RADIANS(1<<63),RADIANS(1<<64),
            RADIANS(5 DIV 2),RADIANS(IFNULL(NULL,9)),RADIANS(NULLIF(1,1));" \
    "$DATABASE"

expect_output_with_headers \
    "dual conversion values" \
    "deg	rad
114.59155902616465	0.03490658503988659" \
    "SELECT DEGREES(2) AS deg,RADIANS(2) AS rad FROM DUAL;" \
    "$DATABASE"

expect_output \
    "successful no warnings" \
    "57.29577951308232	0.017453292519943295
0	0	-1" \
    "SELECT DEGREES(1),RADIANS(1); SHOW WARNINGS; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO 0; SELECT DEGREES(5 DIV 0),RADIANS(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do conversion status" \
    "0	0" \
    "DO DEGREES(NULL),DEGREES(2),RADIANS(NULL),RADIANS(2); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO DEGREES(5 DIV 0),RADIANS(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty degrees arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'DEGREES'" \
    "SELECT DEGREES();" \
    "$DATABASE"

expect_error \
    "extra degrees arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'DEGREES'" \
    "DO DEGREES(1,2);" \
    "$DATABASE"

expect_error \
    "empty radians arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'RADIANS'" \
    "SELECT RADIANS();" \
    "$DATABASE"

expect_error \
    "extra radians arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'RADIANS'" \
    "DO RADIANS(1,2);" \
    "$DATABASE"

expect_error \
    "bare degrees identifier" \
    1054 \
    42S22 \
    "Unknown column 'DEGREES' in 'field list'" \
    "SELECT DEGREES;" \
    "$DATABASE"

expect_error \
    "bare radians identifier" \
    1054 \
    42S22 \
    "Unknown column 'RADIANS' in 'field list'" \
    "SELECT RADIANS;" \
    "$DATABASE"

expect_error \
    "child overflow under degrees" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT DEGREES(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "child overflow under radians" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT RADIANS(3037000500*3037000500);" \
    "$DATABASE"

expect_output_force \
    "select warning before later error" \
    "Warning	1365	Division by 0
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     SELECT DEGREES(5 DIV 0),RADIANS(3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "do warning before later error" \
    "Warning	1365	Division by 0
Error	1690	BIGINT value is out of range in '(3037000500 * 3037000500)'
2	1	-1" \
    "DO 0;
     DO DEGREES(5 DIV 0),RADIANS(3037000500*3037000500);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "DO 0;
     SELECT DEGREES('2'),DEGREES(_binary '2'),DEGREES(X'10'),DEGREES(b'10'),
            DEGREES(2.5),DEGREES(1e1),DEGREES(18446744073709551616),
            DEGREES(999999999999999999999999999999999999999999999999999999999999999999999999999999999),
            DEGREES(@@warning_count);
     DO 0;
     SELECT RADIANS('2'),RADIANS(_binary '2'),RADIANS(X'10'),RADIANS(b'10'),
            RADIANS(2.5),RADIANS(1e1),RADIANS(18446744073709551616),
            RADIANS(999999999999999999999999999999999999999999999999999999999999999999999999999999999),
            RADIANS(@@warning_count);
     SELECT id,DEGREES(id),RADIANS(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
"DEGREES('2')	DEGREES(_binary '2')	DEGREES(X'10')	DEGREES(b'10')	DEGREES(2.5)	DEGREES(1e1)	DEGREES(18446744073709551616)	DEGREES(999999999999999999999999999999999999999999999999999999999999999999999999999999999)	DEGREES(@@warning_count)
114.59155902616465	114.59155902616465	916.7324722093172	114.59155902616465	143.2394487827058	572.9577951308232	1.0569205811815205e21	5.7295779513082316e82	0
RADIANS('2')	RADIANS(_binary '2')	RADIANS(X'10')	RADIANS(b'10')	RADIANS(2.5)	RADIANS(1e1)	RADIANS(18446744073709551616)	RADIANS(999999999999999999999999999999999999999999999999999999999999999999999999999999999)	RADIANS(@@warning_count)
0.03490658503988659	0.03490658503988659	0.2792526803190927	0.03490658503988659	0.04363323129985824	0.17453292519943295	3.2195642035898323e17	1.7453292519943295e79	0
id	DEGREES(id)	RADIANS(id)
-1	-57.29577951308232	-0.017453292519943295
0	0	0
1	57.29577951308232	0.017453292519943295
4	229.1831180523293	0.06981317007977318
NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_degrees_radians_functions_expectations: ok"
