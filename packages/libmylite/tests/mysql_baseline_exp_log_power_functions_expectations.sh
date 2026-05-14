#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_exp_log_power_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_exp_log_power_functions_expectations: $1" >&2
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
    "core exp values" \
    "EXP(NULL)	EXP(TRUE)	EXP(FALSE)	EXP(0)	EXP(1)	EXP(-1)	EXP(2)	EXP(10)	EXP(709)	@@warning_count	ROW_COUNT()
NULL	2.718281828459045	1	1	2.718281828459045	0.36787944117144233	7.38905609893065	22026.465794806718	8.218407461554972e307	0	0" \
    "DO 0;
     SELECT EXP(NULL),EXP(TRUE),EXP(FALSE),EXP(0),EXP(1),EXP(-1),EXP(2),
            EXP(10),EXP(709),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "log values and warnings" \
    "LN(NULL)	LN(TRUE)	LN(FALSE)	LN(0)	LN(1)	LN(-1)	LN(2)	LN(10)	LOG(NULL)	LOG(0)	LOG(1)	LOG(-1)	LOG(2)	LOG10(100)	LOG2(8)	@@warning_count	ROW_COUNT()
NULL	0	NULL	NULL	0	NULL	0.6931471805599453	2.302585092994046	NULL	NULL	0	NULL	0.6931471805599453	2	3	0	0
Level	Code	Message
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm" \
    "DO 0;
     SELECT LN(NULL),LN(TRUE),LN(FALSE),LN(0),LN(1),LN(-1),LN(2),LN(10),
            LOG(NULL),LOG(0),LOG(1),LOG(-1),LOG(2),LOG10(100),LOG2(8),
            @@warning_count,ROW_COUNT();
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output_with_headers \
    "two argument log values and warnings" \
    "LOG(10,100)	LOG(2,8)	LOG(1,8)	LOG(0,8)	LOG(-1,8)	LOG(2,0)	LOG(2,-8)	LOG(NULL,8)	LOG(2,NULL)	@@warning_count	ROW_COUNT()
2	3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	0	0
Level	Code	Message
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm
Warning	3020	Invalid argument for logarithm" \
    "DO 0;
     SELECT LOG(10,100),LOG(2,8),LOG(1,8),LOG(0,8),LOG(-1,8),
            LOG(2,0),LOG(2,-8),LOG(NULL,8),LOG(2,NULL),
            @@warning_count,ROW_COUNT();
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output_with_headers \
    "power values" \
    "POW(NULL,2)	POW(2,NULL)	POW(2,0)	POW(2,3)	POW(2,-3)	POW(-2,3)	POW(-2,2)	POW(-2,-3)	POWER(3,2)	POW(0,0)	@@warning_count	ROW_COUNT()
NULL	NULL	1	8	0.125	-8	4	-0.125	9	1	0	0" \
    "DO 0;
     SELECT POW(NULL,2),POW(2,NULL),POW(2,0),POW(2,3),POW(2,-3),
            POW(-2,3),POW(-2,2),POW(-2,-3),POWER(3,2),POW(0,0),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "child operand values" \
    "EXP(5&3)	LN(5&3)	LOG2(1<<3)	LOG(2,1<<3)	POW(5&3,2)	POW(2,5 DIV 2)	POW(2,1<<3)	@@warning_count	ROW_COUNT()
2.718281828459045	0	3	3	1	4	256	0	0" \
    "DO 0;
     SELECT EXP(5&3),LN(5&3),LOG2(1<<3),LOG(2,1<<3),POW(5&3,2),
            POW(2,5 DIV 2),POW(2,1<<3),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "child division warnings" \
    "NULL	NULL	NULL	NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
5	-1" \
    "DO 0;
     SELECT EXP(5 DIV 0),LN(5 DIV 0),LOG(2,5 DIV 0),
            POW(5 DIV 0,2),POW(2,5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do status and log warnings" \
    "Warning	3020	Invalid argument for logarithm
1	-1" \
    "DO EXP(0),LN(0),LOG(2,8),POW(2,3);
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "exp overflow" \
    1690 \
    22003 \
    "DOUBLE value is out of range in 'exp(710)'" \
    "SELECT EXP(710);" \
    "$DATABASE"

expect_error \
    "pow zero negative overflow" \
    1690 \
    22003 \
    "DOUBLE value is out of range in 'pow(0,-(1))'" \
    "SELECT POW(0,-1);" \
    "$DATABASE"

expect_error \
    "pow large overflow" \
    1690 \
    22003 \
    "DOUBLE value is out of range in 'pow(10,309)'" \
    "SELECT POW(10,309);" \
    "$DATABASE"

expect_output_force \
    "warning before exp overflow" \
    "Warning	1365	Division by 0
Error	1690	DOUBLE value is out of range in 'exp(710)'
2	1	-1" \
    "DO 0;
     SELECT EXP(5 DIV 0),EXP(710);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty exp arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'EXP'" \
    "SELECT EXP();" \
    "$DATABASE"

expect_error \
    "extra exp arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'EXP'" \
    "SELECT EXP(1,2);" \
    "$DATABASE"

expect_error \
    "empty ln arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LN'" \
    "SELECT LN();" \
    "$DATABASE"

expect_error \
    "empty log syntax" \
    1064 \
    42000 \
    "near ')'" \
    "SELECT LOG();" \
    "$DATABASE"

expect_error \
    "extra log syntax" \
    1064 \
    42000 \
    "near ',3)'" \
    "SELECT LOG(1,2,3);" \
    "$DATABASE"

expect_error \
    "empty log10 arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LOG10'" \
    "SELECT LOG10();" \
    "$DATABASE"

expect_error \
    "extra log2 arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LOG2'" \
    "SELECT LOG2(1,2);" \
    "$DATABASE"

expect_error \
    "missing pow arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'POW'" \
    "SELECT POW(2);" \
    "$DATABASE"

expect_error \
    "extra power arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'POWER'" \
    "SELECT POWER(2,3,4);" \
    "$DATABASE"

expect_error \
    "bare exp identifier" \
    1054 \
    42S22 \
    "Unknown column 'EXP' in 'field list'" \
    "SELECT EXP;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "DO 0;
     SELECT EXP('2'),LN('2'),LOG('2'),LOG(2,'8'),LOG10('100'),LOG2('8'),
            POW('2','3'),POW(5.5,2),POW(2,0.5);
     SELECT id,EXP(id),LN(id),POW(id,2) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
"EXP('2')	LN('2')	LOG('2')	LOG(2,'8')	LOG10('100')	LOG2('8')	POW('2','3')	POW(5.5,2)	POW(2,0.5)
7.38905609893065	0.6931471805599453	0.6931471805599453	3	2	3	8	30.25	1.4142135623730951
id	EXP(id)	LN(id)	POW(id,2)
-1	0.36787944117144233	NULL	1
0	1	NULL	0
1	2.718281828459045	0	1
2	7.38905609893065	0.6931471805599453	4
NULL	NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_exp_log_power_functions_expectations: ok"
