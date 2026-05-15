#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_trigonometric_expectations_$$"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_trigonometric_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    {
        printf "SET SESSION sql_mode = '%s';\n" "$DEFAULT_SQL_MODE"
        printf '%s\n' "$sql"
    } | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
        --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_force() {
    sql=$1
    shift
    {
        printf "SET SESSION sql_mode = '%s';\n" "$DEFAULT_SQL_MODE"
        printf '%s\n' "$sql"
    } | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
        --batch --raw --binary-as-hex=1 --skip-column-names --force "$@" 2>/dev/null
}

run_mysql_with_headers() {
    sql=$1
    shift
    {
        printf "SET SESSION sql_mode = '%s';\n" "$DEFAULT_SQL_MODE"
        printf '%s\n' "$sql"
    } | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
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
    "core sin cos tan values" \
    "SIN(NULL)	SIN(TRUE)	SIN(FALSE)	SIN(1)	SIN(0)	SIN(-0)	SIN(+0)	SIN(-1)	SIN(2)	SIN(9223372036854775807)	SIN(-9223372036854775808)	SIN(18446744073709551615)	SIN(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	0.8414709848078965	0	0.8414709848078965	0	0	0	-0.8414709848078965	0.9092974268256817	0.9999303766734422	-0.9999303766734422	0.023598509904439558	-0.023598509904439558	0	0
COS(NULL)	COS(TRUE)	COS(FALSE)	COS(1)	COS(0)	COS(-0)	COS(+0)	COS(-1)	COS(2)	COS(9223372036854775807)	COS(-9223372036854775808)	COS(18446744073709551615)	COS(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	0.5403023058681398	1	0.5403023058681398	1	1	1	0.5403023058681398	-0.4161468365471424	0.011800076512800236	0.011800076512800236	-0.9997215163885841	-0.9997215163885841	0	-1
TAN(NULL)	TAN(TRUE)	TAN(FALSE)	TAN(1)	TAN(0)	TAN(-0)	TAN(+0)	TAN(-1)	TAN(2)	TAN(9223372036854775807)	TAN(-9223372036854775808)	TAN(18446744073709551615)	TAN(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	1.5574077246549023	0	1.5574077246549023	0	0	0	-1.5574077246549023	-2.185039863261519	84.73931296875567	-84.73931296875567	-0.0236050835333497	0.0236050835333497	0	-1" \
    "DO 0;
     SELECT SIN(NULL),SIN(TRUE),SIN(FALSE),SIN(1),SIN(0),SIN(-0),SIN(+0),SIN(-1),SIN(2),
            SIN(9223372036854775807),SIN(-9223372036854775808),
            SIN(18446744073709551615),SIN(-18446744073709551615),
            @@warning_count,ROW_COUNT();
     SELECT COS(NULL),COS(TRUE),COS(FALSE),COS(1),COS(0),COS(-0),COS(+0),COS(-1),COS(2),
            COS(9223372036854775807),COS(-9223372036854775808),
            COS(18446744073709551615),COS(-18446744073709551615),
            @@warning_count,ROW_COUNT();
     SELECT TAN(NULL),TAN(TRUE),TAN(FALSE),TAN(1),TAN(0),TAN(-0),TAN(+0),TAN(-1),TAN(2),
            TAN(9223372036854775807),TAN(-9223372036854775808),
            TAN(18446744073709551615),TAN(-18446744073709551615),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "core cot nonzero values" \
    "COT(NULL)	COT(TRUE)	COT(1)	COT(-1)	COT(2)	COT(9223372036854775807)	COT(-9223372036854775808)	COT(18446744073709551615)	COT(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	0.6420926159343306	0.6420926159343306	-0.6420926159343306	-0.45765755436028577	0.011800898130584457	-0.011800898130584457	-42.36375603531254	42.36375603531254	0	0" \
    "DO 0;
     SELECT COT(NULL),COT(TRUE),COT(1),COT(-1),COT(2),
            COT(9223372036854775807),COT(-9223372036854775808),
            COT(18446744073709551615),COT(-18446744073709551615),
            @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "child operand values" \
    "SIN(5&3)	SIN(~0)	SIN(1<<63)	SIN(1<<64)	SIN(5 DIV 2)	SIN(IFNULL(NULL,1))	SIN(NULLIF(1,1))	COS(5&3)	COS(~0)	COS(1<<63)	COS(1<<64)	COS(5 DIV 2)	COS(IFNULL(NULL,1))	COS(NULLIF(1,1))	TAN(5&3)	TAN(~0)	TAN(1<<63)	TAN(1<<64)	TAN(5 DIV 2)	TAN(IFNULL(NULL,1))	TAN(NULLIF(1,1))	COT(5&3)	COT(~0)	COT(1<<63)	COT(5 DIV 2)	COT(IFNULL(NULL,1))	COT(NULLIF(1,1))
0.8414709848078965	0.023598509904439558	0.9999303766734422	0	0.9092974268256817	0.8414709848078965	NULL	0.5403023058681398	-0.9997215163885841	0.011800076512800236	1	-0.4161468365471424	0.5403023058681398	NULL	1.5574077246549023	-0.0236050835333497	84.73931296875567	0	-2.185039863261519	1.5574077246549023	NULL	0.6420926159343306	-42.36375603531254	0.011800898130584457	-0.45765755436028577	0.6420926159343306	NULL" \
    "SELECT SIN(5&3),SIN(~0),SIN(1<<63),SIN(1<<64),SIN(5 DIV 2),
            SIN(IFNULL(NULL,1)),SIN(NULLIF(1,1)),
            COS(5&3),COS(~0),COS(1<<63),COS(1<<64),COS(5 DIV 2),
            COS(IFNULL(NULL,1)),COS(NULLIF(1,1)),
            TAN(5&3),TAN(~0),TAN(1<<63),TAN(1<<64),TAN(5 DIV 2),
            TAN(IFNULL(NULL,1)),TAN(NULLIF(1,1)),
            COT(5&3),COT(~0),COT(1<<63),COT(5 DIV 2),
            COT(IFNULL(NULL,1)),COT(NULLIF(1,1));" \
    "$DATABASE"

expect_output_force \
    "cot zero error" \
    "Error	1690	DOUBLE value is out of range in 'cot(0)'
1	1	-1" \
    "SELECT COT(0); SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "cot false error" \
    "Error	1690	DOUBLE value is out of range in 'cot(false)'
1	1	-1" \
    "SELECT COT(FALSE); SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
3	0	-1" \
    "DO 0;
     SELECT SIN(5 DIV 0),COS(5 DIV 0),TAN(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "cot child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	0	-1" \
    "DO 0;
     SELECT COT(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warnings" \
    "Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
4	-1" \
    "DO SIN(5 DIV 0),COS(5 DIV 0),TAN(5 DIV 0),COT(5 DIV 0);
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_force \
    "warning before cot error" \
    "Warning	1365	Division by 0
Error	1690	DOUBLE value is out of range in 'cot(0)'
2	1	-1" \
    "DO 0;
     SELECT SIN(5 DIV 0),COT(0);
     SHOW WARNINGS; SELECT @@warning_count,@@error_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty sin arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SIN'" \
    "SELECT SIN();" \
    "$DATABASE"

expect_error \
    "extra cos arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'COS'" \
    "DO COS(1,2);" \
    "$DATABASE"

expect_error \
    "empty tan arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'TAN'" \
    "SELECT TAN();" \
    "$DATABASE"

expect_error \
    "extra cot arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'COT'" \
    "DO COT(1,2);" \
    "$DATABASE"

expect_error \
    "bare sin identifier" \
    1054 \
    42S22 \
    "Unknown column 'SIN' in 'field list'" \
    "SELECT SIN;" \
    "$DATABASE"

expect_error \
    "bare cot identifier" \
    1054 \
    42S22 \
    "Unknown column 'COT' in 'field list'" \
    "SELECT COT;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT SIN('1'),SIN('foo'),SIN(_binary '1'),SIN(X'10'),SIN(b'1'),
            SIN(0.5),SIN(1e0),SIN(18446744073709551616),SIN(@@warning_count);
     SELECT COS('1'),COS('foo'),COS(_binary '1'),COS(X'10'),COS(b'1'),
            COS(0.5),COS(1e0),COS(18446744073709551616),COS(@@warning_count);
     SELECT TAN('1'),TAN('foo'),TAN(_binary '1'),TAN(X'10'),TAN(b'1'),
            TAN(0.5),TAN(1e0),TAN(18446744073709551616),TAN(@@warning_count);
     SELECT id,SIN(id),COS(id),TAN(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
"SIN('1')	SIN('foo')	SIN(_binary '1')	SIN(X'10')	SIN(b'1')	SIN(0.5)	SIN(1e0)	SIN(18446744073709551616)	SIN(@@warning_count)
0.8414709848078965	0	0.8414709848078965	-0.2879033166650653	0.8414709848078965	0.479425538604203	0.8414709848078965	0.023598509904439558	0
COS('1')	COS('foo')	COS(_binary '1')	COS(X'10')	COS(b'1')	COS(0.5)	COS(1e0)	COS(18446744073709551616)	COS(@@warning_count)
0.5403023058681398	1	0.5403023058681398	-0.9576594803233847	0.5403023058681398	0.8775825618903728	0.5403023058681398	-0.9997215163885841	0.5403023058681398
TAN('1')	TAN('foo')	TAN(_binary '1')	TAN(X'10')	TAN(b'1')	TAN(0.5)	TAN(1e0)	TAN(18446744073709551616)	TAN(@@warning_count)
1.5574077246549023	0	1.5574077246549023	0.3006322420239034	1.5574077246549023	0.5463024898437905	1.5574077246549023	-0.0236050835333497	1.5574077246549023
id	SIN(id)	COS(id)	TAN(id)
-1	-0.8414709848078965	0.5403023058681398	-1.5574077246549023
0	0	1	0
1	0.8414709848078965	0.5403023058681398	1.5574077246549023
2	0.9092974268256817	-0.4161468365471424	-2.185039863261519
NULL	NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_trigonometric_functions_expectations: ok"
